// capture.cpp
//{{{  includes defines
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

#include "../shared/utils/cBipBuffer.h"

#define LOG(format, ...) wprintf (format L"\n", __VA_ARGS__)
#define ERR(format, ...) LOG (L"Error: " format, __VA_ARGS__)
//}}}
#define DEFAULT_FILE L"out.wav"

cBipBuffer bipBuffer;

#define CHANNELS 2
#define SAMPLE_RATE 44100
#define ENCODER_BITRATE 128000
//{{{
int writeData (void* file, uint8_t* data, int size) {

  fwrite (data, 1, size, (FILE*)file);
  return size;
  }
//}}}

//{{{
void sine() {

  FILE* file = fopen ("out.aac", "wb");

  av_register_all();

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
  AVIOContext* ioContext = avio_alloc_context (outBuffer, outBufferSize, 1, file, NULL, &writeData, NULL);

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

  // allocate the frame's data buffer
  av_frame_get_buffer (frame, 0);

  AVPacket* packet = av_packet_alloc();

  double t = 0.f;
  double inc = 2.0 * M_PI * 440.0 / encoderContext->sample_rate;
  for (int i = 0; i < 200; i++) {
    auto samples0 = (float*)frame->data[0];
    auto samples1 = (float*)frame->data[1];
    for (auto j = 0; j < encoderContext->frame_size; j++) {
      samples0[j] = float(sin(t));
      samples1[j] = float(sin(t));
      t += inc;
      }

    if (avcodec_send_frame (encoderContext, frame) == 0)
      while (avcodec_receive_packet (encoderContext, packet) == 0)
        if (av_write_frame (outputFormatContext, packet) < 0)
          exit(0);
    }

  // Flush cached packets
  if (avcodec_send_frame (encoderContext, NULL) == 0)
    while (avcodec_receive_packet (encoderContext, packet) == 0)
      if (av_write_frame (outputFormatContext, packet) < 0)
        exit(0);

  av_write_trailer (outputFormatContext);
  fclose (file);

  avcodec_free_context (&encoderContext);
  av_frame_free (&frame);
  avformat_free_context (outputFormatContext);
  av_freep (&ioContext);
  av_freep (&outBuffer);
  av_packet_free (&packet);

  return;
  }
//}}}

//{{{
class AudioClientStopOnExit {
public:
    AudioClientStopOnExit(IAudioClient *p) : m_p(p) {}
    ~AudioClientStopOnExit() {
        HRESULT hr = m_p->Stop();
        if (FAILED(hr)) {
            ERR(L"IAudioClient::Stop failed: hr = 0x%08x", hr);
        }
    }

private:
    IAudioClient *m_p;
};
//}}}
//{{{
class AvRevertMmThreadCharacteristicsOnExit {
public:
  AvRevertMmThreadCharacteristicsOnExit(HANDLE hTask) : m_hTask(hTask) {}

