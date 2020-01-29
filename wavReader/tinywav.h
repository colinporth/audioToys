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
  typedef struct tWavHeader {
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint32_t Format;

    uint32_t Subchunk1ID;
    uint32_t Subchunk1Size;

    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;

    uint32_t Subchunk2ID;
    uint32_t Subchunk2Size;
    } tWavHeader;
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
