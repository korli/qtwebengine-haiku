/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/skottie/src/animator/Keyframe.h"

#include "include/private/SkNx.h"
#include "modules/skottie/src/SkottieJson.h"
#include "modules/skottie/src/SkottieValue.h"
#include "modules/skottie/src/animator/Animator.h"
#include "modules/skottie/src/animator/Vector.h"
#include "src/core/SkSafeMath.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace skottie {

// Parses an array of exact size.
static bool parse_array(const skjson::ArrayValue* ja, float* a, size_t count) {
    if (!ja || ja->size() != count) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!Parse((*ja)[i], a + i)) {
            return false;
        }
    }

    return true;
}

template <>
bool ValueTraits<VectorValue>::FromJSON(const skjson::Value& jv, const internal::AnimationBuilder*,
                                        VectorValue* v) {
    if (const skjson::ArrayValue* ja = jv) {
        const auto size = ja->size();
        v->resize(size);
        return parse_array(ja, v->data(), size);
    }

    return false;
}

namespace internal {
namespace {

// Vector specialization - stores float vector values (of same length) in consolidated/contiguous
// storage.  Keyframe records hold the storage offset for each value:
//
// fStorage: [     vec0     ][     vec1     ] ... [     vecN     ]
//            <-  vec_len ->  <-  vec_len ->       <-  vec_len ->
//
//           ^               ^                    ^
// fKFs[]: .idx            .idx       ...       .idx
//
class VectorKeyframeAnimator final : public KeyframeAnimatorBase {
public:
    VectorKeyframeAnimator(std::vector<Keyframe> kfs,
                           std::vector<SkCubicMap> cms,
                           std::vector<float> storage,
                           size_t vec_len,
                           VectorValue* target_value)
        : INHERITED(std::move(kfs), std::move(cms))
        , fStorage(std::move(storage))
        , fVecLen(vec_len)
        , fTarget(target_value) {

        // Resize the target value appropriately.
        fTarget->resize(fVecLen);
    }

private:
    StateChanged onSeek(float t) override {
        const auto& lerp_info = this->getLERPInfo(t);

        SkASSERT(lerp_info.vrec0.idx + fVecLen <= fStorage.size());
        SkASSERT(lerp_info.vrec1.idx + fVecLen <= fStorage.size());
        SkASSERT(fTarget->size() == fVecLen);

        const auto* v0  = fStorage.data() + lerp_info.vrec0.idx;
        const auto* v1  = fStorage.data() + lerp_info.vrec1.idx;
              auto* dst = fTarget->data();

        if (lerp_info.isConstant()) {
            if (std::memcmp(dst, v0, fVecLen * sizeof(float))) {
                std::copy(v0, v0 + fVecLen, dst);
                return true;
            }
            return false;
        }

        size_t count = fVecLen;
        bool changed = false;

        while (count >= 4) {
            const auto old_val = Sk4f::Load(dst),
                       new_val = Lerp(Sk4f::Load(v0), Sk4f::Load(v1), lerp_info.weight);

            changed |= (new_val != old_val).anyTrue();
            new_val.store(dst);

            v0    += 4;
            v1    += 4;
            dst   += 4;
            count -= 4;
        }

        while (count-- > 0) {
            const auto new_val = Lerp(*v0++, *v1++, lerp_info.weight);

            changed |= (new_val != *dst);
            *dst++ = new_val;
        }

        return changed;
    }

    const std::vector<float> fStorage;
    const size_t             fVecLen;

    VectorValue*             fTarget;

