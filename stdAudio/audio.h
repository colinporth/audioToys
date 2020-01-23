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
    //{{{
    cAudioBuffer (SampleType* data, size_t numFrames, size_t numChannels, sContiguousInterleaved)
        : mNumFrames(numFrames), mNumChannels(numChannels), mStride(mNumChannels), mIsContiguous(true) {

      assert (numChannels <= mMaxNumChannels);
      for (auto i = 0; i < mNumChannels; ++i)
        mChannels[i] = data + i;
      }
    //}}}
    //{{{
    cAudioBuffer (SampleType* data, size_t numFrames, size_t numChannels, sContiguousDeinterleaved)
        : mNumFrames(numFrames), mNumChannels(numChannels), mStride(1), mIsContiguous(true) {

      assert (numChannels <= mMaxNumChannels);
      for (auto i = 0; i < mNumChannels; ++i)
        mChannels[i] = data + (i * mNumFrames);
      }
    //}}}
    //{{{
    cAudioBuffer (SampleType** data, size_t numFrames, size_t numChannels, sPtrToPtrDeinterleaved)
        : mNumFrames(numFrames), mNumChannels(numChannels), mStride(1), mIsContiguous(false) {

      assert (numChannels <= mMaxNumChannels);
      copy (data, data + mNumChannels, mChannels.begin());
      }
    //}}}

    SampleType* data() const noexcept { return mIsContiguous ? mChannels[0] : nullptr; }

    bool isContiguous() const noexcept { return mIsContiguous; }
    bool areFramesContiguous() const noexcept { return mStride == mNumChannels; }
    bool areChannelsContiguous() const noexcept { return mStride == 1; }

    size_t getSizeFrames() const noexcept { return mNumFrames; }
    size_t getSizeChannels() const noexcept { return mNumChannels; }
    size_t getSizeSamples() const noexcept { return mNumChannels * mNumFrames; }

    //{{{
    SampleType& operator() (size_t frame, size_t channel) noexcept {

      return const_cast<SampleType&>(std::as_const(*this).operator()(frame, channel));
      }
    //}}}
    //{{{
    const SampleType& operator() (size_t frame, size_t channel) const noexcept {

      return mChannels[channel][frame * mStride];
      }
    //}}}

  private:
    bool mIsContiguous = false;

    size_t mNumFrames = 0;
    size_t mNumChannels = 0;
    size_t mStride = 0;

    constexpr static size_t mMaxNumChannels = 16;
    std::array<SampleType*, mMaxNumChannels> mChannels = {};
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
      cComInitializer() : mHr (CoInitialize (nullptr)) {}

      ~cComInitializer() {
        if (SUCCEEDED (mHr))
          CoUninitialize();
        }

      operator HRESULT() const { return mHr; }

      HRESULT mHr;
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
      mAudioClient(other.mAudioClient),
      mAudioCaptureClient(other.mAudioCaptureClient),
      mAudioRenderClient(other.mAudioRenderClient),
      mEventHandle(other.mEventHandle),
      mDeviceId(std::move(other.mDeviceId)),
      mRunning(other.mRunning.load()),
      mName(std::move(other.mName)),
      mMixFormat(other.mMixFormat),
      mProcessingThread(std::move(other.mProcessingThread)),
      mBufferFrameCount(other.mBufferFrameCount),
      mIsRenderDevice(other.mIsRenderDevice),
      mStopCallback(std::move(other.mStopCallback)),
      mUserCallback(std::move(other.mUserCallback))
    {
      other.mDevice = nullptr;
      other.mAudioClient = nullptr;
      other.mAudioCaptureClient = nullptr;
      other.mAudioRenderClient = nullptr;
      other.mEventHandle = nullptr;
    }
    //}}}
    //{{{
    cAudioDevice& operator= (cAudioDevice&& other) noexcept {

      if (this == &other)
        return *this;

      mDevice = other.mDevice;
      mAudioClient = other.mAudioClient;
      mAudioCaptureClient = other.mAudioCaptureClient;
      mAudioRenderClient = other.mAudioRenderClient;
      mEventHandle = other.mEventHandle;
      mDeviceId = std::move(other.mDeviceId);
      mRunning = other.mRunning.load();
      mName = std::move(other.mName);
      mMixFormat = other.mMixFormat;
      mProcessingThread = std::move(other.mProcessingThread);
      mBufferFrameCount = other.mBufferFrameCount;
      mIsRenderDevice = other.mIsRenderDevice;
      mStopCallback = std::move (other.mStopCallback);
      mUserCallback = std::move (other.mUserCallback);

      other.mDevice = nullptr;
      other.mAudioClient = nullptr;
      other.mAudioCaptureClient = nullptr;
      other.mAudioRenderClient = nullptr;
      other.mEventHandle = nullptr;
    }
    //}}}
    //{{{
    ~cAudioDevice() {

      stop();

      if (mAudioCaptureClient != nullptr)
        mAudioCaptureClient->Release();

      if (mAudioRenderClient != nullptr)
        mAudioRenderClient->Release();

      if (mAudioClient != nullptr)
        mAudioClient->Release();

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
    buffer_size_t getBufferSizeFrames() const noexcept { return mBufferFrameCount; }
    //{{{
    bool setBufferSizeFrames (buffer_size_t bufferSize) {
      mBufferFrameCount = bufferSize;
      return true;
      }
    //}}}

    //{{{
    template <typename SampleType> constexpr bool supportsSampleType() const noexcept {

      return is_same_v<SampleType, float> ||
             is_same_v<SampleType, int32_t> ||
             is_same_v<SampleType, int16_t>;
      }
    //}}}
    //{{{
    template <typename SampleType> bool setSampleType() {

      if (_is_connected() && !is_sampleType<SampleType>())
        throw sAudioDeviceException ("Cannot change sample type after connecting a callback.");

      return setSampleTypeHelper<SampleType>();
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
    template <typename CallbackType,
              std::enable_if_t <std::is_nothrow_invocable_v <CallbackType, cAudioDevice&, sAudioDeviceIo<float>&>, int> = 0>
    void connect (CallbackType callback) {

      setSampleTypeHelper<float>();
      connectHelper (wasapi_float_callback_t { callback } );
      }
    //}}}
    //{{{  template int32_t void connect (CallbackType callback
    template <typename CallbackType,
              std::enable_if_t <std::is_nothrow_invocable_v <CallbackType, cAudioDevice&, sAudioDeviceIo<int32_t>&>, int> = 0>
    void connect (CallbackType callback) {

      setSampleTypeHelper<int32_t>();
      connectHelper (wasapi_int32_callback_t { callback } );
      }
    //}}}
    //{{{  template int16_t void connect (CallbackType callback
    template <typename CallbackType,
              std::enable_if_t <std::is_nothrow_invocable_v <CallbackType, cAudioDevice&, sAudioDeviceIo<int16_t>&>, int> = 0>
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

      if (mAudioClient == nullptr)
        return false;

      if (!mRunning) {
        mEventHandle = CreateEvent (nullptr, FALSE, FALSE, nullptr);
        if (mEventHandle == nullptr)
          return false;

        REFERENCE_TIME periodicity = 0;
        const REFERENCE_TIME ref_times_per_second = 10'000'000;
        REFERENCE_TIME buffer_duration = (ref_times_per_second * mBufferFrameCount) / mMixFormat.Format.nSamplesPerSec;
        HRESULT hr = mAudioClient->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                               AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                               buffer_duration, periodicity, &mMixFormat.Format, nullptr);

        // TODO: Deal with AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED return code by resetting the buffer_duration and retrying:
        // https://docs.microsoft.com/en-us/windows/desktop/api/audioclient/nf-audioclient-iaudioclient-initialize
        if (FAILED(hr))
          return false;

        /*HRESULT render_hr =*/ mAudioClient->GetService (cWaspiUtil::getIAudioRenderClientInterfaceId(), reinterpret_cast<void**>(&mAudioRenderClient));
        /*HRESULT capture_hr =*/ mAudioClient->GetService (cWaspiUtil::getIAudioCaptureClientInterfaceId(), reinterpret_cast<void**>(&mAudioCaptureClient));

        // TODO: Make sure to clean up more gracefully from errors
        hr = mAudioClient->GetBufferSize (&mBufferFrameCount);
        if (FAILED (hr))
          return false;
        hr = mAudioClient->SetEventHandle (mEventHandle);
        if (FAILED (hr))
          return false;
        hr = mAudioClient->Start();
        if (FAILED (hr))
          return false;

        mRunning = true;

        if (!mUserCallback.valueless_by_exception()) {
          mProcessingThread = std::thread { [this]() {
            SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
            while (mRunning) {
              visit ([this](auto&& callback) { if (callback) process(callback); }, mUserCallback);
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

      if (mRunning) {
        mRunning = false;

        if (mProcessingThread.joinable())
          mProcessingThread.join();
        if (mAudioClient != nullptr)
          mAudioClient->Stop();
        if (mEventHandle != nullptr)
          CloseHandle (mEventHandle);

        mStopCallback (*this);
        }

      return true;
      }
    //}}}

    bool isRunning() const noexcept { return mRunning; }
    void wait() const { WaitForSingleObject (mEventHandle, INFINITE); }

    //{{{  template void float process (const CallbackType& callback
    template <typename CallbackType,
              std::enable_if_t <std::is_invocable_v<CallbackType, cAudioDevice&, sAudioDeviceIo<float>&>, int> = 0>
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

      if (mAudioClient == nullptr)
        return false;

      if (!mRunning)
        return false;

      UINT32 current_padding = 0;
      mAudioClient->GetCurrentPadding (&current_padding);

      auto num_frames_available = mBufferFrameCount - current_padding;
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
      if (mAudioClient == nullptr)
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

      HRESULT hr = mDevice->Activate (cWaspiUtil::getIAudioClientInterfaceId(), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&mAudioClient));
      if (FAILED(hr))
        return;
      }
    //}}}
    //{{{
    void initMixFormat() {

      WAVEFORMATEX* deviceMixFormat;
      HRESULT hr = mAudioClient->GetMixFormat (&deviceMixFormat);
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
    bool isConnected() const noexcept {

      if (mUserCallback.valueless_by_exception())
        return false;

      return visit([](auto&& callback) { return static_cast<bool>(callback); }, mUserCallback);
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

      if (mAudioClient == nullptr)
        return;

      if (!mixFormatMatchesType <SampleType>())
        return;

      if (isOutput()) {
        UINT32 current_padding = 0;
        mAudioClient->GetCurrentPadding (&current_padding);

        auto numFramesAvailable = mBufferFrameCount - current_padding;
        if (numFramesAvailable == 0)
          return;

        BYTE* data = nullptr;
        mAudioRenderClient->GetBuffer (numFramesAvailable, &data);
        if (data == nullptr)
          return;

        sAudioDeviceIo<SampleType> deviceIo;
        deviceIo.outputBuffer = { reinterpret_cast<SampleType*>(data), numFramesAvailable, mMixFormat.Format.nChannels, contiguousInterleaved };
        callback (*this, deviceIo);

        mAudioRenderClient->ReleaseBuffer (numFramesAvailable, 0);
        }

      else if (isInput()) {
        UINT32 nextPacketSize = 0;
        mAudioCaptureClient->GetNextPacketSize (&nextPacketSize);
        if (nextPacketSize == 0)
          return;

        // TODO: Support device position.
        DWORD flags = 0;
        BYTE* data = nullptr;
        mAudioCaptureClient->GetBuffer (&data, &nextPacketSize, &flags, nullptr, nullptr);
        if (data == nullptr)
          return;

        sAudioDeviceIo<SampleType> deviceIo;
        deviceIo.inputBuffer = { reinterpret_cast<SampleType*>(data), nextPacketSize, mMixFormat.Format.nChannels, contiguousInterleaved };
        callback (*this, deviceIo);

        mAudioCaptureClient->ReleaseBuffer (nextPacketSize);
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
    template <typename CallbackType> void connectHelper (CallbackType callback) {

      if (mRunning)
        throw sAudioDeviceException ("Cannot connect to running audio_device.");

      mUserCallback = move (callback);
      }
    //}}}

    IMMDevice* mDevice = nullptr;
    IAudioClient* mAudioClient = nullptr;
    IAudioCaptureClient* mAudioCaptureClient = nullptr;
    IAudioRenderClient* mAudioRenderClient = nullptr;
    HANDLE mEventHandle;

    std::wstring mDeviceId;
    std::atomic<bool> mRunning = false;
    std::string mName;

    WAVEFORMATEXTENSIBLE mMixFormat;
    std::thread mProcessingThread;
    UINT32 mBufferFrameCount = 0;
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
          mEnumerator(enumerator), mEvent(event), mCallback(std::move(callback)) {

        if (mEnumerator == nullptr)
          throw sAudioDeviceException("Attempting to create a notification client for a null enumerator");

        mEnumerator->RegisterEndpointNotificationCallback(this);
        }
      //}}}
      //{{{
      virtual ~WASAPINotificationClient() {

        mEnumerator->UnregisterEndpointNotificationCallback (this);
        }
      //}}}

      //{{{
      HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged (EDataFlow flow, ERole role, [[maybe_unused]] LPCWSTR deviceId) {

        if (role != ERole::eConsole)
          return S_OK;

        if (flow == EDataFlow::eRender) {
          if (mEvent != cAudioDeviceListEvent::eDefaultOutputChanged)
            return S_OK;
          }
        else if (flow == EDataFlow::eCapture) {
          if (mEvent != cAudioDeviceListEvent::eDefaultInputChanged)
            return S_OK;
          }

        mCallback();
        return S_OK;
        }
      //}}}

      //{{{
      HRESULT STDMETHODCALLTYPE OnDeviceAdded ([[maybe_unused]] LPCWSTR deviceId) {

        if (mEvent != cAudioDeviceListEvent::eListChanged)
          return S_OK;

        mCallback();
        return S_OK;
        }
      //}}}
      //{{{
      HRESULT STDMETHODCALLTYPE OnDeviceRemoved ([[maybe_unused]] LPCWSTR deviceId) {

        if (mEvent != cAudioDeviceListEvent::eListChanged)
          return S_OK;

        mCallback();
        return S_OK;
        }
      //}}}
      //{{{
      HRESULT STDMETHODCALLTYPE OnDeviceStateChanged ([[maybe_unused]] LPCWSTR deviceId, [[maybe_unused]] DWORD new_state) {

        if (mEvent != cAudioDeviceListEvent::eListChanged)
          return S_OK;

        mCallback();

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
      IMMDeviceEnumerator* mEnumerator;
      cAudioDeviceListEvent mEvent;
      std::function<void()> mCallback;
      };
    //}}}

    cWaspiUtil::cComInitializer mComInitializer;

    IMMDeviceEnumerator* mEnumerator = nullptr;
    std::array <std::unique_ptr <WASAPINotificationClient>, 3> mCallbackMonitors;
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

      cWaspiUtil::cComInitializer comInitializer;

      IMMDeviceEnumerator* enumerator = nullptr;
      cWaspiUtil::cAutoRelease enumerator_release { enumerator };

      HRESULT hr = CoCreateInstance (cWaspiUtil::getMMDeviceEnumeratorClassId(), nullptr,
                                     CLSCTX_ALL, cWaspiUtil::getIMMDeviceEnumeratorInterfaceId(),
                                     reinterpret_cast<void**>(&enumerator));
      if (FAILED (hr))
        return std::nullopt;

      IMMDevice* device = nullptr;
      hr = enumerator->GetDefaultAudioEndpoint (outputDevice ? eRender : eCapture, eConsole, &device);
      if (FAILED(hr))
        return std::nullopt;

      try {
        return cAudioDevice { device, outputDevice };
        }
      catch (const sAudioDeviceException&) {
        return std::nullopt;
        }
      }
    //}}}
    //{{{
    static std::vector<IMMDevice*> getDevices (bool outputDevices) {

      cWaspiUtil::cComInitializer comInitializer;

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

      cWaspiUtil::cComInitializer comInitializer;

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
  cAudioDeviceList getAudioInputDeviceList() { return cAudioDeviceEnumerator::getInputDeviceList(); }
  cAudioDeviceList getAudioOutputDeviceList() { return cAudioDeviceEnumerator::getOutputDeviceList(); }
  std::optional<cAudioDevice> getDefaultAudioInputDevice() { return cAudioDeviceEnumerator::getDefaultInputDevice(); }
  std::optional<cAudioDevice> getDefaultAudioOutputDevice() { return cAudioDeviceEnumerator::getDefaultOutputDevice(); }

  //{{{
  template <typename F, typename /* = enable_if_t<is_invocable_v<F>> */> void setAudioDeviceListCallback (cAudioDeviceListEvent event, F&& callback) {
    cAudioDeviceMonitor::instance().registerCallback (event, std::move (callback));
    }
  //}}}
  }
