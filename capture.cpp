// capture.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <stdio.h>
#include <windows.h>

#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>

#include <initguid.h>

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/timestamp.h>
  #include <libswresample/swresample.h>
  }

#include <string>

#include "../shared/utils/cLog.h"
#include "../shared/utils/cBipBuffer.h"
//}}}
#define DEFAULT_FILE L"out.wav"

#define CHANNELS 2
#define SAMPLE_RATE 48000
#define ENCODER_BITRATE 128000

//{{{
class cCoInitialize {
public:
  cCoInitialize() {
    CoInitialize (NULL);
    }

  ~cCoInitialize() {
     CoUninitialize();
     }
  };
//}}}
//{{{
class cCapture {
public:
  //{{{
  cCapture() {
    getCaptureDevice();
    mBipBuffer.allocateBuffer (1024 * 32);

    if (FAILED (mMMDevice->Activate (__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient)))
      cLog::log (LOGERROR, "IMMDevice::Activate (IAudioClient) failed");

    if (FAILED (audioClient->GetDevicePeriod (&hnsDefaultDevicePeriod, NULL)))
      cLog::log (LOGERROR, "IAudioClient::GetDevicePeriod failed");

    // get the default device format
    if (FAILED (audioClient->GetMixFormat (&mWaveFormatEx)))
      cLog::log (LOGERROR, "IAudioClient::GetMixFormat failed");

    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK do not work together...
    // the "data ready" event never gets set so we're going to do a timer-driven loop
    if (FAILED (audioClient->Initialize (
         AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, mWaveFormatEx, 0)))
      cLog::log (LOGERROR, "IAudioClient::Initialize failed");

    // activate an IAudioCaptureClient
    if (FAILED (audioClient->GetService (__uuidof(IAudioCaptureClient), (void**)&audioCaptureClient)))
      cLog::log (LOGERROR, "IAudioClient::GetService (IAudioCaptureClient) failed");

    // start audioClient
    if (FAILED (audioClient->Start()))
      cLog::log (LOGERROR, "IAudioClient::Start failed");
    }
  //}}}
  //{{{
  ~cCapture() {

    if (audioClient) {
      audioClient->Stop();
      audioClient->Release();
      }

    if (audioCaptureClient)
      audioCaptureClient->Release();

    if (mWaveFormatEx)
      CoTaskMemFree (mWaveFormatEx);

    if (mMMDevice)
      mMMDevice->Release();
    }
  //}}}

  //{{{
  static DWORD WINAPI captureThread (LPVOID param) {

    cLog::setThreadName ("capt");
    cCapture* capture = (cCapture*)param;

    cCoInitialize coInitialize;
    //{{{  register task with MMCSS
    DWORD nTaskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics (L"Audio", &nTaskIndex);

    if (hTask == NULL) {
      cLog::log (LOGERROR, "AvSetMmThreadCharacteristics failed: last error = %u", GetLastError());
      return 0;
      }
    //}}}
    //{{{  create and set the waitable timer
    // create a periodic waitable timer
    HANDLE wakeUp = CreateWaitableTimer (NULL, FALSE, NULL);

    if (NULL == wakeUp) {
      cLog::log (LOGERROR, "CreateWaitableTimer failed: last error = %u", GetLastError());
      return 0;
      }

    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart = -capture->hnsDefaultDevicePeriod / 2; // negative means relative time
    LONG lTimeBetweenFires = (LONG)capture->hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds

    if (!SetWaitableTimer (wakeUp, &liFirstFire, lTimeBetweenFires, NULL, NULL, FALSE)) {
      cLog::log (LOGERROR, "SetWaitableTimer failed: last error = %u", GetLastError());
      return 0;
      }
    //}}}

    bool done = false;
    while (!done) {
      UINT32 packetSize;
      capture->audioCaptureClient->GetNextPacketSize (&packetSize);
      while (packetSize > 0) {
        BYTE* data;
        UINT32 numFramesRead;
        DWORD dwFlags;
        if (FAILED (capture->audioCaptureClient->GetBuffer (&data, &numFramesRead, &dwFlags, NULL, NULL))) {
          cLog::log (LOGERROR, "IAudioCaptureClient::GetBuffer failed");
          done = true;
          break;
          }
        if (AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags)
          cLog::log (LOGINFO, "audioCaptureClient::GetBuffer discontinuity");
        else if (dwFlags != 0)
          cLog::log (LOGINFO, "audioCaptureClient::GetBuffer flags 0x%08x", dwFlags);
        if (numFramesRead == 0) {
          cLog::log (LOGERROR, "audioCaptureClient::GetBuffer read 0 frames");
          done = true;
          }

        cLog::log (LOGINFO2, "captured frames %d bytes:%d", numFramesRead, numFramesRead * capture->mWaveFormatEx->nBlockAlign);
        LONG bytesToWrite = numFramesRead * capture->mWaveFormatEx->nBlockAlign;
        int bytesAllocated = 0;
        uint8_t* ptr = capture->mBipBuffer.reserve (bytesToWrite, bytesAllocated);
        if (ptr && (bytesAllocated == bytesToWrite)) {
          memcpy (ptr, data, bytesAllocated);
          capture->mBipBuffer.commit (bytesAllocated);
          }

        if (FAILED (capture->audioCaptureClient->ReleaseBuffer (numFramesRead))) {
          cLog::log (LOGERROR, "audioCaptureClient::ReleaseBuffer failed");
          done = true;
          break;
          }

        capture->audioCaptureClient->GetNextPacketSize (&packetSize);
        }

      DWORD dwWaitResult = WaitForSingleObject (wakeUp, INFINITE);
      if (dwWaitResult != WAIT_OBJECT_0) {
        cLog::log (LOGERROR, "WaitForSingleObject return value %u", dwWaitResult);
        done = true;
        }
      }

    AvRevertMmThreadCharacteristics (hTask);
    CancelWaitableTimer (wakeUp);
    CloseHandle (wakeUp);

    return 0;
    }
  //}}}

