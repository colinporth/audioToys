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
//}}}

const int kCaptureChannnels = 2;

//{{{
class cWavWriter {
public:
  //{{{
  cWavWriter (char* filename, WAVEFORMATEX* waveFormatEx) {

    mFilename = (char*)malloc (strlen (filename));
    strcpy (mFilename, filename);
    open (mFilename, waveFormatEx);
    }
  //}}}
  //{{{
  ~cWavWriter() {
    finish();
    }
  //}}}

  bool getOk() { return mOk; }
  //{{{
  void write (float* data, int numSamples) {

    LONG bytesToWrite = numSamples * mBlockAlign;
    auto bytesWritten = mmioWrite (file, reinterpret_cast<PCHAR>(data), bytesToWrite);

    if (bytesWritten == bytesToWrite)
      framesWritten +=  bytesToWrite / mBlockAlign;
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
class cAacWriter {
public:
  //{{{
  cAacWriter (char* filename, int channels, int sampleRate, int bitRate) {

  #define OUTPUT_CHANNELS 2
  #define OUTPUT_BIT_RATE 128000
  #define OUTPUT_SAMPLE_RATE 48000

    if (openOutFile (filename, channels, sampleRate, bitRate))
      if (avformat_write_header (mFormatContext, NULL) < 0)
        mOk = true;
    }
  //}}}
  //{{{
  ~cAacWriter() {

    bool flushingEncoder = true;
    while (flushingEncoder)
      encodeFrame (NULL, flushingEncoder);

    av_write_trailer (mFormatContext);
    }
  //}}}

  bool getOk() { return mOk; }

  int getChannels() { return mCodecContext->channels; }
  int getSampleRate() { return mCodecContext->sample_rate; }
  int getBitRate() { return (int)mCodecContext->bit_rate; }
  int getFrameSize() { return mCodecContext->frame_size; }

  //{{{
  void write (float* data, int numSamples) {

    AVFrame* frame = av_frame_alloc();
    frame->nb_samples = mCodecContext->frame_size;
    frame->channel_layout = mCodecContext->channel_layout;
    frame->format = mCodecContext->sample_fmt;
    frame->sample_rate = mCodecContext->sample_rate;
    av_frame_get_buffer (frame, 0);

    auto samplesL = (float*)frame->data[0];
    auto samplesR = (float*)frame->data[1];
    for (int i = 0; i < getFrameSize(); i++) {
      *samplesL++ = *data++;
      *samplesR++ = *data++;
      }

    bool hasData;
    encodeFrame (frame, hasData);
    av_frame_free (&frame);
    }
  //}}}

private:
  //{{{
  bool openOutFile (const char* filename, int channels, int sampleRate, int bitRate) {

    // Find the encoder to be used by its name
    AVCodec* codec = avcodec_find_encoder (AV_CODEC_ID_AAC);
    mCodecContext = avcodec_alloc_context3 (codec);

    // Set the basic encoder parameters, input file's sample rate is used to avoid a sample rate conversion
    mCodecContext->channels = channels;
    mCodecContext->channel_layout = av_get_default_channel_layout (OUTPUT_CHANNELS);
    mCodecContext->sample_rate = sampleRate;
    mCodecContext->sample_fmt = codec->sample_fmts[0];
    mCodecContext->bit_rate = bitRate;
    mCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    // Open the output file to write to it. */
    AVIOContext* ioContext;
    int error = avio_open (&ioContext, filename, AVIO_FLAG_WRITE);

    // Create a new format context for the output container format
    mFormatContext = avformat_alloc_context();
    mFormatContext->pb = ioContext;
    mFormatContext->oformat = av_guess_format (NULL, filename, NULL);

    // Create a new audio stream in the output file container. */
    AVStream* stream = avformat_new_stream (mFormatContext, NULL);
    stream->time_base.den = OUTPUT_SAMPLE_RATE;
    stream->time_base.num = 1;

    // Some container formats (like MP4) require global headers to be present
    // Mark the encoder so that it behaves accordingly
    if (mFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
      mCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Open the encoder for the audio stream to use it later
    //auto res = av_opt_set (codecContext->priv_data, "profile", "aac_he", 0);
    //printf ("setopt %x", res);
    error = avcodec_open2 (mCodecContext, codec, NULL);
    error = avcodec_parameters_from_context (stream->codecpar, mCodecContext);

    return true;
    }
  //}}}
  //{{{
  bool encodeFrame (AVFrame* frame, bool& hasData) {

    bool ok = false;
    hasData = false;

    // Packet used for temporary storage
    AVPacket output_packet;
    av_init_packet (&output_packet);
    output_packet.data = NULL;
    output_packet.size = 0;

    // Set a timestamp based on the sample rate for the container
    if (frame) {
      frame->pts = mPts;
      mPts += frame->nb_samples;
      }

    // Send the audio frame stored in the temporary packet to the encoder
    // The output audio stream encoder is used to do this The encoder signals that it has nothing more to encode
    int error = avcodec_send_frame (mCodecContext, frame);
    if (error == AVERROR_EOF) {
      ok = true;
      goto cleanup;
      }
    else if (error < 0) {
      cLog::log (LOGERROR, "error send packet for encoding");
      goto cleanup;
      }

    // Receive one encoded frame from the encoder
    // If the encoder asks for more data to be able to provide an encoded frame, return indicating that no data is present
    error = avcodec_receive_packet (mCodecContext, &output_packet);
    if (error == AVERROR(EAGAIN))
      ok = true;
    else if (error == AVERROR_EOF)
      ok = true;
    else if (error < 0)
      cLog::log (LOGERROR, "error encode frame");
    else {
      ok = true;
      hasData = true;
      }

    // Write one audio frame from the temporary packet to the output file
    if (hasData)
      if (av_write_frame (mFormatContext, &output_packet) < 0) {
        ok = false;
        cLog::log (LOGERROR, "error write frame");
        }

  cleanup:
    av_packet_unref (&output_packet);
    return ok;
    }
  //}}}

  AVCodecContext* mCodecContext = NULL;
  AVFormatContext* mFormatContext = NULL;

  bool mOk = false;

  int64_t mPts = 0;
  };
//}}}
//{{{
class cCaptureWASAPI {
public:
  //{{{
  cCaptureWASAPI() {

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

      // simple 1Gb big linear buffer for now
      mStreamFirst = (float*)malloc (0x40000000);
      mStreamLast = mStreamFirst + (0x40000000 / 4);
      mStreamReadPtr = mStreamFirst;
      mStreamWritePtr = mStreamFirst;
      }
    }
  //}}}
  //{{{
  ~cCaptureWASAPI() {

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

    free (mStreamFirst);
    }
  //}}}

  //{{{
  float* getFrame (int frameSize) {
  // get frameSize worth of floats if available

    if (mStreamWritePtr - mStreamReadPtr >= frameSize * kCaptureChannnels) {
      auto ptr = mStreamReadPtr;
      mStreamReadPtr += frameSize * kCaptureChannnels;
      return ptr;
      }
    else
      return nullptr;
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

          //cLog::log (LOGINFO, "captured frames:%d bytes:%d", numFramesRead, numFramesRead * mWaveFormatEx->nBlockAlign);
          memcpy (mStreamWritePtr, data, numFramesRead * mWaveFormatEx->nBlockAlign);
          mStreamWritePtr += numFramesRead * kCaptureChannnels;
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

private:
  IMMDevice* mMMDevice = NULL;

  IAudioClient* mAudioClient = NULL;
  IAudioCaptureClient* mAudioCaptureClient = NULL;
  REFERENCE_TIME mHhnsDefaultDevicePeriod;

  float* mStreamFirst = nullptr;
  float* mStreamLast = nullptr;
  float* mStreamReadPtr = nullptr;
  float* mStreamWritePtr = nullptr;
  };
//}}}
//{{{
DWORD WINAPI captureThread (LPVOID param) {

  cCaptureWASAPI* capture = (cCaptureWASAPI*)param;

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
void avLogCallback (void* ptr, int level, const char* fmt, va_list vargs) {

  char str[100];
  vsnprintf (str, 100, fmt, vargs);

  // trim trailing return
  auto len = strlen (str);
  if (len > 0)
    str[len-1] = 0;

  switch (level) {
    case AV_LOG_PANIC:
      cLog::log (LOGERROR,   "ffmpeg Panic - %s", str);
      break;
    case AV_LOG_FATAL:
      cLog::log (LOGERROR,   "ffmpeg Fatal - %s ", str);
      break;
    case AV_LOG_ERROR:
      cLog::log (LOGERROR,   "ffmpeg Error - %s ", str);
      break;
    case AV_LOG_WARNING:
      cLog::log (LOGNOTICE,  "ffmpeg Warn  - %s ", str);
      break;
    case AV_LOG_INFO:
      cLog::log (LOGINFO,    "ffmpeg Info  - %s ", str);
      break;
    case AV_LOG_VERBOSE:
      cLog::log (LOGINFO,    "ffmpeg Verbo - %s ", str);
      break;
    case AV_LOG_DEBUG:
      cLog::log (LOGINFO,    "ffmpeg Debug - %s ", str);
      break;
    case AV_LOG_TRACE:
      cLog::log (LOGINFO,    "ffmpeg Trace - %s ", str);
      break;
    default :
      cLog::log (LOGERROR,   "ffmpeg ????? - %s ", str);
      break;
    }
  }
//}}}
//{{{
int main() {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "capture");
  av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (avLogCallback);

  cCaptureWASAPI capture;
  cAacWriter aacWriter ("D:/Capture/out.aac", kCaptureChannnels, 48000, 128000);
  cWavWriter wavWriter ("D:/Capture/out.wav", capture.mWaveFormatEx);

  // capture thread
  HANDLE hThread = CreateThread (NULL, 0, captureThread, &capture, 0, NULL );
  if (!hThread) {
    //{{{
    cLog::log (LOGERROR, "CreateThread failed: last error is %u", GetLastError());
    return 0;
    }
    //}}}

  const int frameSize = aacWriter.getFrameSize();
  cLog::log (LOGINFO, "capture and encode with frameSize:%d", frameSize);

  while (true) {
    auto framePtr = capture.getFrame (frameSize);
    if (framePtr) {
      // enough data to encode
      wavWriter.write (framePtr, frameSize);
      aacWriter.write (framePtr, frameSize);
      }
    else
      Sleep (10);
    }

  CloseHandle (hThread);
  CoUninitialize();

  return 0;
  }
//}}}