  ~AvRevertMmThreadCharacteristicsOnExit() {
    if (!AvRevertMmThreadCharacteristics(m_hTask)) {
      ERR(L"AvRevertMmThreadCharacteristics failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_hTask;
  };
//}}}
//{{{
class CancelWaitableTimerOnExit {
public:
  CancelWaitableTimerOnExit(HANDLE h) : m_h(h) {}

  ~CancelWaitableTimerOnExit() {
    if (!CancelWaitableTimer(m_h)) {
      ERR(L"CancelWaitableTimer failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}
//{{{
class CloseHandleOnExit {
public:
  CloseHandleOnExit(HANDLE h) : m_h(h) {}

  ~CloseHandleOnExit() {
    if (!CloseHandle(m_h)) {
      ERR(L"CloseHandle failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}
//{{{
class CoTaskMemFreeOnExit {
public:
  CoTaskMemFreeOnExit(PVOID p) : m_p(p) {}

  ~CoTaskMemFreeOnExit() {
    CoTaskMemFree(m_p);
    }

private:
  PVOID m_p;
  };
//}}}
//{{{
class CoUninitializeOnExit {
public:
  ~CoUninitializeOnExit() {
    CoUninitialize();
    }
  };
//}}}
//{{{
class PropVariantClearOnExit {
public:
    PropVariantClearOnExit(PROPVARIANT *p) : m_p(p) {}
    ~PropVariantClearOnExit() {
        HRESULT hr = PropVariantClear(m_p);
        if (FAILED(hr)) {
            ERR(L"PropVariantClear failed: hr = 0x%08x", hr);
        }
    }

private:
    PROPVARIANT *m_p;
};
//}}}
//{{{
class ReleaseOnExit {
public:
  ReleaseOnExit(IUnknown *p) : m_p(p) {}

  ~ReleaseOnExit() {
    m_p->Release();
    }

private:
  IUnknown *m_p;
  };
//}}}
//{{{
class SetEventOnExit {
public:
  SetEventOnExit(HANDLE h) : m_h(h) {}
  ~SetEventOnExit() {
    if (!SetEvent(m_h)) {
      ERR(L"SetEvent failed: last error is %d", GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}
//{{{
class WaitForSingleObjectOnExit {
public:
  WaitForSingleObjectOnExit(HANDLE h) : m_h(h) {}

  ~WaitForSingleObjectOnExit() {
    DWORD dwWaitResult = WaitForSingleObject(m_h, INFINITE);
    if (WAIT_OBJECT_0 != dwWaitResult) {
      ERR(L"WaitForSingleObject returned unexpected result 0x%08x, last error is %d", dwWaitResult, GetLastError());
      }
    }

private:
  HANDLE m_h;
  };
//}}}

//{{{
class CPrefs {
public:
  CPrefs() : m_MMDevice(NULL), m_hFile(NULL), m_bInt16(true), m_pwfx(NULL), m_szFilename(NULL) {

    list_devices();
    get_default_device (&m_MMDevice);
    m_szFilename = DEFAULT_FILE;
    open_file (m_szFilename, &m_hFile);
    }

  ~CPrefs() {
    if (NULL != m_MMDevice)
      m_MMDevice->Release();
    if (NULL != m_hFile)
       mmioClose(m_hFile, 0);
    if (NULL != m_pwfx)
      CoTaskMemFree(m_pwfx);
    }

  IMMDevice* m_MMDevice;
  HMMIO m_hFile;
  bool m_bInt16;
  PWAVEFORMATEX m_pwfx;
  LPCWSTR m_szFilename;

private:
  //{{{
  HRESULT list_devices() {

    // get an enumerator
    IMMDeviceEnumerator *pMMDeviceEnumerator;
    HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), (void**)&pMMDeviceEnumerator);
    if (FAILED (hr)) {
      //{{{
      ERR (L"CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x", hr);
      return hr;
      }
      //}}}
    ReleaseOnExit releaseMMDeviceEnumerator (pMMDeviceEnumerator);

    // get all the active render endpoints
    IMMDeviceCollection* pMMDeviceCollection;
    hr = pMMDeviceEnumerator->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection);
    if (FAILED (hr)) {
      //{{{
      ERR (L"IMMDeviceEnumerator::EnumAudioEndpoints failed: hr = 0x%08x", hr);
      return hr;
      }
      //}}}
    ReleaseOnExit releaseMMDeviceCollection (pMMDeviceCollection);

    UINT count;
    hr = pMMDeviceCollection->GetCount (&count);
    if (FAILED(hr)) {
      //{{{
      ERR (L"IMMDeviceCollection::GetCount failed: hr = 0x%08x", hr);
      return hr;
      }
      //}}}
    LOG (L"Active render endpoints found: %u", count);

    for (UINT i = 0; i < count; i++) {
      // get the "n"th device
      IMMDevice* pMMDevice;
      hr = pMMDeviceCollection->Item (i, &pMMDevice);
      if (FAILED (hr)) {
        //{{{
        ERR (L"IMMDeviceCollection::Item failed: hr = 0x%08x", hr);
        return hr;
        }
        //}}}
      ReleaseOnExit releaseMMDevice (pMMDevice);

      // open the property store on that device
      IPropertyStore* pPropertyStore;
      hr = pMMDevice->OpenPropertyStore (STGM_READ, &pPropertyStore);
      if (FAILED(hr)) {
        //{{{
        ERR(L"IMMDevice::OpenPropertyStore failed: hr = 0x%08x", hr);
        return hr;
        }
        //}}}
      ReleaseOnExit releasePropertyStore (pPropertyStore);

      // get the long name property
      PROPVARIANT pv; PropVariantInit (&pv);
      hr = pPropertyStore->GetValue (PKEY_Device_FriendlyName, &pv);
      if (FAILED (hr)) {
        //{{{
        ERR (L"IPropertyStore::GetValue failed: hr = 0x%08x", hr);
        return hr;
        }
        //}}}

      PropVariantClearOnExit clearPv (&pv);
      if (VT_LPWSTR != pv.vt) {
        //{{{
        ERR (L"PKEY_Device_FriendlyName variant type is %u - expected VT_LPWSTR", pv.vt);
        return E_UNEXPECTED;
        }
        //}}}

      LOG (L"    %ls", pv.pwszVal);
      }

    return S_OK;
    }
  //}}}
  //{{{
  HRESULT get_default_device (IMMDevice** ppMMDevice) {

    // activate a device enumerator
    IMMDeviceEnumerator* pMMDeviceEnumerator;
    HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), NULL,
                                   CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pMMDeviceEnumerator);
    if (FAILED (hr)) {
     //{{{
     ERR (L"CoCreateInstance (IMMDeviceEnumerator) failed: hr = 0x%08x", hr);
     return hr;
      }
     //}}}
    ReleaseOnExit releaseMMDeviceEnumerator (pMMDeviceEnumerator);

    // get the default render endpoint
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint (eRender, eConsole, ppMMDevice);
    if (FAILED (hr)) {
      //{{{
      ERR (L"IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x", hr);
      return hr;
      }
      //}}}

    return S_OK;
    }
  //}}}
  //{{{
  HRESULT open_file (LPCWSTR szFileName, HMMIO* phFile) {

    // some flags cause mmioOpen write to this buffer, but not any that we're using
    MMIOINFO mi = {0};
    *phFile = mmioOpen (const_cast<LPWSTR>(szFileName), &mi, MMIO_READWRITE | MMIO_CREATE);
    if (NULL == *phFile) {
      //{{{
      ERR (L"mmioOpen(\"%ls\", ...) failed. wErrorRet == %u", szFileName, mi.wErrorRet);
      return E_FAIL;
      }
      //}}}

    return S_OK;
    }
  //}}}
  };
//}}}

//{{{
HRESULT writeWaveHeader (HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO* pckRIFF, MMCKINFO* pckData) {

  // make a RIFF/WAVE chunk
  pckRIFF->ckid = MAKEFOURCC ('R', 'I', 'F', 'F');
  pckRIFF->fccType = MAKEFOURCC ('W', 'A', 'V', 'E');

  MMRESULT result = mmioCreateChunk(hFile, pckRIFF, MMIO_CREATERIFF);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioCreateChunk (\"RIFF/WAVE\") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // make a 'fmt ' chunk (within the RIFF/WAVE chunk)
  MMCKINFO chunk;
  chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
  result = mmioCreateChunk(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioCreateChunk (\"fmt \") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // write the WAVEFORMATEX data to it
  LONG lBytesInWfx = sizeof(WAVEFORMATEX) + pwfx->cbSize;
  LONG lBytesWritten = mmioWrite (hFile, reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(pwfx)), lBytesInWfx);
  if (lBytesWritten != lBytesInWfx) {
    //{{{
    ERR (L"mmioWrite (fmt data) wrote %u bytes; expected %u bytes", lBytesWritten, lBytesInWfx);
    return E_FAIL;
    }
    //}}}

  // ascend from the 'fmt ' chunk
  result = mmioAscend(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioAscend (\"fmt \" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // make a 'fact' chunk whose data is (DWORD)0
  chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
  result = mmioCreateChunk(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioCreateChunk (\"fmt \") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // write (DWORD)0 to it
  // this is cleaned up later
  DWORD frames = 0;
  lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
  if (lBytesWritten != sizeof(frames)) {
    //{{{
    ERR (L"mmioWrite(fact data) wrote %u bytes; expected %u bytes", lBytesWritten, (UINT32)sizeof(frames));
    return E_FAIL;
    }
    //}}}

  // ascend from the 'fact' chunk
  result = mmioAscend(hFile, &chunk, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioAscend (\"fact\" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  // make a 'data' chunk and leave the data pointer there
  pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
  result = mmioCreateChunk(hFile, pckData, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }
    //}}}

  return S_OK;
  }
//}}}
//{{{
HRESULT finishWaveFile (HMMIO hFile, MMCKINFO* pckRIFF, MMCKINFO* pckData, int frames) {

  MMRESULT result;

  result = mmioAscend (hFile, pckData, 0);
  if (MMSYSERR_NOERROR != result) {
    ERR (L"mmioAscend(\"data\" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }

  result = mmioAscend (hFile, pckRIFF, 0);
  if (MMSYSERR_NOERROR != result) {
    ERR (L"mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x", result);
    return E_FAIL;
    }

  result = mmioClose (hFile, 0);
  hFile = NULL;
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioClose failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  // everything went well... fixup the fact chunk in the file

  // reopen the file in read/write mode
  MMIOINFO mi = {0};
  hFile = mmioOpen (const_cast<LPWSTR>(DEFAULT_FILE), &mi, MMIO_READWRITE);
  if (NULL == hFile) {
    //{{{
    ERR (L"mmioOpen failed");
    return -__LINE__;
    }
    //}}}

  // descend into the RIFF/WAVE chunk
  MMCKINFO ckRIFF = {0};
  ckRIFF.ckid = MAKEFOURCC ('W', 'A', 'V', 'E'); // this is right for mmioDescend
  result = mmioDescend (hFile, &ckRIFF, NULL, MMIO_FINDRIFF);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioDescend(\"WAVE\") failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  // descend into the fact chunk
  MMCKINFO ckFact = {0};
  ckFact.ckid = MAKEFOURCC ('f', 'a', 'c', 't');
  result = mmioDescend (hFile, &ckFact, &ckRIFF, MMIO_FINDCHUNK);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioDescend(\"fact\") failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  // write frames to the fact chunk
  LONG bytesWritten = mmioWrite (hFile, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
  if (bytesWritten != sizeof (frames)) {
    //{{{
    ERR (L"Updating the fact chunk wrote %u bytes; expected %u", bytesWritten, (UINT32)sizeof(frames));
    return -__LINE__;
    }
    //}}}

  // ascend out of the fact chunk
  result = mmioAscend (hFile, &ckFact, 0);
  if (MMSYSERR_NOERROR != result) {
    //{{{
    ERR (L"mmioAscend(\"fact\") failed: MMSYSERR = %u", result);
    return -__LINE__;
    }
    //}}}

  return S_OK;
  }
//}}}

//{{{
struct sCaptureContext {
  IMMDevice* MMDevice;
  bool bInt16;

  HMMIO file;
  HANDLE stopEvent;
  UINT32 frames;

  HRESULT hr;
  };
//}}}
//{{{
void capture (sCaptureContext* context) {

  //{{{  activate an IAudioClient
  IAudioClient* audioClient;
  if (FAILED (context->MMDevice->Activate (__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient))) {
    ERR (L"IMMDevice::Activate (IAudioClient) failed");
    return;
    }

  ReleaseOnExit releaseAudioClient (audioClient);
  //}}}
  //{{{  get the default device periodicity
  REFERENCE_TIME hnsDefaultDevicePeriod;
  if (FAILED (audioClient->GetDevicePeriod (&hnsDefaultDevicePeriod, NULL))) {
    ERR (L"IAudioClient::GetDevicePeriod failed");
    return;
    }
  //}}}
  //{{{  get the default device format
  WAVEFORMATEX* waveFormatEx;
  if (FAILED (audioClient->GetMixFormat (&waveFormatEx))) {
    ERR (L"IAudioClient::GetMixFormat failed");
    return;
    }

  CoTaskMemFreeOnExit freeMixFormat (waveFormatEx);
  //}}}
  if (context->bInt16) {
    //{{{  coerce int16 waveFormat, in-place not changing size of format, engine auto-convert float to int
    switch (waveFormatEx->wFormatTag) {
      case WAVE_FORMAT_IEEE_FLOAT:
        printf ("WAVE_FORMAT_IEEE_FLOAT\n");

        waveFormatEx->wFormatTag = WAVE_FORMAT_PCM;
        waveFormatEx->wBitsPerSample = 16;
        waveFormatEx->nBlockAlign = waveFormatEx->nChannels * waveFormatEx->wBitsPerSample / 8;
        waveFormatEx->nAvgBytesPerSec = waveFormatEx->nBlockAlign * waveFormatEx->nSamplesPerSec;
        break;

      case WAVE_FORMAT_EXTENSIBLE: {
        printf ("WAVE_FORMAT_EXTENSIBLE\n");
        PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(waveFormatEx);
        if (IsEqualGUID (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
          printf ("- KSDATAFORMAT_SUBTYPE_IEEE_FLOAT\n");
          pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
          pEx->Samples.wValidBitsPerSample = 16;
          waveFormatEx->wBitsPerSample = 16;
          waveFormatEx->nBlockAlign = waveFormatEx->nChannels * waveFormatEx->wBitsPerSample / 8;
          waveFormatEx->nAvgBytesPerSec = waveFormatEx->nBlockAlign * waveFormatEx->nSamplesPerSec;
          }
        else {
          //{{{
          ERR (L"%s", L"Don't know how to coerce mix format to int-16");
          return;
          }
          //}}}
         }
        break;

      default:
        ERR (L"Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16", waveFormatEx->wFormatTag);
        return;
      }
    }
    //}}}

  MMCKINFO ckRIFF = {0};
  MMCKINFO ckData = {0};
  if (FAILED (writeWaveHeader (context->file, waveFormatEx, &ckRIFF, &ckData)))
    return;

  //{{{  create a periodic waitable timer
  HANDLE wakeUp = CreateWaitableTimer (NULL, FALSE, NULL);
  if (NULL == wakeUp) {
    ERR (L"CreateWaitableTimer failed: last error = %u", GetLastError());
    return;
    }

  CloseHandleOnExit closeWakeUp (wakeUp);
  //}}}
  //{{{  IAudioClient::Initialize
  // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK do not work together...
  // the "data ready" event never gets set so we're going to do a timer-driven loop
  if (FAILED (audioClient->Initialize (AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, waveFormatEx, 0))) {
    ERR (L"IAudioClient::Initialize failed");
    return;
    }
  //}}}
  //{{{  activate an IAudioCaptureClient
  IAudioCaptureClient* audioCaptureClient;
  if (FAILED (audioClient->GetService (__uuidof(IAudioCaptureClient), (void**)&audioCaptureClient))) {
    ERR (L"IAudioClient::GetService (IAudioCaptureClient) failed");
    return;
    }

  ReleaseOnExit releaseAudioCaptureClient (audioCaptureClient);
  //}}}
  //{{{  register with MMCSS
  DWORD nTaskIndex = 0;
  HANDLE hTask = AvSetMmThreadCharacteristics (L"Audio", &nTaskIndex);
  if (NULL == hTask) {
    ERR (L"AvSetMmThreadCharacteristics failed: last error = %u", GetLastError());
    return;
    }

  AvRevertMmThreadCharacteristicsOnExit unregisterMmcss (hTask);
  //}}}
  //{{{  set the waitable timer
  LARGE_INTEGER liFirstFire;
  liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
  LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds

  if (!SetWaitableTimer (wakeUp, &liFirstFire, lTimeBetweenFires, NULL, NULL, FALSE)) {
    ERR (L"SetWaitableTimer failed: last error = %u", GetLastError());
    return;
    }

  CancelWaitableTimerOnExit cancelWakeUp (wakeUp);
  //}}}
  //{{{  IAudioClient::Start
  if (FAILED (audioClient->Start())) {
    ERR (L"IAudioClient::Start failed");
    return;
    }

  AudioClientStopOnExit stopAudioClient (audioClient);
  //}}}

  // loopback capture loop
  context->frames = 0;
  HANDLE waitArray[2] = { context->stopEvent, wakeUp };
  bool done = false;
  while (!done) {
    UINT32 packetSize;
    audioCaptureClient->GetNextPacketSize (&packetSize);
    while (packetSize > 0) {
      BYTE* pData;
      UINT32 numFramesToRead;
      DWORD dwFlags;
      if (FAILED (audioCaptureClient->GetBuffer (&pData, &numFramesToRead, &dwFlags, NULL, NULL))) {
        //{{{
        ERR (L"IAudioCaptureClient::GetBuffer failed");
        return;
        }
        //}}}
      if ((context->frames == 0) && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
        //{{{
        LOG (L"%s", L"glitch on first packet");
        }
        //}}}
      else if (dwFlags != 0) {
        //{{{
        LOG (L"IAudioCaptureClient::GetBuffer flags 0x%08x", dwFlags);
        return;
        }
        //}}}
      if (numFramesToRead == 0) {
        //{{{
        ERR (L"IAudioCaptureClient::GetBuffer read 0 frames");
        return;
        }
        //}}}

      //printf ("numFrames %d %d %d bytes:%d\n",
      //  context->frames, numFramesToRead, waveFormatEx->nBlockAlign, numFramesToRead * waveFormatEx->nBlockAlign);

      LONG bytesToWrite = numFramesToRead * waveFormatEx->nBlockAlign;

      int bytesAllocated = 0;
      uint8_t* ptr = bipBuffer.reserve (bytesToWrite, bytesAllocated);
      if (ptr && (bytesAllocated == bytesToWrite)) {
        memcpy (ptr, pData, bytesAllocated);
        bipBuffer.commit (bytesAllocated);
        }

      LONG bytesWritten = mmioWrite (context->file, reinterpret_cast<PCHAR>(pData), bytesToWrite);
      if (bytesWritten != bytesToWrite) {
        //{{{
        ERR (L"mmioWrite wrote %u bytes expected %u bytes", bytesWritten, bytesToWrite);
        return;
        }
        //}}}
      if (FAILED (audioCaptureClient->ReleaseBuffer (numFramesToRead))) {
        //{{{
        ERR (L"IAudioCaptureClient::ReleaseBuffer failed");
        return;
        }
        //}}}





      context->frames += numFramesToRead;
      audioCaptureClient->GetNextPacketSize (&packetSize);
      }

    DWORD dwWaitResult = WaitForMultipleObjects (ARRAYSIZE(waitArray), waitArray, FALSE, INFINITE);
    if (WAIT_OBJECT_0 == dwWaitResult) {
      //{{{
      LOG (L"Received stop event");
      done = true;
      }
      //}}}
    else if (WAIT_OBJECT_0 + 1 != dwWaitResult) {
      //{{{
      ERR (L"Unexpected WaitForMultipleObjects return value %u", dwWaitResult);
      return;
      }
      //}}}
    }

  finishWaveFile (context->file, &ckData, &ckRIFF, context->frames);
  }
//}}}
//{{{
DWORD WINAPI captureThread (LPVOID context) {

  sCaptureContext* captureContext = (sCaptureContext*)context;

  captureContext->hr = CoInitialize (NULL);
  if (FAILED (captureContext->hr)) {
    ERR (L"CoInitialize failed: hr = 0x%08x", captureContext->hr);
    return 0;
    }
  CoUninitializeOnExit cuoe;

  capture (captureContext);

  return 0;
  }
//}}}
//{{{
DWORD WINAPI readThread (LPVOID context) {

  sCaptureContext* captureContext = (sCaptureContext*)context;

  CoInitialize (NULL);
  CoUninitializeOnExit cuoe;

  FILE* file = fopen ("out.aac", "wb");

  av_register_all();

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
  AVIOContext* ioContext = avio_alloc_context (outBuffer, outBufferSize, 1, file, NULL, &writeData, NULL);

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

  // allocate the frame's data buffer
  av_frame_get_buffer (frame, 0);

  AVPacket* packet = av_packet_alloc();

  while (true) {
    int len = encoderContext->frame_size;
    auto ptr = (int16_t*)bipBuffer.getContiguousBlock (len);
    if (len >= encoderContext->frame_size) {
      printf ("read block %d\n", len);
      auto samples0 = (float*)frame->data[0];
      auto samples1 = (float*)frame->data[1];
      for (auto j = 0; j < encoderContext->frame_size; j++) {
        samples0[j] = float(*ptr++) / 0x10000;
        samples1[j] = float(*ptr++) / 0x10000;
        }
      bipBuffer.decommitBlock (len);

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
  fclose (file);

  avcodec_free_context (&encoderContext);
  av_frame_free (&frame);
  avformat_free_context (outputFormatContext);
  av_freep (&ioContext);
  av_freep (&outBuffer);
  av_packet_free (&packet);

  return 0;
  }
//}}}

//{{{
int _cdecl wmain (int argc, LPCWSTR argv[]) {

  HRESULT hr = CoInitialize (NULL);
  if (FAILED(hr)) {
    //{{{
    ERR(L"CoInitialize failed: hr = 0x%08x", hr);
    return -__LINE__;
    }
    //}}}

  CoUninitializeOnExit cuoe;

  int a = argc;
  LPCWSTR nn = argv[0];
  printf ("%d %ls", a, nn);

  bipBuffer.allocateBuffer (1024 * 32);

  // parse command line
  CPrefs prefs;

  // create a "stop capturing now" event
  HANDLE stopEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
  if (NULL == stopEvent) {
    //{{{
    ERR (L"CreateEvent failed: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}
  CloseHandleOnExit closeStopEvent (stopEvent);

  // create arguments for loopback capture thread
  sCaptureContext captureContext;
  captureContext.MMDevice = prefs.m_MMDevice;
  captureContext.bInt16 = prefs.m_bInt16;
  captureContext.file = prefs.m_hFile;
  captureContext.stopEvent = stopEvent;
  captureContext.frames = 0;
  captureContext.hr = E_UNEXPECTED; // thread will overwrite this

  HANDLE hReadThread = CreateThread (NULL, 0, readThread, &captureContext, 0, NULL );
  if (hReadThread == NULL) {
    //{{{
    ERR (L"CreateThread failed: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}
  CloseHandleOnExit closeThread1 (hReadThread);

  HANDLE hThread = CreateThread (NULL, 0, captureThread, &captureContext, 0, NULL );
  if (hThread == NULL) {
    //{{{
    ERR (L"CreateThread failed: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}
  CloseHandleOnExit closeThread (hThread);

  // at this point capture is running .wait for the user to press a key or for capture to error out
  WaitForSingleObjectOnExit waitForThread (hThread);
  SetEventOnExit setStopEvent (stopEvent);
  HANDLE hStdIn = GetStdHandle (STD_INPUT_HANDLE);
  if (INVALID_HANDLE_VALUE == hStdIn) {
    //{{{
    ERR (L"GetStdHandle returned INVALID_HANDLE_VALUE: last error is %u", GetLastError());
    return -__LINE__;
    }
    //}}}

  HANDLE rhHandles[2] = { hThread, hStdIn };
  bool bKeepWaiting = true;
  while (bKeepWaiting) {
    auto dwWaitResult = WaitForMultipleObjects (2, rhHandles, FALSE, INFINITE);
    switch (dwWaitResult) {
      case WAIT_OBJECT_0: // hThread
        ERR (L"%s", L"The thread terminated early - something bad happened");
        bKeepWaiting = false;
        break;

      case WAIT_OBJECT_0 + 1: // hStdIn
        // see if any of them was an Enter key-up event
        INPUT_RECORD rInput[128];
        DWORD nEvents;
        if (!ReadConsoleInput (hStdIn, rInput, ARRAYSIZE(rInput), &nEvents)) {
          //{{{
          ERR (L"ReadConsoleInput failed: last error is %u", GetLastError());
          bKeepWaiting = false;
          }
          //}}}
        else {
          for (DWORD i = 0; i < nEvents; i++) {
            if (KEY_EVENT == rInput[i].EventType &&
                VK_RETURN == rInput[i].Event.KeyEvent.wVirtualKeyCode && !rInput[i].Event.KeyEvent.bKeyDown) {
              //{{{
              LOG (L"%s", L"Stopping capture...");
              bKeepWaiting = false;
              break;
              }
              //}}}
              }
            // if none of them were Enter key-up events continue waiting
            }
          break;

      default:
        ERR (L"WaitForMultipleObjects returned unexpected value 0x%08x", dwWaitResult);
        bKeepWaiting = false;
        break;
      }
    }

  // let prefs' destructor call mmioClose
  return 0;
  }
//}}}
