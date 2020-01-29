#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

//{{{
#ifdef __cplusplus
  extern "C" {
#endif
//}}}

//{{{
typedef struct TinyWavHeader {
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
  } TinyWavHeader;
//}}}
//{{{
typedef enum TinyWavChannelFormat {
  TW_INTERLEAVED, // channel buffer is interleaved e.g. [LRLRLRLR]
  TW_INLINE,      // channel buffer is inlined e.g. [LLLLRRRR]
  TW_SPLIT        // channel buffer is split e.g. [[LLLL],[RRRR]]
  } TinyWavChannelFormat;
//}}}
//{{{
typedef enum TinyWavSampleFormat {
  TW_INT16 = 2,  // two byte signed integer
  TW_FLOAT32 = 4 // four byte IEEE float
  } TinyWavSampleFormat;
//}}}
//{{{
typedef struct TinyWav {
  FILE* f;
  TinyWavHeader h;
  int16_t numChannels;
  uint32_t totalFramesWritten;
  TinyWavChannelFormat chanFmt;
  TinyWavSampleFormat sampFmt;
  } TinyWav;
//}}}

int tinywav_open_read (TinyWav* tw, const char* path, TinyWavChannelFormat chanFmt, TinyWavSampleFormat sampFmt);
int tinywav_read (TinyWav* tw, void* data, int len);
void tinywav_close_read (TinyWav *tw);

int tinywav_open_write (TinyWav* tw, int16_t numChannels, int32_t samplerate,
                        TinyWavSampleFormat sampFmt, TinyWavChannelFormat chanFmt,
                        const char* path);
size_t tinywav_write (TinyWav* tw, void* f, int len);
void tinywav_close_write (TinyWav* tw);

//{{{
#ifdef __cplusplus
  }
#endif
//}}}