    using INHERITED = KeyframeAnimatorBase;
};

} // namespace

VectorKeyframeAnimatorBuilder::VectorKeyframeAnimatorBuilder(VectorLenParser  parse_len,
                                                             VectorDataParser parse_data)
    : fParseLen(parse_len)
    , fParseData(parse_data) {}

sk_sp<KeyframeAnimatorBase> VectorKeyframeAnimatorBuilder::make(const AnimationBuilder& abuilder,
                                                                const skjson::ArrayValue& jkfs,
                                                                void* target_value) {
    SkASSERT(jkfs.size() > 0);

    // peek at the first keyframe value to find our vector length
    const skjson::ObjectValue* jkf0 = jkfs[0];
    if (!jkf0 || !fParseLen((*jkf0)["s"], &fVecLen)) {
        return nullptr;
    }

    SkSafeMath safe;
    // total elements: vector length x number vectors
    const auto total_size = safe.mul(fVecLen, jkfs.size());

    // we must be able to store all offsets in Keyframe::Value::idx (uint32_t)
    if (!safe || !SkTFitsIn<uint32_t>(total_size)) {
        return nullptr;
    }
    fStorage.resize(total_size);

    if (!this->parseKeyframes(abuilder, jkfs)) {
        return nullptr;
    }

    // parseKFValue() might have stored fewer vectors thanks to tail-deduping.
    SkASSERT(fCurrentVec <= jkfs.size());
    fStorage.resize(fCurrentVec * fVecLen);
    fStorage.shrink_to_fit();

    return sk_sp<VectorKeyframeAnimator>(
                new VectorKeyframeAnimator(std::move(fKFs),
                                           std::move(fCMs),
                                           std::move(fStorage),
                                           fVecLen,
                                           static_cast<VectorValue*>(target_value)));
}

bool VectorKeyframeAnimatorBuilder::parseValue(const AnimationBuilder&,
                                               const skjson::Value& jv,
                                               void* raw_v) const {
    size_t vec_len;
    if (!this->fParseLen(jv, &vec_len)) {
        return false;
    }

    auto* v = static_cast<VectorValue*>(raw_v);
    v->resize(vec_len);
    return fParseData(jv, vec_len, v->data());
}

bool VectorKeyframeAnimatorBuilder::parseKFValue(const AnimationBuilder&,
                                                 const skjson::ObjectValue&,
                                                 const skjson::Value& jv,
                                                 Keyframe::Value* kfv) {
    auto offset = fCurrentVec * fVecLen;
    SkASSERT(offset + fVecLen <= fStorage.size());

    if (!fParseData(jv, fVecLen, fStorage.data() + offset)) {
        return false;
    }

    SkASSERT(!fCurrentVec || offset >= fVecLen);
    // compare with previous vector value
    if (fCurrentVec > 0 && !memcmp(fStorage.data() + offset,
                                   fStorage.data() + offset - fVecLen,
                                   fVecLen * sizeof(float))) {
        // repeating value -> use prev offset (dedupe)
        offset -= fVecLen;
    } else {
        // new value -> advance the current index
        fCurrentVec += 1;
    }

    // Keyframes record the storage-offset for a given vector value.
    kfv->idx = SkToU32(offset);

    return true;
}

template <>
bool AnimatablePropertyContainer::bind<VectorValue>(const AnimationBuilder& abuilder,
                                                    const skjson::ObjectValue* jprop,
                                                    VectorValue* v) {
    if (!jprop) {
        return false;
    }

    if (!ParseDefault<bool>((*jprop)["s"], false)) {
        // Regular (static or keyframed) vector value.
        VectorKeyframeAnimatorBuilder builder(
                    // Len parser.
                    [](const skjson::Value& jv, size_t* len) -> bool {
                        if (const skjson::ArrayValue* ja = jv) {
                            *len = ja->size();
                            return true;
                        }
                        return false;
                    },
                    // Data parser.
                    [](const skjson::Value& jv, size_t len, float* data) {
                        return parse_array(jv, data, len);
                    });

        return this->bindImpl(abuilder, jprop, builder, v);
    }

    // Separate-dimensions vector value: each component is animated independently.
    *v = { 0, 0, 0 };
    return this->bind(abuilder, (*jprop)["x"], v->data() + 0)
         | this->bind(abuilder, (*jprop)["y"], v->data() + 1)
         | this->bind(abuilder, (*jprop)["z"], v->data() + 2);
}

} // namespace internal
} // namespace skottie