  IMMDevice* mMMDevice = NULL;
  IAudioClient* audioClient = NULL;
  REFERENCE_TIME hnsDefaultDevicePeriod;
  WAVEFORMATEX* mWaveFormatEx = NULL;
  IAudioCaptureClient* audioCaptureClient = NULL;

  cBipBuffer mBipBuffer;

private:
  //{{{
  void getCaptureDevice() {

    // activate a device enumerator
    IMMDeviceEnumerator* pMMDeviceEnumerator = NULL;
    if (FAILED (CoCreateInstance (__uuidof(MMDeviceEnumerator), NULL,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pMMDeviceEnumerator))) {
      cLog::log (LOGERROR, "getCaptureDevice CoCreateInstance (IMMDeviceEnumerator) failed");
      return;
      }

    // get default render endpoint
    if (FAILED (pMMDeviceEnumerator->GetDefaultAudioEndpoint (eRender, eConsole, &mMMDevice)))
      cLog::log (LOGERROR, "getCaptureDevice IMMDeviceEnumerator::GetDefaultAudioEndpoint failed");

    if (pMMDeviceEnumerator)
      pMMDeviceEnumerator->Release();
    }

  //}}}
  };
//}}}
//{{{
class cWaveFile {
public:
  //{{{
  cWaveFile (WAVEFORMATEX* waveFormatEx) {
    open (waveFormatEx);
    }
  //}}}
  //{{{
  ~cWaveFile() {
    finish();
    }
  //}}}

  //{{{
  void write (void* data, int frames, LONG bytesToWrite) {

    auto bytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(data), bytesToWrite);

    if (bytesWritten == bytesToWrite)
      framesWritten += frames;
    else
      cLog::log (LOGERROR, "mmioWrite failed - wrote only %u of %u bytes", bytesWritten, bytesToWrite);
    }
  //}}}

