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
//}}}

//{{{
constexpr std::array<int, 22> notes = {
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
  return pitch_standard_hz * std::pow (2.0f, float (note - 69) / 12.0f);
  }
//}}}

//{{{
bool is_default_device (const audio_device& d) {

  if (d.is_input()) {
    auto default_in = get_default_audio_input_device();
    return default_in.has_value() && d.device_id() == default_in->device_id();
    }
  else if (d.is_output()) {
    auto default_out = get_default_audio_output_device();
    return default_out.has_value() && d.device_id() == default_out->device_id();
    }

  return false;
  }
//}}}
//{{{
void print_device_info (const audio_device& d) {

  std::cout << "- \"" << d.name() << "\", ";
  std::cout << "sample rate = " << d.get_sample_rate() << " Hz, ";
  std::cout << "buffer size = " << d.get_buffer_size_frames() << " frames, ";
  std::cout << (d.is_input() ? d.get_num_input_channels() : d.get_num_output_channels()) << " channels";
  std::cout << (is_default_device(d) ? " [DEFAULT DEVICE]\n" : "\n");
  };
//}}}
//{{{
void print_device_list (const audio_device_list& list) {

  for (auto& item : list)
    print_device_info (item);
  }
//}}}
//{{{
void print_all_devices() {

  std::cout << "Input devices:\n==============\n";
  print_device_list (get_audio_input_device_list());

  std::cout << "\nOutput devices:\n===============\n";
  print_device_list (get_audio_output_device_list());
  }
//}}}

std::atomic<bool> stop = false;
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

    auto next_sample = std::copysign (0.1f, std::sin(_phase));
    _phase = std::fmod (_phase + _delta, 2.0f * float(M_PI));
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

  set_audio_device_list_callback (audio_device_list_event::device_list_changed, [] {
    //{{{
    std::cout << "\n=== Audio device list changed! ===\n\n";
    print_all_devices();
    });
    //}}}
  set_audio_device_list_callback (audio_device_list_event::default_input_device_changed, [] {
    //{{{
    std::cout << "\n=== Default input device changed! ===\n\n";
    print_all_devices();
    });
    //}}}
  set_audio_device_list_callback (audio_device_list_event::default_output_device_changed, [] {
    //{{{
    std::cout << "\n=== Default output device changed! ===\n\n";
    print_all_devices();
    });
    //}}}

  auto device = get_default_audio_output_device();
  if (!device)
    return 1;

  device->set_sample_rate (44100);

  auto synth = synthesiser();
  synth.set_sample_rate (float (device->get_sample_rate()));

  std::random_device rd;
  std::minstd_rand gen (rd());
  std::uniform_real_distribution<float> white_noise (-1.0f, 1.0f);

  float frequency_hz = 440.0f;
  float delta = 2.0f * frequency_hz * float(M_PI / device->get_sample_rate());
  float phase = 0;

  device->connect([=] (audio_device&, audio_device_io<float>& io) mutable noexcept {
    if (!io.output_buffer.has_value())
      return;

    auto& out = *io.output_buffer;
    // elody
    for (int frame = 0; frame < out.size_frames(); ++frame) {
      auto next_sample = synth.get_next_sample();
      for (int channel = 0; channel < out.size_channels(); ++channel)
        out (frame, channel) = next_sample;
      }
    //{{{  sine
    //for (int frame = 0; frame < out.size_frames(); ++frame) {
      //float next_sample = std::sin (phase);
      //phase = std::fmod (phase + delta, 2.0f * static_cast<float>(M_PI));
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
    std::this_thread::sleep_for (std::chrono::milliseconds(50));
    }
  }
