// libstdaudio
//{{{
// Copyright (c) 2018 - Timur Doumler
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
//}}}
#pragma once
//{{{  includes
#define NOMINMAX

#include <optional>
#include <cassert>
#include <chrono>
#include <cctype>
#include <codecvt>

#include <string>
#include <iostream>
#include <vector>

#include <functional>
#include <thread>
#include <forward_list>
#include <atomic>
#include <string_view>

#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <variant>
#include <array>
//}}}

namespace audio {
  struct sContiguousInterleaved {};
  inline constexpr sContiguousInterleaved contiguousInterleaved;

  struct sContiguousDeinterleaved {};
  inline constexpr sContiguousDeinterleaved contiguousDeinterleaved;

  struct sPtrToPtrDeinterleaved {};
  inline constexpr sPtrToPtrDeinterleaved ptrToPtrDeinterleaved;

  //{{{
  template <typename SampleType> class cAudioBuffer {
  public:
    using sampleType = SampleType;
    using indexType = size_t;

    //{{{
    cAudioBuffer (sampleType* data, indexType numFrames, indexType numChannels, sContiguousInterleaved)
        : mNumFrames(numFrames), mNumChannels(numChannels), mStride(mNumChannels), mIsContiguous(true) {

      assert (numChannels <= mMaxNumChannels);
      for (auto i = 0; i < mNumChannels; ++i)
        mChannels[i] = data + i;
      }
    //}}}
    //{{{
    cAudioBuffer (sampleType* data, indexType numFrames, indexType numChannels, sContiguousDeinterleaved)
        : mNumFrames(numFrames), mNumChannels(numChannels), mStride(1), mIsContiguous(true) {

      assert (numChannels <= mMaxNumChannels);
      for (auto i = 0; i < mNumChannels; ++i)
        mChannels[i] = data + (i * mNumFrames);
      }
    //}}}
    //{{{
    cAudioBuffer (sampleType** data, indexType numFrames, indexType numChannels, sPtrToPtrDeinterleaved)
        : mNumFrames(numFrames), mNumChannels(numChannels), mStride(1), mIsContiguous(false) {

      assert (numChannels <= mMaxNumChannels);
      copy (data, data + mNumChannels, mChannels.begin());
      }
    //}}}

    sampleType* data() const noexcept { return mIsContiguous ? mChannels[0] : nullptr; }

    bool isContiguous() const noexcept { return mIsContiguous; }
    bool areFramesContiguous() const noexcept { return mStride == mNumChannels; }
    bool areChannelsContiguous() const noexcept { return mStride == 1; }

    indexType getSizeFrames() const noexcept { return mNumFrames; }
    indexType getSizeChannels() const noexcept { return mNumChannels; }
    indexType getSizeSamples() const noexcept { return mNumChannels * mNumFrames; }

    //{{{
    sampleType& operator() (indexType frame, indexType channel) noexcept {

      return const_cast<sampleType&>(std::as_const(*this).operator()(frame, channel));
      }
    //}}}
    //{{{
    const sampleType& operator() (indexType frame, indexType channel) const noexcept {

      return mChannels[channel][frame * mStride];
      }
    //}}}

  private:
    bool mIsContiguous = false;

    indexType mNumFrames = 0;
    indexType mNumChannels = 0;
    indexType mStride = 0;

    constexpr static size_t mMaxNumChannels = 16;
    std::array<sampleType*, mMaxNumChannels> mChannels = {};
    };
  //}}}
  //{{{
  template <typename SampleType> struct sAudioDeviceIo {

    std::optional <cAudioBuffer <SampleType>> inputBuffer;
    std::optional <std::chrono::time_point <std::chrono::steady_clock>> inputTime;

    std::optional <cAudioBuffer<SampleType>> outputBuffer;
    std::optional <std::chrono::time_point <std::chrono::steady_clock>> outputTime;
    };
  //}}}

