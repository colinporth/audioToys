// capture.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

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

#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cBipBuffer.h"
//}}}

#define CHANNELS 2
#define SAMPLE_RATE 48000
#define ENCODER_BITRATE 128000

//{{{
class cWavFile {
public:
  //{{{
  cWavFile (char* filename, WAVEFORMATEX* waveFormatEx) {

    mFilename = (char*)malloc (strlen (filename));
    strcpy (mFilename, filename);
    open (mFilename, waveFormatEx);
    }
  //}}}
  //{{{
  ~cWavFile() {
    finish();
    }
  //}}}

  bool getOk() { return mOk; }
  //{{{
  void write (void* data, LONG bytesToWrite) {

    auto bytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(data), bytesToWrite);

    if (bytesWritten == bytesToWrite)
      framesWritten +=  bytesToWrite/ mBlockAlign;
    else
      cLog::log (LOGERROR, "mmioWrite failed - wrote only %u of %u bytes", bytesWritten, bytesToWrite);
    }
  //}}}

private:
  //{{{
  void open (char* filename, WAVEFORMATEX* waveFormatEx) {

    MMIOINFO mi = { 0 };
    file = mmioOpen (filename, &mi, MMIO_READWRITE | MMIO_CREATE);

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

    mBlockAlign = waveFormatEx->nBlockAlign;
    mOk = true;
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
    file = mmioOpen (mFilename, &mi, MMIO_READWRITE);
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

  char* mFilename;
  HMMIO file;

  MMCKINFO ckRIFF = { 0 };
  MMCKINFO ckData = { 0 };

  bool mOk = false;

  int mBlockAlign = 0;
  int framesWritten = 0;
  };
//}}}

//{{{
class cCapture {
public:
  //{{{
  cCapture() {

    // activate a device enumerator
    IMMDeviceEnumerator* pMMDeviceEnumerator = NULL;
    if (FAILED (CoCreateInstance (__uuidof(MMDeviceEnumerator), NULL,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pMMDeviceEnumerator))) {
      cLog::log (LOGERROR, "cCapture create IMMDeviceEnumerator failed");
      }

    else {
      // get default render endpoint
      if (FAILED (pMMDeviceEnumerator->GetDefaultAudioEndpoint (eRender, eMultimedia, &mMMDevice)))
        cLog::log (LOGERROR, "cCapture MMDeviceEnumerator::GetDefaultAudioEndpoint failed");

      mBipBuffer.allocateBuffer (0x100000); // 1mb

      if (FAILED (mMMDevice->Activate (__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&mAudioClient)))
        cLog::log (LOGERROR, "cCapture IMMDevice::Activate IAudioClient failed");

      if (FAILED (mAudioClient->GetDevicePeriod (&mHhnsDefaultDevicePeriod, NULL)))
        cLog::log (LOGERROR, "cCapture audioClient GetDevicePeriod failed");

      // get the default device format
      if (FAILED (mAudioClient->GetMixFormat (&mWaveFormatEx)))
        cLog::log (LOGERROR, "cCapture audioClient GetMixFormat failed");

      // with AUDCLNT_STREAMFLAGS_LOOPBACK, AUDCLNT_STREAMFLAGS_EVENTCALLBACK "data ready" event never gets set
      if (FAILED (mAudioClient->Initialize (
           AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, mWaveFormatEx, 0)))
        cLog::log (LOGERROR, "cCapture AudioClient initialize failed");

      // activate an IAudioCaptureClient
      if (FAILED (mAudioClient->GetService (__uuidof(IAudioCaptureClient), (void**)&mAudioCaptureClient)))
        cLog::log (LOGERROR, "cCapture create IAudioCaptureClient failed");

      // start audioClient
      if (FAILED (mAudioClient->Start()))
        cLog::log (LOGERROR, "cCapture audioClient start failed");

      if (pMMDeviceEnumerator)
        pMMDeviceEnumerator->Release();
      }
    }
  //}}}
  //{{{
  ~cCapture() {

    if (mAudioClient) {
      mAudioClient->Stop();
      mAudioClient->Release();
      }

    if (mAudioCaptureClient)
      mAudioCaptureClient->Release();

    if (mWaveFormatEx)
      CoTaskMemFree (mWaveFormatEx);

    if (mMMDevice)
      mMMDevice->Release();
    }
  //}}}

