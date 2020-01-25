// This example app plays a short melody using a simple square wave synthesiser.
//{{{
// Copyright (c) 2018 - Timur Doumler
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
//}}}
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <cmath>
#include <array>
#include <thread>
#include <random>
#include "audio.h"

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"

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
float noteToFrequencyHz (int note) {
  constexpr float pitchStandardHz = 440.0f;
  return pitchStandardHz * pow (2.0f, float (note - 69) / 12.0f);
  }
//}}}

//{{{
bool isDefaultDevice (const cAudioDevice& device) {

  if (device.isInput()) {
    auto defaultInputDevice = getDefaultAudioInputDevice();
    return defaultInputDevice.has_value() && (device.getDeviceId() == defaultInputDevice->getDeviceId());
    }

  else if (device.isOutput()) {
    auto defaultOutputDevice = getDefaultAudioOutputDevice();
    return defaultOutputDevice.has_value() && (device.getDeviceId() == defaultOutputDevice->getDeviceId());
    }

  return false;
  }
//}}}
//{{{
void printDeviceInfo (const cAudioDevice& device) {

  cLog::log (LOGINFO, device.getName());
  cLog::log (LOGINFO, "-  sampleRate = " + dec(device.getSampleRate()) +  "Hz" +
                      " bufferSize = " + dec(device.getBufferSizeFrames()) + " frames " +
                      (device.isInput() ? dec(device.getNumInputChannels()) : dec(device.getNumOutputChannels())) + " chans" +
                      (isDefaultDevice (device) ? " default" : ""));
  };
//}}}
//{{{
void printDeviceList (const cAudioDeviceList& deviceList) {

  for (auto& device : deviceList)
    printDeviceInfo (device);
  }
//}}}
//{{{
void printDevices (const string& title) {

  cLog::log (LOGINFO, title);

  cLog::log (LOGINFO, "Input devices");
  printDeviceList (getAudioInputDeviceList());

  cLog::log (LOGINFO, "Output devices");
  printDeviceList (getAudioOutputDeviceList());
  }
//}}}

atomic<bool> stop = false;
//{{{
struct synthesiser {
  //{{{
  float getNextSample() {

    assert (mSampleRate > 0);

    mMsCounter += 1000.0f / mSampleRate;
    if (mMsCounter >= mNoteDurationMs) {
      mMsCounter = 0;
      if (++mCurrentNoteIndex < notes.size()) {
        update();
        }
      else {
        stop.store (true);
        return 0;
        }
      }

    auto nextSample = copysign (0.1f, sin (mPhase));
    mPhase = fmod (mPhase + mDelta, 2.0f * float(M_PI));
    return nextSample;
    }
  //}}}
  //{{{
  void setSampleRate (int sampleRate) {

    mSampleRate = (float)sampleRate;
    update();
    }
  //}}}

private:
  //{{{
  void update() noexcept {
    float frequencyHz = noteToFrequencyHz (notes.at (mCurrentNoteIndex));
    mDelta = 2.0f * frequencyHz * static_cast<float>(M_PI / mSampleRate);
    }
  //}}}

  float mSampleRate = 0;
  float mDelta = 0;
  float mPhase = 0;
  float mMsCounter = 0;
  float mNoteDurationMs = 60'000.0f / bpm;
  int mCurrentNoteIndex = 0;
  };
//}}}

int main() {
  cLog::init (LOGINFO, false, "", "stdAudio");

  printDevices ("");
  setAudioDeviceListCallback (cAudioDeviceListEvent::eListChanged, [] { printDevices ("deviceList changed"); });
  setAudioDeviceListCallback (cAudioDeviceListEvent::eDefaultInputChanged, [] { printDevices ("Def input changed"); });
  setAudioDeviceListCallback (cAudioDeviceListEvent::eDefaultOutputChanged, [] { printDevices ("Def output changed"); });

  auto device = getDefaultAudioOutputDevice();
  if (!device)
    return 1;
  device->setSampleRate (44100);

  //{{{  synth
  auto synth = synthesiser();
  synth.setSampleRate (device->getSampleRate());
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

  device->connect ([=] (cAudioDevice& device, cAudioBuffer& buffer) mutable noexcept {
    cLog::log (LOGINFO, " connect %d %d %d", device.getSampleRate(), device.getBufferSizeFrames(), buffer.getSizeFrames());

    for (int frame = 0; frame < buffer.getSizeFrames(); ++frame) {
      auto nextSample = synth.getNextSample();
      for (int channel = 0; channel < buffer.getSizeChannels(); ++channel)
        buffer (frame, channel) = nextSample;
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
    this_thread::sleep_for (chrono::milliseconds (50));
    }
  }