  //{{{
  class cWaspiUtil {
  public:
    //{{{
    static const CLSID& getMMDeviceEnumeratorClassId() {

      static const CLSID MMDeviceEnumerator_class_id = __uuidof(MMDeviceEnumerator);
      return MMDeviceEnumerator_class_id;
      }
    //}}}
    //{{{
    static const IID& getIMMDeviceEnumeratorInterfaceId() {

      static const IID IMMDeviceEnumerator_interface_id = __uuidof(IMMDeviceEnumerator);
      return IMMDeviceEnumerator_interface_id;
      }
    //}}}
    //{{{
    static const IID& getIAudioClientInterfaceId() {

      static const IID IAudioClient_interface_id = __uuidof(IAudioClient);
      return IAudioClient_interface_id;
      }
    //}}}
    //{{{
    static const IID& getIAudioRenderClientInterfaceId() {

      static const IID IAudioRenderClient_interface_id = __uuidof(IAudioRenderClient);
      return IAudioRenderClient_interface_id;
      }
    //}}}
    //{{{
    static const IID& getIAudioCaptureClientInterfaceId() {

      static const IID IAudioCaptureClient_interface_id = __uuidof(IAudioCaptureClient);
      return IAudioCaptureClient_interface_id;
      }
    //}}}

    //{{{
    class cComInitializer {
    public:
      cComInitializer() : _hr (CoInitialize (nullptr)) { }

      ~cComInitializer() {
        if (SUCCEEDED (_hr))
          CoUninitialize();
        }

      operator HRESULT() const { return _hr; }

      HRESULT _hr;
      };
    //}}}
    //{{{
    template<typename T> class cAutoRelease {
    public:
      cAutoRelease (T*& value) : _value(value) {}

      ~cAutoRelease() {
        if (_value != nullptr)
          _value->Release();
        }

    private:
      T*& _value;
      };
    //}}}

    //{{{
    static std::string convertString (const wchar_t* wide_string) {

      int required_characters = WideCharToMultiByte (CP_UTF8, 0, wide_string,
                                                     -1, nullptr, 0, nullptr, nullptr);
      if (required_characters <= 0)
        return {};

      std::string output;
      output.resize (static_cast<size_t>(required_characters));
      WideCharToMultiByte (CP_UTF8, 0, wide_string, -1,
                           output.data(), static_cast<int>(output.size()), nullptr, nullptr);

      return output;
      }
    //}}}
    //{{{
    static std::string convertString (const std::wstring& input) {

      int required_characters = WideCharToMultiByte (CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                                                     nullptr, 0, nullptr, nullptr);
      if (required_characters <= 0)
        return {};

      std::string output;
      output.resize (static_cast<size_t>(required_characters));
      WideCharToMultiByte (CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                           output.data(), static_cast<int>(output.size()), nullptr, nullptr);

      return output;
      }
    //}}}
    };
  //}}}
  //{{{
  struct sAudioDeviceException : public std::runtime_error {
    explicit sAudioDeviceException (const char* what) : runtime_error(what) { }
    };
  //}}}
  //{{{
  class cAudioDevice {
  public:
    cAudioDevice() = delete;
    cAudioDevice (const cAudioDevice&) = delete;
    cAudioDevice& operator= (const cAudioDevice&) = delete;

    //{{{
    cAudioDevice (cAudioDevice&& other) :
      mDevice(other.mDevice),
      _audio_client(other._audio_client),
      _audio_capture_client(other._audio_capture_client),
      _audio_render_client(other._audio_render_client),
      _event_handle(other._event_handle),
      mDeviceId(std::move(other.mDeviceId)),
      _running(other._running.load()),
      mName(std::move(other.mName)),
      mMixFormat(other.mMixFormat),
      _processing_thread(std::move(other._processing_thread)),
      _buffer_frame_count(other._buffer_frame_count),
      mIsRenderDevice(other.mIsRenderDevice),
      mStopCallback(std::move(other.mStopCallback)),
      mUserCallback(std::move(other.mUserCallback))
    {
      other.mDevice = nullptr;
      other._audio_client = nullptr;
      other._audio_capture_client = nullptr;
      other._audio_render_client = nullptr;
      other._event_handle = nullptr;
    }
    //}}}
    //{{{
    cAudioDevice& operator= (cAudioDevice&& other) noexcept {

      if (this == &other)
        return *this;

      mDevice = other.mDevice;
      _audio_client = other._audio_client;
      _audio_capture_client = other._audio_capture_client;
      _audio_render_client = other._audio_render_client;
      _event_handle = other._event_handle;
      mDeviceId = std::move(other.mDeviceId);
      _running = other._running.load();
      mName = std::move(other.mName);
      mMixFormat = other.mMixFormat;
      _processing_thread = std::move(other._processing_thread);
      _buffer_frame_count = other._buffer_frame_count;
      mIsRenderDevice = other.mIsRenderDevice;
      mStopCallback = std::move (other.mStopCallback);
      mUserCallback = std::move (other.mUserCallback);

      other.mDevice = nullptr;
      other._audio_client = nullptr;
      other._audio_capture_client = nullptr;
      other._audio_render_client = nullptr;
      other._event_handle = nullptr;
    }
    //}}}
    //{{{
    ~cAudioDevice() {

      stop();

      if (_audio_capture_client != nullptr)
        _audio_capture_client->Release();

      if (_audio_render_client != nullptr)
        _audio_render_client->Release();

      if (_audio_client != nullptr)
        _audio_client->Release();

      if (mDevice != nullptr)
        mDevice->Release();
      }
    //}}}