  //{{{
  void run() {

    // create a periodic waitable timer, -ve relative time, convert to milliseconds
    HANDLE wakeUp = CreateWaitableTimer (NULL, FALSE, NULL);
    LARGE_INTEGER firstFire;
    firstFire.QuadPart = -mHhnsDefaultDevicePeriod / 2;
    LONG timeBetweenFires = (LONG)mHhnsDefaultDevicePeriod / 2 / (10 * 1000);
    SetWaitableTimer (wakeUp, &firstFire, timeBetweenFires, NULL, NULL, FALSE);

    bool done = false;
    while (!done) {
      UINT32 packetSize;
      mAudioCaptureClient->GetNextPacketSize (&packetSize);
      while (!done && (packetSize > 0)) {
        BYTE* data;
        UINT32 numFramesRead;
        DWORD dwFlags;
        if (FAILED (mAudioCaptureClient->GetBuffer (&data, &numFramesRead, &dwFlags, NULL, NULL))) {
          //{{{  exit
          cLog::log (LOGERROR, "IAudioCaptureClient::GetBuffer failed");
          done = true;
          break;
          }
          //}}}
        if (numFramesRead == 0) {
          //{{{  exit
          cLog::log (LOGERROR, "audioCaptureClient::GetBuffer read 0 frames");
          done = true;
          }
          //}}}
        if (dwFlags == AUDCLNT_BUFFERFLAGS_SILENT)
          cLog::log (LOGINFO, "audioCaptureClient::GetBuffer silent %d", numFramesRead);
        else {
          if (dwFlags == AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
            cLog::log (LOGINFO, "audioCaptureClient::GetBuffer discontinuity");

          cLog::log (LOGINFO2, "captured frames:%d bytes:%d", numFramesRead, numFramesRead * mWaveFormatEx->nBlockAlign);
          LONG bytesToWrite = numFramesRead * mWaveFormatEx->nBlockAlign;
          int bytesAllocated = 0;
          uint8_t* ptr = mBipBuffer.reserve (bytesToWrite, bytesAllocated);
          if (ptr && (bytesAllocated == bytesToWrite)) {
            memcpy (ptr, data, bytesAllocated);
            mBipBuffer.commit (bytesAllocated);
            }
          else
            cLog::log (LOGERROR, "captureThread buffer full on write %d of  %d", bytesAllocated, bytesToWrite);
          }

        if (FAILED (mAudioCaptureClient->ReleaseBuffer (numFramesRead))) {
          //{{{  exit
          cLog::log (LOGERROR, "audioCaptureClient::ReleaseBuffer failed");
          done = true;
          break;
          }
          //}}}

        mAudioCaptureClient->GetNextPacketSize (&packetSize);
        }

      DWORD dwWaitResult = WaitForSingleObject (wakeUp, INFINITE);
      if (dwWaitResult != WAIT_OBJECT_0) {
        //{{{  exit
        cLog::log (LOGERROR, "WaitForSingleObject error %u", dwWaitResult);
        done = true;
        }
        //}}}
      }

    CancelWaitableTimer (wakeUp);
    CloseHandle (wakeUp);
    }
  //}}}

  WAVEFORMATEX* mWaveFormatEx = NULL;

  cBipBuffer mBipBuffer;
  float mMaxSampleValue = 0.f;

private:
  IMMDevice* mMMDevice = NULL;

  IAudioClient* mAudioClient = NULL;
  IAudioCaptureClient* mAudioCaptureClient = NULL;
  REFERENCE_TIME mHhnsDefaultDevicePeriod;
  };
//}}}
//{{{
DWORD WINAPI captureThread (LPVOID param) {

  cCapture* capture = (cCapture*)param;

  cLog::setThreadName ("capt");
  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  //  register task with MMCSS, set off wakeup timer
  DWORD nTaskIndex = 0;
  HANDLE hTask = AvSetMmThreadCharacteristics ("Audio", &nTaskIndex);
  if (hTask) {
    capture->run();
    AvRevertMmThreadCharacteristics (hTask);
    }
  else
    cLog::log (LOGERROR, "AvSetMmThreadCharacteristics failed %u", GetLastError());

  CoUninitialize();

  return 0;
  }
//}}}

//{{{
int writeDataCallback (void* file, uint8_t* data, int size) {

  fwrite (data, 1, size, (FILE*)file);
  return size;
  }
//}}}
//{{{
void avLogCallback (void* ptr, int level, const char* fmt, va_list vargs) {

  char str[256];
  vsnprintf (str, 256, fmt, vargs);
  auto len = strlen (str);
  if (len > 0)
    str[len-1] = 0;

  cLog::log (LOGINFO, str);
  }
//}}}
//{{{
int main() {

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "capture");

