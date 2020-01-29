#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

//{{{
#ifdef __cplusplus
  extern "C" {
#endif
//}}}

class cTinyWav {
public:
  enum eSampleFormat { TW_INT16 = 2, TW_FLOAT32 = 4 };
  enum eChannelFormat { TW_INTERLEAVED, TW_INLINE, TW_SPLIT };

  int getNumChannels()  { return mNumChannels; }
  eSampleFormat getSampleFormat()  { return mSampleFormat; }
  eChannelFormat getChannelFormat()  { return mChannelFormat; }

  int openRead (const char* filePath, eSampleFormat sampleFormat, eChannelFormat channelFormat);
  int read (void* data, int len);
  void closeRead();

  int openWrite (const char* path, int16_t numChannels, int32_t samplerate,
                 eSampleFormat sampleFormat, eChannelFormat channelFormat);
  size_t write (void* data, int len);
  void closeWrite();

private:
  //{{{
  struct tWavHeader {
    uint32_t ChunkID;        // 0
    uint32_t ChunkSize;      // 4
    uint32_t Format;         // 8

    uint32_t Subchunk1ID;    // 12
    uint32_t Subchunk1Size;  // 16

    uint16_t AudioFormat;    // 20
    uint16_t NumChannels;    // 22
    uint32_t SampleRate;     // 24
    uint32_t ByteRate;       // 28
    uint16_t BlockAlign;     // 32
    uint16_t BitsPerSample;  // 34

    uint32_t Subchunk2ID;    // 36
    uint32_t Subchunk2Size;  // 40
    };                       // 44
  //}}}

  //{{{
  static uint16_t htons (uint16_t v) {
    return (v >> 8) | (v << 8);
    }
  //}}}
  //{{{
  static uint32_t htonl (uint32_t v) {
    return htons(v >> 16) | (htons((uint16_t) v) << 16);
    }
  //}}}

  FILE* mFile;
  tWavHeader mHeader;

  int16_t mNumChannels;
  eChannelFormat mChannelFormat;
  eSampleFormat mSampleFormat;

  uint32_t mTotalFramesWritten;
  };

//{{{
#ifdef __cplusplus
  }
#endif
//}}}
