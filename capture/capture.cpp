// capture.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <windows.h>

#include <string>
#include <thread>
#include <avrt.h>

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/timestamp.h>
  #include <libswresample/swresample.h>
  }

#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cCaptureWASAPI.h"
#include "../../shared/utils/audioHelpers.h"
//}}}

int main() {
  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "capture");
  av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (cLog::avLogCallback);

  auto capture = new cCaptureWASAPI (0x80000000);
  cWavWriter wavWriter ("D:/Capture/capture.wav", capture->getWaveFormatEx());
  cAacWriter aacWriter ("D:/Capture/capture.aac", capture->getChannels(), 48000, 128000);

  std::thread ([=]() {
    //{{{  cCaptureWASAPI run
    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("capt");

    // elevate task priority
    DWORD nTaskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics ("Audio", &nTaskIndex);
    if (hTask) {
      capture->run();
      AvRevertMmThreadCharacteristics (hTask);
      }

    CoUninitialize();
    //}}}
    } ).detach();

  const int frameSize = aacWriter.getFrameSize();
  cLog::log (LOGINFO, "capture and encode with frameSize:%d", frameSize);

  while (true) {
    auto framePtr = capture->getFrame (frameSize);
    if (framePtr) {
      wavWriter.write (framePtr, frameSize);
      aacWriter.write (framePtr, frameSize);
      }
    else
      Sleep (10);
    }

  delete (capture);

  CoUninitialize();
  return 0;
  }
