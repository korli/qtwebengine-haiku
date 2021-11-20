// Copyright 2021 Gerasim Troeglazov. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/haiku/audio_output_stream_haiku.h"

#include "base/bind.h"
#include "base/fuchsia/default_context.h"
#include "base/memory/writable_shared_memory_region.h"
#include "media/audio/haiku/audio_manager_haiku.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

AudioOutputStreamHaiku::AudioOutputStreamHaiku(
    AudioManagerHaiku* manager,
    const AudioParameters& parameters)
    : manager_(manager),
      parameters_(parameters),
      player_(NULL),
      audio_bus_(AudioBus::Create(parameters)) {
  process_name_.SetTo("QtWebEngine");
  app_info appInfo;
  if (be_app->GetAppInfo(&appInfo) == B_OK) {
      BPath path(&appInfo.ref);
      process_name_.SetTo(path.Leaf());
  }
}

AudioOutputStreamHaiku::~AudioOutputStreamHaiku() {}

bool AudioOutputStreamHaiku::Open() {
  media_raw_audio_format format;
  format = {
      parameters_.sample_rate(),
      parameters_.channels(),
      media_raw_audio_format::B_AUDIO_FLOAT,
      B_MEDIA_LITTLE_ENDIAN,
      parameters_.GetBytesPerBuffer(kSampleFormatF32)
  };

  player_ = new BSoundPlayer(&format, process_name_.String(), audio_callback,
      NULL, static_cast<void*>(this));

  return true;
}

void AudioOutputStreamHaiku::Start(AudioSourceCallback* callback) {
  DCHECK(!callback_);
  callback_ = callback;

  player_->Start();
  player_->SetHasData(true);
}

void AudioOutputStreamHaiku::Stop() {
  callback_ = nullptr;

  player_->SetHasData(false);
  player_->Stop();
}

void AudioOutputStreamHaiku::Flush() {}

void AudioOutputStreamHaiku::SetVolume(double volume) {
  DCHECK(0.0 <= volume && volume <= 1.0) << volume;
  player_->SetVolume(volume);
}

void AudioOutputStreamHaiku::GetVolume(double* volume) {
  *volume = player_->Volume();
}

void AudioOutputStreamHaiku::Close() {
  Stop();

  delete player_;

  manager_->ReleaseOutputStream(this);
}

void AudioOutputStreamHaiku::AudioCallback(void *stream, size_t len) noexcept
{
  int frames_filled = callback_->OnMoreData(base::TimeDelta::FromMicroseconds(0), base::TimeTicks::Now(), 0, audio_bus_.get());
  if (frames_filled <= 0) {
  	memset(stream, 0, len);
    return;
  }

  audio_bus_->ToInterleaved<Float32SampleTypeTraitsNoClip>(
      frames_filled, reinterpret_cast<float*>(stream));
}

}  // namespace media
