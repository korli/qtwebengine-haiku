// Copyright 2021 Gerasim Troeglazov. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_HAIKU_AUDIO_OUTPUT_STREAM_HAIKU_H_
#define MEDIA_AUDIO_HAIKU_AUDIO_OUTPUT_STREAM_HAIKU_H_

#include <OS.h>
#include <Application.h>
#include <SoundPlayer.h>
#include <Roster.h>
#include <String.h>
#include <NodeInfo.h>
#include <Path.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerHaiku;

class AudioOutputStreamHaiku : public AudioOutputStream {
 public:
  // Caller must ensure that manager outlives the stream.
  AudioOutputStreamHaiku(AudioManagerHaiku* manager,
                           const AudioParameters& parameters);

  // AudioOutputStream interface.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void Flush() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;

 private:
  ~AudioOutputStreamHaiku() override;

  AudioManagerHaiku* manager_;
  AudioParameters parameters_;

  BSoundPlayer *player_;
  BString process_name_;

  std::unique_ptr<AudioBus> audio_bus_;

  void AudioCallback(void *stream, size_t len) noexcept;
  static void audio_callback(void *cookie, void *buffer, size_t len, const media_raw_audio_format &) noexcept {
       static_cast<AudioOutputStreamHaiku*>(cookie)->AudioCallback(buffer, len);
  }

  AudioSourceCallback* callback_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputStreamHaiku);
};

}  // namespace media

#endif  // MEDIA_AUDIO_HAIKU_AUDIO_OUTPUT_STREAM_HAIKU_H_