private:
  //{{{
  void open (WAVEFORMATEX* waveFormatEx) {

    MMIOINFO mi = { 0 };
    file = mmioOpen (const_cast<LPWSTR>(DEFAULT_FILE), &mi, MMIO_READWRITE | MMIO_CREATE);

    // make a RIFF/WAVE chunk
    ckRIFF.ckid = MAKEFOURCC ('R', 'I', 'F', 'F');
    ckRIFF.fccType = MAKEFOURCC ('W', 'A', 'V', 'E');

    MMRESULT result = mmioCreateChunk (file, &ckRIFF, MMIO_CREATERIFF);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioCreateChunk (\"RIFF/WAVE\") failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}

    // make a 'fmt ' chunk (within the RIFF/WAVE chunk)
    MMCKINFO chunk;
    chunk.ckid = MAKEFOURCC ('f', 'm', 't', ' ');
    result = mmioCreateChunk (file, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioCreateChunk (\"fmt \") failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}

    // write the WAVEFORMATEX data to it
    LONG lBytesInWfx = sizeof(WAVEFORMATEX) + waveFormatEx->cbSize;
    LONG lBytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(waveFormatEx)), lBytesInWfx);
    if (lBytesWritten != lBytesInWfx) {
      //{{{
      cLog::log (LOGERROR, "mmioWrite (fmt data) wrote %u bytes; expected %u bytes", lBytesWritten, lBytesInWfx);
      return;
      }
      //}}}

    // ascend from the 'fmt ' chunk
    result = mmioAscend(file, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioAscend (\"fmt \" failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}

    // make a 'fact' chunk whose data is (DWORD)0
    chunk.ckid = MAKEFOURCC ('f', 'a', 'c', 't');
    result = mmioCreateChunk (file, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioCreateChunk (\"fmt \") failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}

    // write (DWORD)0 to it
    // this is cleaned up later
    framesWritten = 0;
    lBytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(&framesWritten), sizeof(framesWritten));
    if (lBytesWritten != sizeof (framesWritten)) {
      //{{{
      cLog::log (LOGERROR, "mmioWrite(fact data) wrote %u bytes; expected %u bytes", lBytesWritten, (UINT32)sizeof(framesWritten));
      return;
      }
      //}}}

    // ascend from the 'fact' chunk
    result = mmioAscend (file, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioAscend (\"fact\" failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}

    // make a 'data' chunk and leave the data pointer there
    ckData.ckid = MAKEFOURCC ('d', 'a', 't', 'a');
    result = mmioCreateChunk (file, &ckData, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}
    }
  //}}}
  //{{{
  void finish() {

    MMRESULT result = mmioAscend (file, &ckData, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioAscend(\"data\" failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}

    result = mmioAscend (file, &ckRIFF, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x", result);
      return;
      }
      //}}}

    result = mmioClose (file, 0);
    file = NULL;
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioClose failed: MMSYSERR = %u", result);
      return;
      }
      //}}}

    // everything went well... fixup the fact chunk in the file

    // reopen the file in read/write mode
    MMIOINFO mi = { 0 };
    file = mmioOpen (const_cast<LPWSTR>(DEFAULT_FILE), &mi, MMIO_READWRITE);
    if (NULL == file) {
      //{{{
      cLog::log (LOGERROR, "mmioOpen failed");
      return;
      }
      //}}}

    // descend into the RIFF/WAVE chunk
    MMCKINFO ckRIFF1 = {0};
    ckRIFF1.ckid = MAKEFOURCC ('W', 'A', 'V', 'E'); // this is right for mmioDescend
    result = mmioDescend (file, &ckRIFF1, NULL, MMIO_FINDRIFF);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioDescend(\"WAVE\") failed: MMSYSERR = %u", result);
      return;
      }
      //}}}

    // descend into the fact chunk
    MMCKINFO ckFact = {0};
    ckFact.ckid = MAKEFOURCC ('f', 'a', 'c', 't');
    result = mmioDescend (file, &ckFact, &ckRIFF1, MMIO_FINDCHUNK);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioDescend(\"fact\") failed: MMSYSERR = %u", result);
      return;
      }
      //}}}

    // write framesWritten to the fact chunk
    LONG bytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(&framesWritten), sizeof(framesWritten));
    if (bytesWritten != sizeof (framesWritten)) {
      //{{{
      cLog::log (LOGERROR, "Updating the fact chunk wrote %u bytes; expected %u", bytesWritten, (UINT32)sizeof(framesWritten));
      return;
      }
      //}}}

    // ascend out of the fact chunk
    result = mmioAscend (file, &ckFact, 0);
    if (MMSYSERR_NOERROR != result) {
      //{{{
      cLog::log (LOGERROR, "mmioAscend(\"fact\") failed: MMSYSERR = %u", result);
      return;
      }
      //}}}

    mmioClose (file, 0);
    }
  //}}}

  HMMIO file;

  MMCKINFO ckRIFF = { 0 };
  MMCKINFO ckData = { 0 };

  int framesWritten = 0;
  };
//}}}

//{{{
int writeDataCallback (void* file, uint8_t* data, int size) {

  fwrite (data, 1, size, (FILE*)file);
  return size;
  }
//}}}
//{{{
void avLogCallback (void* ptr, int level, const char* fmt, va_list vargs) {
  cLog::log (LOGINFO, fmt, vargs);
  }
//}}}
//{{{
int main() {

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "capture");

  //av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (avLogCallback);

  cCoInitialize coInitialize;

  cCapture capture;

  // capture thread
  HANDLE hThread = CreateThread (NULL, 0, capture.captureThread, &capture, 0, NULL );
  if (!hThread) {
    //{{{
    cLog::log (LOGERROR, "CreateThread failed: last error is %u", GetLastError());
    return 0;
    }
    //}}}

  while (capture.mBipBuffer.getCommittedSize() == 0)
    Sleep (10);
  cWaveFile waveFile (capture.mWaveFormatEx);

  FILE* aacFile = fopen ("out.aac", "wb");

  AVCodec* codec = avcodec_find_encoder (AV_CODEC_ID_AAC);

  AVCodecContext* encoderContext = avcodec_alloc_context3 (codec);
  encoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
  encoderContext->bit_rate = ENCODER_BITRATE;
  encoderContext->sample_rate = SAMPLE_RATE;
  encoderContext->channels = CHANNELS;
  encoderContext->channel_layout = av_get_default_channel_layout (CHANNELS);
  encoderContext->time_base.num = 1;
  encoderContext->time_base.den = SAMPLE_RATE;
  encoderContext->codec_type = AVMEDIA_TYPE_AUDIO ;
  avcodec_open2 (encoderContext, codec, NULL);

  // create ADTS container for encoded frames
  AVOutputFormat* outputFormat = av_guess_format ("adts", NULL, NULL);
  AVFormatContext* outputFormatContext = NULL;
  avformat_alloc_output_context2 (&outputFormatContext, outputFormat, "", NULL);

  // create ioContext for adts container with writeData callback
  int outBufferSize = 4096;
  uint8_t* outBuffer = (uint8_t*)av_malloc (outBufferSize);
  AVIOContext* ioContext = avio_alloc_context (outBuffer, outBufferSize, 1, aacFile, NULL, &writeDataCallback, NULL);

  // link container's context to the previous I/O context
  outputFormatContext->pb = ioContext;
  AVStream* adts_stream = avformat_new_stream (outputFormatContext, NULL);
  adts_stream->id = outputFormatContext->nb_streams-1;

  // copy encoder's parameters
  avcodec_parameters_from_context (adts_stream->codecpar, encoderContext);

  // allocate stream private data and write the stream header
  avformat_write_header (outputFormatContext, NULL);

  // allocate an frame to be filled with input data.
  AVFrame* frame = av_frame_alloc();
  frame->format = AV_SAMPLE_FMT_FLTP;
  frame->channels = CHANNELS;
  frame->nb_samples = encoderContext->frame_size;
  frame->sample_rate = SAMPLE_RATE;
  frame->channel_layout = av_get_default_channel_layout (CHANNELS);

  cLog::log (LOGINFO, "frame_size %d", encoderContext->frame_size);

  // allocate the frame's data buffer
  av_frame_get_buffer (frame, 0);

  AVPacket* packet = av_packet_alloc();

  while (true) {
    int len = encoderContext->frame_size;
    auto ptr = (float*)capture.mBipBuffer.getContiguousBlock (len);

    if (len >= encoderContext->frame_size) {
      // enough data to encode
      cLog::log (LOGINFO1, "encode read block frame_size bytes:%d", len);

      waveFile.write (ptr, len / capture.mWaveFormatEx->nBlockAlign, len);

      auto samplesL = (float*)frame->data[0];
      auto samplesR = (float*)frame->data[1];
      for (auto sample = 0; sample < encoderContext->frame_size; sample++) {
        samplesL[sample] = *ptr++;
        samplesR[sample] = *ptr++;
        }
      capture.mBipBuffer.decommitBlock (len);

      if (avcodec_send_frame (encoderContext, frame) == 0)
        while (avcodec_receive_packet (encoderContext, packet) == 0)
          if (av_write_frame (outputFormatContext, packet) < 0)
            exit(0);
      }
    else
      Sleep (1);
    }

  // Flush cached packets
  if (avcodec_send_frame (encoderContext, NULL) == 0)
    while (avcodec_receive_packet (encoderContext, packet) == 0)
      if (av_write_frame (outputFormatContext, packet) < 0)
        exit(0);

  av_write_trailer (outputFormatContext);
  fclose (aacFile);

  avcodec_free_context (&encoderContext);
  av_frame_free (&frame);
  avformat_free_context (outputFormatContext);
  av_freep (&ioContext);
  av_freep (&outBuffer);
  av_packet_free (&packet);

  CloseHandle (hThread);

  return 0;
  }
//}}}
