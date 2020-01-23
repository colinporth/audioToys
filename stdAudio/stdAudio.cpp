// This example app plays a short melody using a simple square wave synthesiser.
//{{{
// Copyright (c) 2018 - Timur Doumler
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
//}}}
//{{{  includes
#ifdef _WIN32
  #define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <array>
#include <thread>
#include <random>
#include "audio.h"

using namespace std;
using namespace audio;
//}}}

//{{{
constexpr array<int, 22> notes = {
  88, 86, 78, 78, 80, 80,
  85, 83, 74, 74, 76, 76,
  83, 81, 73, 73, 76, 76,
  81, 81, 81, 81
  };
//}}}

constexpr float bpm = 260.0;
//{{{
float note_to_frequency_hz (int note) {
  constexpr float pitch_standard_hz = 440.0f;
  return pitch_standard_hz * pow (2.0f, float (note - 69) / 12.0f);
  }
//}}}

//{{{
bool isDefaultDevice (const cAudioDevice& d) {

  if (d.isInput()) {
    auto default_in = getDefaultAudioInputDevice();
    return default_in.has_value() && d.getDeviceId() == default_in->getDeviceId();
    }
  else if (d.isOutput()) {
    auto default_out = getDefaultAudioOutputDevice();
    return default_out.has_value() && d.getDeviceId() == default_out->getDeviceId();
    }

  return false;
  }
//}}}
//{{{
void print_device_info (const cAudioDevice& d) {

  cout << "- \"" << d.getName() << "\", ";
  cout << "sample rate = " << d.getSampleRate() << " Hz, ";
  cout << "buffer size = " << d.getBufferSizeFrames() << " frames, ";
  cout << (d.isInput() ? d.getNumInputChannels() : d.getNumOutputChannels()) << " channels";
  cout << (isDefaultDevice(d) ? " [DEFAULT DEVICE]\n" : "\n");
  };
//}}}
//{{{
void print_device_list (const cAudioDeviceList& list) {

  for (auto& item : list)
    print_device_info (item);
  }
//}}}
//{{{
void print_all_devices() {

  cout << "Input devices:\n==============\n";
  print_device_list (getAudioInputDeviceList());

  cout << "\nOutput devices:\n===============\n";
  print_device_list (getAudioOutputDeviceList());
  }
//}}}

atomic<bool> stop = false;
//{{{
struct synthesiser {
  //{{{
  float get_next_sample() {

    assert (_sample_rate > 0);

    _ms_counter += 1000.0f / _sample_rate;
    if (_ms_counter >= _note_duration_ms) {
      _ms_counter = 0;
      if (++_current_note_index < notes.size()) {
        update();
        }
      else {
        stop.store(true);
        return 0;
        }
      }

    auto next_sample = copysign (0.1f, sin(_phase));
    _phase = fmod (_phase + _delta, 2.0f * float(M_PI));
    return next_sample;
    }
  //}}}
  //{{{
  void set_sample_rate (float sample_rate) {

    _sample_rate = sample_rate;
    update();
    }
  //}}}

private:
  //{{{
  void update() noexcept {
    float frequency_hz = note_to_frequency_hz(notes.at(_current_note_index));
    _delta = 2.0f * frequency_hz * static_cast<float>(M_PI / _sample_rate);
    }
  //}}}

  float _sample_rate = 0;
  float _delta = 0;
  float _phase = 0;
  float _ms_counter = 0;
  float _note_duration_ms = 60'000.0f / bpm;
  int _current_note_index = 0;
  };
//}}}

int main() {
  print_all_devices();

  setAudioDeviceListCallback(cAudioDeviceListEvent::eListChanged, [] {
    //{{{
    cout << "\n=== Audio device list changed! ===\n\n";
    print_all_devices();
    });
    //}}}
  setAudioDeviceListCallback(cAudioDeviceListEvent::eDefaultInputChanged, [] {
    //{{{
    cout << "\n=== Default input device changed! ===\n\n";
    print_all_devices();
    });
    //}}}
  setAudioDeviceListCallback(cAudioDeviceListEvent::eDefaultOutputChanged, [] {
    //{{{
    cout << "\n=== Default output device changed! ===\n\n";
    print_all_devices();
    });
    //}}}

  auto device = getDefaultAudioOutputDevice();
  if (!device)
    return 1;
  device->setSampleRate (44100);

  //{{{  synth
  auto synth = synthesiser();
  synth.set_sample_rate (float (device->getSampleRate()));
  //}}}
  //{{{  noise
  random_device rd;
  minstd_rand gen (rd());
  uniform_real_distribution<float> white_noise (-1.0f, 1.0f);
  //}}}
  //{{{  sine
  float frequency_hz = 440.0f;
  float delta = 2.0f * frequency_hz * float(M_PI / device->getSampleRate());
  float phase = 0;
  //}}}

  device->connect([=] (cAudioDevice&, sAudioDeviceIo<float>& io) mutable noexcept {
    if (!io.outputBuffer.has_value())
      return;

    auto& out = *io.outputBuffer;
    // melody
    for (int frame = 0; frame < out.getSizeFrames(); ++frame) {
      auto next_sample = synth.get_next_sample();
      for (int channel = 0; channel < out.getSizeChannels(); ++channel)
        out (frame, channel) = next_sample;
      }
    //{{{  sine
    //for (int frame = 0; frame < out.size_frames(); ++frame) {
      //float next_sample = sin (phase);
      //phase = fmod (phase + delta, 2.0f * static_cast<float>(M_PI));
      //for (int channel = 0; channel < out.size_channels(); ++channel)
        //out (frame, channel) = 0.2f * next_sample;
      //}
    //}}}
    //{{{  noise
    //for (int frame = 0; frame < out.size_frames(); ++frame)
      //for (int channel = 0; channel < out.size_channels(); ++channel)
        //out (frame, channel) = white_noise (gen);
    //}}}
    });

  device->start();
  while (!stop.load()) {
    this_thread::sleep_for (chrono::milliseconds(50));
    }
  }