    std::string_view getName() const noexcept { return mName; }

    using deviceId_t = std::wstring;
    deviceId_t getDeviceId() const noexcept { return mDeviceId; }

    bool isInput() const noexcept { return mIsRenderDevice == false; }
    bool isOutput() const noexcept { return mIsRenderDevice == true; }

    //{{{
    int getNumInputChannels() const noexcept {

      if (isInput() == false)
        return 0;

      return mMixFormat.Format.nChannels;
      }
    //}}}
    //{{{
    int getNumOutputChannels() const noexcept {

      if (isOutput() == false)
        return 0;

      return mMixFormat.Format.nChannels;
      }
    //}}}

    using sampleRate_t = DWORD;
    sampleRate_t getSampleRate() const noexcept { return mMixFormat.Format.nSamplesPerSec; }
    //{{{
    bool setSampleRate (sampleRate_t sampleRate) {
      mMixFormat.Format.nSamplesPerSec = sampleRate;
      fixupMixFormat();
      return true;
      }
    //}}}

    using buffer_size_t = UINT32;
    buffer_size_t getBufferSizeFrames() const noexcept { return _buffer_frame_count; }
    //{{{
    bool setBufferSizeFrames (buffer_size_t bufferSize) {
      _buffer_frame_count = bufferSize;
      return true;
      }
    //}}}

    //{{{
    template <typename SampleType> constexpr bool supports_sampleType() const noexcept {

      return is_same_v<SampleType, float> || is_same_v<SampleType, int32_t> || is_same_v<SampleType, int16_t>;
      }
    //}}}
    //{{{
    template <typename SampleType> bool setSampleType() {

      if (_is_connected() && !is_sampleType<SampleType>())
        throw sAudioDeviceException ("Cannot change sample type after connecting a callback.");

      return _set_sampleType_helper<SampleType>();
      }
    //}}}
    //{{{
    template <typename SampleType> bool isSampleType() const {

      return mMixFormat_matchesType<SampleType>();
      }
    //}}}
    constexpr bool canConnect() const noexcept { return true; }
    constexpr bool canProcess() const noexcept { return true; }

    //{{{  template float void connect (CallbackType callback)
    template <typename CallbackType, std::enable_if_t<std::is_nothrow_invocable_v<CallbackType, cAudioDevice&, sAudioDeviceIo<float>&>, int> = 0>
    void connect (CallbackType callback) {

      setSampleTypeHelper<float>();
      connectHelper(wasapi_float_callback_t { callback } );
      }
    //}}}
    //{{{  template int32_t void connect (CallbackType callback
    template <typename CallbackType, std::enable_if_t<std::is_nothrow_invocable_v<CallbackType, cAudioDevice&, sAudioDeviceIo<int32_t>&>, int> = 0>
    void connect (CallbackType callback) {

      setSampleTypeHelper<int32_t>();
      connectHelper (wasapi_int32_callback_t { callback } );
      }
    //}}}
    //{{{  template int16_t void connect (CallbackType callback
    template <typename CallbackType, std::enable_if_t<std::is_nothrow_invocable_v<CallbackType, cAudioDevice&, sAudioDeviceIo<int16_t>&>, int> = 0>
    void connect (CallbackType callback) {

      setSampleTypeHelper<int16_t>();
      connectHelper (wasapi_int16_callback_t { callback } );
      }
    //}}}