  //av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (avLogCallback);

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cCapture capture;
  cWavFile wavFile ("D:/Capture/out.wav", capture.mWaveFormatEx);

  // capture thread
  HANDLE hThread = CreateThread (NULL, 0, captureThread, &capture, 0, NULL );
  if (!hThread) {
    //{{{
    cLog::log (LOGERROR, "CreateThread failed: last error is %u", GetLastError());
    return 0;
    }
    //}}}

  while (capture.mBipBuffer.getCommittedSize() == 0)
    Sleep (10);

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
  encoderContext->codec_type = AVMEDIA_TYPE_AUDIO;
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

  float lastMaxSampleValue = 0.f;
  bool done = false;
  while (!done) {
    int bytes = encoderContext->frame_size;
    auto ptr = (float*)capture.mBipBuffer.getContiguousBlock (bytes);

    if (bytes >= encoderContext->frame_size) {
      // enough data to encode
      cLog::log (LOGINFO1, "encode read block frame_size bytes:%d", bytes);

      wavFile.write (ptr, bytes);

      // float32 interleaved to float32 planar
      auto samplesL = (float*)frame->data[0];
      auto samplesR = (float*)frame->data[1];
      for (auto sample = 0; sample < encoderContext->frame_size; sample++) {
        capture.mMaxSampleValue = max (capture.mMaxSampleValue, abs(*ptr));
        samplesL[sample] = *ptr++;
        capture.mMaxSampleValue = max (capture.mMaxSampleValue, abs(*ptr));
        samplesR[sample] = *ptr++;
        }
      capture.mBipBuffer.decommitBlock (bytes);

      if (!avcodec_send_frame (encoderContext, frame))
        while (!avcodec_receive_packet (encoderContext, packet))
          if (av_write_frame (outputFormatContext, packet) < 0) {
            done = true;
            break;
            }

      if (capture.mMaxSampleValue > lastMaxSampleValue) {
        cLog::log (LOGINFO, "new max %6.4f", capture.mMaxSampleValue);
        lastMaxSampleValue = capture.mMaxSampleValue;
        }
      }
    else
      Sleep (10);
    }

  // Flush cached packets
  if (!avcodec_send_frame (encoderContext, NULL))
    while (!avcodec_receive_packet (encoderContext, packet))
      if (av_write_frame (outputFormatContext, packet) < 0)
        break;

  av_write_trailer (outputFormatContext);
  fclose (aacFile);

  avcodec_free_context (&encoderContext);
  av_frame_free (&frame);
  avformat_free_context (outputFormatContext);
  av_freep (&ioContext);
  av_freep (&outBuffer);
  av_packet_free (&packet);

  CloseHandle (hThread);
  CoUninitialize();

  return 0;
  }
//}}}
