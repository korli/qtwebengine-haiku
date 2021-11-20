// Copyright 2021 Gerasim Troeglazov. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/haiku/audio_manager_haiku.h"

#include <memory>

#include "media/audio/haiku/audio_output_stream_haiku.h"

namespace media {

AudioManagerHaiku::AudioManagerHaiku(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

AudioManagerHaiku::~AudioManagerHaiku() = default;

bool AudioManagerHaiku::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerHaiku::HasAudioInputDevices() {
  return true;
}

void AudioManagerHaiku::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  *device_names = {AudioDeviceName::CreateDefault()};
}

void AudioManagerHaiku::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  *device_names = {AudioDeviceName::CreateDefault()};
}

AudioParameters AudioManagerHaiku::GetInputStreamParameters(
    const std::string& device_id) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_MONO, 16000, 160);
  params.set_effects(AudioParameters::ECHO_CANCELLER |
                     AudioParameters::NOISE_SUPPRESSION |
                     AudioParameters::AUTOMATIC_GAIN_CONTROL);

  return params;
}

AudioParameters AudioManagerHaiku::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 480);
}

const char* AudioManagerHaiku::GetName() {
  return "Haiku";
}

AudioOutputStream* AudioManagerHaiku::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioOutputStream* AudioManagerHaiku::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (!device_id.empty() &&
      device_id != AudioDeviceDescription::kDefaultDeviceId) {
    return nullptr;
  }

  return new AudioOutputStreamHaiku(this, params);
}

AudioInputStream* AudioManagerHaiku::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* AudioManagerHaiku::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerHaiku>(std::move(audio_thread),
                                               audio_log_factory);
}

}  // namespace media