    // TODO: remove std::function as soon as C++20 default-ctable lambda and lambda in unevaluated contexts become available
    using no_op_t = std::function<void (cAudioDevice&)>;
    //{{{  template bool start (
    // TODO: is_nothrow_invocable_t does not compile, temporarily replaced with is_invocable_t
    template <typename StartCallbackType = no_op_t,
              typename StopCallbackType = no_op_t,
              typename = std::enable_if_t <std::is_invocable_v <StartCallbackType, cAudioDevice&> &&
                                           std::is_invocable_v <StopCallbackType, cAudioDevice&>> >
    bool start (StartCallbackType&& start_callback = [](cAudioDevice&) noexcept {},
                StopCallbackType&& stop_callback = [](cAudioDevice&) noexcept {}) {

      if (_audio_client == nullptr)
        return false;

      if (!_running) {
        _event_handle = CreateEvent (nullptr, FALSE, FALSE, nullptr);
        if (_event_handle == nullptr)
          return false;

        REFERENCE_TIME periodicity = 0;
        const REFERENCE_TIME ref_times_per_second = 10'000'000;
        REFERENCE_TIME buffer_duration = (ref_times_per_second * _buffer_frame_count) / mMixFormat.Format.nSamplesPerSec;
        HRESULT hr = _audio_client->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                                AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                buffer_duration, periodicity, &mMixFormat.Format, nullptr);

        // TODO: Deal with AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED return code by resetting the buffer_duration and retrying:
        // https://docs.microsoft.com/en-us/windows/desktop/api/audioclient/nf-audioclient-iaudioclient-initialize
        if (FAILED(hr))
          return false;

        /*HRESULT render_hr =*/ _audio_client->GetService (cWaspiUtil::getIAudioRenderClientInterfaceId(), reinterpret_cast<void**>(&_audio_render_client));
        /*HRESULT capture_hr =*/ _audio_client->GetService (cWaspiUtil::getIAudioCaptureClientInterfaceId(), reinterpret_cast<void**>(&_audio_capture_client));

        // TODO: Make sure to clean up more gracefully from errors
        hr = _audio_client->GetBufferSize(&_buffer_frame_count);
        if (FAILED (hr))
          return false;
        hr = _audio_client->SetEventHandle (_event_handle);
        if (FAILED (hr))
          return false;
        hr = _audio_client->Start();
        if (FAILED (hr))
          return false;

        _running = true;

        if (!mUserCallback.valueless_by_exception()) {
          _processing_thread = std::thread { [this]() {
            SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
            while (_running) {
              visit([this](auto&& callback) { if (callback) process(callback); }, mUserCallback);
              wait();
              }
            } };
          }

        start_callback (*this);
        mStopCallback = stop_callback;
        }

      return true;
      }
    //}}}
    //{{{
    bool stop() {

      if (_running) {
        _running = false;

        if (_processing_thread.joinable())
          _processing_thread.join();
        if (_audio_client != nullptr)
          _audio_client->Stop();
        if (_event_handle != nullptr)
          CloseHandle (_event_handle);

        mStopCallback (*this);
        }

      return true;
      }
    //}}}

    bool isRunning() const noexcept { return _running; }
    void wait() const { WaitForSingleObject (_event_handle, INFINITE); }

    //{{{  template void float process (const CallbackType& callback
    template <typename CallbackType,
              std::enable_if_t<std::is_invocable_v<CallbackType, cAudioDevice&, sAudioDeviceIo<float>&>, int> = 0>
    void process (const CallbackType& callback) {
      if (!mixFormatMatchesType<float>())
        throw sAudioDeviceException ("Attempting to process a callback for a sample type that does not match the configured sample type.");
      processHelper<float>(callback);
      }
    //}}}
    //{{{  template void int32 process (const CallbackType& callback
    template <typename CallbackType,
              std::enable_if_t<std::is_invocable_v<CallbackType, cAudioDevice&, sAudioDeviceIo<int32_t>&>, int> = 0>
    void process (const CallbackType& callback) {
      if (!mixFormatMatchesType<int32_t>())
        throw sAudioDeviceException ("Attempting to process a callback for a sample type that does not match the configured sample type.");
      processHelper<int32_t>(callback);
      }
    //}}}
    //{{{  template void int16_t process (const CallbackType& callback
    template <typename CallbackType,
             std::enable_if_t<std::is_invocable_v<CallbackType, cAudioDevice&, sAudioDeviceIo<int16_t>&>, int> = 0>
    void process (const CallbackType& callback) {
      if (!mixFormatMatchesType<int16_t>())
        throw sAudioDeviceException ("Attempting to process a callback for a sample type that does not match the configured sample type.");

      processHelper<int16_t>(callback);
      }
    //}}}
    //{{{
    bool hasUnprocessedIo() const noexcept {

      if (_audio_client == nullptr)
        return false;

      if (!_running)
        return false;

      UINT32 current_padding = 0;
      _audio_client->GetCurrentPadding (&current_padding);

      auto num_frames_available = _buffer_frame_count - current_padding;
      return num_frames_available > 0;
      }
    //}}}

  private:
    friend class cAudioDeviceEnumerator;
    //{{{
    cAudioDevice (IMMDevice* device, bool isRenderDevice) :
        mDevice(device), mIsRenderDevice(isRenderDevice) {

      // TODO: Handle errors better.  Maybe by throwing exceptions?
      if (mDevice == nullptr)
        throw sAudioDeviceException("IMMDevice is null.");

      initDeviceIdName();
      if (mDeviceId.empty())
        throw sAudioDeviceException("Could not get device id.");

      if (mName.empty())
        throw sAudioDeviceException("Could not get device name.");

      initAudioClient();
      if (_audio_client == nullptr)
        return;

      initMixFormat();
      }
    //}}}

    //{{{
    void initDeviceIdName() {

      LPWSTR deviceId = nullptr;
      HRESULT hr = mDevice->GetId (&deviceId);
      if (SUCCEEDED (hr)) {
        mDeviceId = deviceId;
        CoTaskMemFree (deviceId);
        }

      IPropertyStore* property_store = nullptr;
      cWaspiUtil::cAutoRelease auto_release_property_store { property_store };

      hr = mDevice->OpenPropertyStore (STGM_READ, &property_store);
      if (SUCCEEDED(hr)) {
        PROPVARIANT property_variant;
        PropVariantInit (&property_variant);

        auto try_acquire_name = [&](const auto& property_name) {
          hr = property_store->GetValue (property_name, &property_variant);
          if (SUCCEEDED(hr)) {
            mName = cWaspiUtil::convertString (property_variant.pwszVal);
            return true;
            }

          return false;
          };

        try_acquire_name (PKEY_Device_FriendlyName) ||
          try_acquire_name (PKEY_DeviceInterface_FriendlyName) ||
            try_acquire_name (PKEY_Device_DeviceDesc);

        PropVariantClear (&property_variant);
        }
      }
    //}}}
    //{{{
    void initAudioClient() {

      HRESULT hr = mDevice->Activate (cWaspiUtil::getIAudioClientInterfaceId(), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&_audio_client));
      if (FAILED(hr))
        return;
      }
    //}}}
    //{{{
    void initMixFormat() {

      WAVEFORMATEX* deviceMixFormat;
      HRESULT hr = _audio_client->GetMixFormat (&deviceMixFormat);
      if (FAILED (hr))
        return;

      auto* deviceMixFormatEx = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(deviceMixFormat);
      mMixFormat = *deviceMixFormatEx;

      CoTaskMemFree (deviceMixFormat);
      }
    //}}}
    //{{{
    void fixupMixFormat() {
      mMixFormat.Format.nBlockAlign = mMixFormat.Format.nChannels * mMixFormat.Format.wBitsPerSample / 8;
      mMixFormat.Format.nAvgBytesPerSec = mMixFormat.Format.nSamplesPerSec * mMixFormat.Format.wBitsPerSample * mMixFormat.Format.nChannels / 8;
      }
    //}}}

    //{{{
    template <typename CallbackType> void connectHelper (CallbackType callback) {

      if (_running)
        throw sAudioDeviceException ("Cannot connect to running audio_device.");

      mUserCallback = move (callback);
      }
    //}}}
    //{{{
    template <typename SampleType> bool mixFormatMatchesType() const noexcept {

      if constexpr (std::is_same_v<SampleType, float>)
        return mMixFormat.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

      else if constexpr (std::is_same_v<SampleType, int32_t>)
        return (mMixFormat.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) &&
               (mMixFormat.Format.wBitsPerSample == sizeof(int32_t) * 8);

      else if constexpr (std::is_same_v<SampleType, int16_t>)
        return (mMixFormat.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) &&
               (mMixFormat.Format.wBitsPerSample == sizeof(int16_t) * 8);

      else
        return false;
      }
    //}}}
    //{{{
    template <typename SampleType, typename CallbackType> void processHelper (const CallbackType& callback) {

      if (_audio_client == nullptr)
        return;

      if (!mixFormatMatchesType <SampleType>())
        return;

      if (isOutput()) {
        UINT32 current_padding = 0;
        _audio_client->GetCurrentPadding (&current_padding);

        auto num_frames_available = _buffer_frame_count - current_padding;
        if (num_frames_available == 0)
          return;

        BYTE* data = nullptr;
        _audio_render_client->GetBuffer (num_frames_available, &data);
        if (data == nullptr)
          return;

        sAudioDeviceIo<SampleType> device_io;
        device_io.outputBuffer = { reinterpret_cast<SampleType*>(data), num_frames_available, mMixFormat.Format.nChannels, contiguousInterleaved };
        callback (*this, device_io);

        _audio_render_client->ReleaseBuffer (num_frames_available, 0);
        }

      else if (isInput()) {
        UINT32 next_packet_size = 0;
        _audio_capture_client->GetNextPacketSize (&next_packet_size);
        if (next_packet_size == 0)
          return;

        // TODO: Support device position.
        DWORD flags = 0;
        BYTE* data = nullptr;
        _audio_capture_client->GetBuffer (&data, &next_packet_size, &flags, nullptr, nullptr);
        if (data == nullptr)
          return;

        sAudioDeviceIo<SampleType> device_io;
        device_io.inputBuffer = { reinterpret_cast<SampleType*>(data), next_packet_size, mMixFormat.Format.nChannels, contiguousInterleaved };
        callback (*this, device_io);

        _audio_capture_client->ReleaseBuffer (next_packet_size);
        }
      }
    //}}}
    //{{{
    template <typename SampleType> bool setSampleTypeHelper() {

      if constexpr (std::is_same_v<SampleType, float>)
        mMixFormat.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

      else if constexpr (std::is_same_v<SampleType, int32_t>)
        mMixFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

      else if constexpr (std::is_same_v<SampleType, int16_t>)
        mMixFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

      else
        return false;

      mMixFormat.Format.wBitsPerSample = sizeof(SampleType) * 8;
      mMixFormat.Samples.wValidBitsPerSample = mMixFormat.Format.wBitsPerSample;
      fixupMixFormat();

      return true;
      }
    //}}}
    //{{{
    bool isConnected() const noexcept {

      if (mUserCallback.valueless_by_exception())
        return false;

      return visit([](auto&& callback) { return static_cast<bool>(callback); }, mUserCallback);
      }
    //}}}

    IMMDevice* mDevice = nullptr;
    IAudioClient* _audio_client = nullptr;
    IAudioCaptureClient* _audio_capture_client = nullptr;
    IAudioRenderClient* _audio_render_client = nullptr;
    HANDLE _event_handle;

    std::wstring mDeviceId;
    std::atomic<bool> _running = false;
    std::string mName;

    WAVEFORMATEXTENSIBLE mMixFormat;
    std::thread _processing_thread;
    UINT32 _buffer_frame_count = 0;
    bool mIsRenderDevice = true;

    std::function <void (cAudioDevice&)> mStopCallback;

    using wasapi_float_callback_t = std::function <void (cAudioDevice&, sAudioDeviceIo<float>&) >;
    using wasapi_int32_callback_t = std::function <void (cAudioDevice&, sAudioDeviceIo<int32_t>&) >;
    using wasapi_int16_callback_t = std::function <void (cAudioDevice&, sAudioDeviceIo<int16_t>&) >;
    std::variant <wasapi_float_callback_t, wasapi_int32_callback_t, wasapi_int16_callback_t> mUserCallback;

    cWaspiUtil::cComInitializer mComInitializer;
    };
  //}}}

  enum class cAudioDeviceListEvent { eListChanged, eDefaultInputChanged, eDefaultOutputChanged, };
  template <typename F, typename = std::enable_if_t<std::is_invocable_v<F>>> void setAudioDeviceListCallback (cAudioDeviceListEvent, F&&);

  class cAudioDeviceList : public std::forward_list <cAudioDevice> {};

  //{{{
  class cAudioDeviceMonitor {
  public:
    //{{{
    static cAudioDeviceMonitor& instance() {

      static cAudioDeviceMonitor singleton;
      return singleton;
      }
    //}}}

    //{{{
    template <typename F> void registerCallback (cAudioDeviceListEvent event, F&& callback) {

      mCallbackMonitors[static_cast<int>(event)].reset (new WASAPINotificationClient { mEnumerator, event, std::move(callback)});
      }
    //}}}
    //{{{
    template <> void registerCallback (cAudioDeviceListEvent event, nullptr_t&&) {

      mCallbackMonitors[static_cast<int>(event)].reset();
      }
    //}}}

  private:
    //{{{
    cAudioDeviceMonitor() {

      HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                     __uuidof(IMMDeviceEnumerator), (void**)&mEnumerator);
      if (FAILED(hr))
        throw sAudioDeviceException ("Could not create device enumerator");
      }
    //}}}
    //{{{
    ~cAudioDeviceMonitor() {

      if (mEnumerator == nullptr)
        return;

      for (auto& callback_monitor : mCallbackMonitors)
        callback_monitor.reset();

      mEnumerator->Release();
      }
    //}}}

    //{{{
    class WASAPINotificationClient : public IMMNotificationClient {
    public:
      //{{{
      WASAPINotificationClient (IMMDeviceEnumerator* enumerator, cAudioDeviceListEvent event, std::function<void()> callback) :
          _enumerator(enumerator), _event(event), _callback(std::move(callback)) {

        if (_enumerator == nullptr)
          throw sAudioDeviceException("Attempting to create a notification client for a null enumerator");

        _enumerator->RegisterEndpointNotificationCallback(this);
        }
      //}}}
      //{{{
      virtual ~WASAPINotificationClient() {

        _enumerator->UnregisterEndpointNotificationCallback (this);
        }
      //}}}

      //{{{
      HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged (EDataFlow flow, ERole role, [[maybe_unused]] LPCWSTR deviceId) {

        if (role != ERole::eConsole)
          return S_OK;

        if (flow == EDataFlow::eRender) {
          if (_event != cAudioDeviceListEvent::eDefaultOutputChanged)
            return S_OK;
          }
        else if (flow == EDataFlow::eCapture) {
          if (_event != cAudioDeviceListEvent::eDefaultInputChanged)
            return S_OK;
          }

        _callback();
        return S_OK;
        }
      //}}}

      //{{{
      HRESULT STDMETHODCALLTYPE OnDeviceAdded ([[maybe_unused]] LPCWSTR deviceId) {

        if (_event != cAudioDeviceListEvent::eListChanged)
          return S_OK;

        _callback();
        return S_OK;
        }
      //}}}
      //{{{
      HRESULT STDMETHODCALLTYPE OnDeviceRemoved ([[maybe_unused]] LPCWSTR deviceId) {

        if (_event != cAudioDeviceListEvent::eListChanged)
          return S_OK;

        _callback();
        return S_OK;
        }
      //}}}
      //{{{
      HRESULT STDMETHODCALLTYPE OnDeviceStateChanged ([[maybe_unused]] LPCWSTR deviceId, [[maybe_unused]] DWORD new_state) {

        if (_event != cAudioDeviceListEvent::eListChanged)
          return S_OK;

        _callback();

        return S_OK;
        }
      //}}}
      //{{{
      HRESULT STDMETHODCALLTYPE OnPropertyValueChanged ([[maybe_unused]] LPCWSTR deviceId, [[maybe_unused]] const PROPERTYKEY key) {

        return S_OK;
        }
      //}}}

      //{{{
      HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, VOID **requested_interface) {

        if (IID_IUnknown == riid)
          *requested_interface = (IUnknown*)this;
        else if (__uuidof(IMMNotificationClient) == riid)
          *requested_interface = (IMMNotificationClient*)this;
        else {
          *requested_interface = nullptr;
          return E_NOINTERFACE;
          }

        return S_OK;
        }
      //}}}
      ULONG STDMETHODCALLTYPE AddRef() { return 1; }
      ULONG STDMETHODCALLTYPE Release() { return 0; }

    private:
      cWaspiUtil::cComInitializer mComInitializer;
      IMMDeviceEnumerator* _enumerator;
      cAudioDeviceListEvent _event;
      std::function<void()> _callback;
      };
    //}}}

    cWaspiUtil::cComInitializer mComInitializer;
    IMMDeviceEnumerator* mEnumerator = nullptr;
    std::array<std::unique_ptr<WASAPINotificationClient>, 3> mCallbackMonitors;
    };
  //}}}
  //{{{
  class cAudioDeviceEnumerator {
  public:
    static std::optional<cAudioDevice> getDefaultInputDevice() { return getDefaultDevice (false); }
    static std::optional<cAudioDevice> getDefaultOutputDevice() { return getDefaultDevice (true); }

    static auto getInputDeviceList() { return getDeviceList (false); }
    static auto getOutputDeviceList() { return getDeviceList (true); }

  private:
    cAudioDeviceEnumerator() = delete;

    //{{{
    static std::optional<cAudioDevice> getDefaultDevice (bool outputDevice) {

      cWaspiUtil::cComInitializer com_initializer;

      IMMDeviceEnumerator* enumerator = nullptr;
      cWaspiUtil::cAutoRelease enumerator_release { enumerator };

      HRESULT hr = CoCreateInstance (cWaspiUtil::getMMDeviceEnumeratorClassId(), nullptr,
                                     CLSCTX_ALL, cWaspiUtil::getIMMDeviceEnumeratorInterfaceId(),
                                     reinterpret_cast<void**>(&enumerator));
      if (FAILED(hr))
        return std::nullopt;

      IMMDevice* device = nullptr;
      hr = enumerator->GetDefaultAudioEndpoint (outputDevice ? eRender : eCapture, eConsole, &device);
      if (FAILED(hr))
        return std::nullopt;

      try {
        return cAudioDevice{ device, outputDevice };
        }
      catch (const sAudioDeviceException&) {
        return std::nullopt;
        }
      }
    //}}}
    //{{{
    static std::vector<IMMDevice*> getDevices (bool outputDevices) {

      cWaspiUtil::cComInitializer com_initializer;

      IMMDeviceEnumerator* enumerator = nullptr;
      cWaspiUtil::cAutoRelease enumerator_release { enumerator };

      HRESULT hr = CoCreateInstance (cWaspiUtil::getMMDeviceEnumeratorClassId(), nullptr,
                                     CLSCTX_ALL, cWaspiUtil::getIMMDeviceEnumeratorInterfaceId(),
                                     reinterpret_cast<void**>(&enumerator));
      if (FAILED(hr))
        return {};

      IMMDeviceCollection* device_collection = nullptr;
      cWaspiUtil::cAutoRelease collection_release { device_collection };

      EDataFlow selected_data_flow = outputDevices ? eRender : eCapture;
      hr = enumerator->EnumAudioEndpoints (selected_data_flow, DEVICE_STATE_ACTIVE, &device_collection);
      if (FAILED (hr))
        return {};

      UINT device_count = 0;
      hr = device_collection->GetCount (&device_count);
      if (FAILED (hr))
        return {};

      std::vector<IMMDevice*> devices;
      for (UINT i = 0; i < device_count; i++) {
        IMMDevice* device = nullptr;
        hr = device_collection->Item (i, &device);
        if (FAILED(hr)) {
          if (device != nullptr)
            device->Release();
          continue;
          }

        if (device != nullptr)
          devices.push_back (device);
        }

      return devices;
      }
    //}}}
    //{{{
    static cAudioDeviceList getDeviceList (bool outputDevices) {

      cWaspiUtil::cComInitializer com_initializer;

      cAudioDeviceList devices;
      const auto mmdevices = getDevices (outputDevices);

      for (auto* mmdevice : mmdevices) {
        if (mmdevice == nullptr)
          continue;

        try {
          devices.push_front (cAudioDevice { mmdevice, outputDevices });
          }
        catch (const sAudioDeviceException&) {
          // TODO: Should I do anything with this exception?
          // My impulse is to leave it alone.  The result of this function
          // should be an array of properly-constructed devices.  If we
          // couldn't create a device, then we shouldn't return it from
          // this function.
          }
        }

      return devices;
      }
    //}}}
    };
  //}}}

  //{{{
  std::optional<cAudioDevice> getDefaultAudioInputDevice() {
    return cAudioDeviceEnumerator::getDefaultInputDevice();
    }
  //}}}
  //{{{
  std::optional<cAudioDevice> getDefaultAudioOutputDevice() {
    return cAudioDeviceEnumerator::getDefaultOutputDevice();
    }
  //}}}
  cAudioDeviceList getAudioInputDeviceList() { return cAudioDeviceEnumerator::getInputDeviceList(); }
  cAudioDeviceList getAudioOutputDeviceList() { return cAudioDeviceEnumerator::getOutputDeviceList(); }

  //{{{
  template <typename F, typename /* = enable_if_t<is_invocable_v<F>> */> void setAudioDeviceListCallback (cAudioDeviceListEvent event, F&& callback) {
    cAudioDeviceMonitor::instance().registerCallback (event, std::move (callback));
    }
  //}}}
  }
