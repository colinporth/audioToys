// cTineyWav.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <string.h>
#include <malloc.h>

#include "tinywav.h"
//}}}

// class cTinyWav
//{{{
int cTinyWav::openRead (const char* filePath, eSampleFormat sampleFormat, eChannelFormat channelFormat) {

  mSampleFormat = sampleFormat;
  mChannelFormat = channelFormat;

  mFile = fopen (filePath, "rb");
  assert (mFile != NULL);

  auto ret = fread (&mHeader, sizeof (tWavHeader), 1, mFile);
  assert (ret > 0);
  assert (mHeader.ChunkID == htonl (0x52494646));     // "RIFF"
  assert (mHeader.Format == htonl (0x57415645));      // "WAVE"
  assert (mHeader.Subchunk1ID == htonl (0x666d7420)); // "fmt "

  // skip over any other chunks before the "data" chunk
  while (mHeader.Subchunk2ID != htonl (0x64617461)) {
    fseek (mFile, 4, SEEK_CUR);
    fread (&mHeader.Subchunk2ID, 4, 1, mFile);
    }

  // data chunk
  fread (&mHeader.Subchunk2Size, 4, 1, mFile);

  mNumChannels = mHeader.NumChannels;
  mTotalFramesWritten = mHeader.Subchunk2Size / (mNumChannels * sampleFormat);

  return 0;
  }
//}}}
//{{{
int cTinyWav::read (void* data, int len) {
// returns number of frames read

  switch (mSampleFormat) {
    case TW_INT16: {
      auto z = (int16_t*)alloca (mNumChannels * len * sizeof(int16_t));
      switch (mChannelFormat) {
        //{{{
        case TW_INTERLEAVED: {
          const float* const x = (const float* const) data;
          for (int i = 0; i < mNumChannels*len; ++i)
            z[i] = (int16_t) (x[i] * 32767.0f);

          break;
          }
        //}}}
        //{{{
        case TW_INLINE: {
          const float* const x = (const float* const) data;
          for (int i = 0, k = 0; i < len; ++i)
            for (int j = 0; j < mNumChannels; ++j)
              z[k++] = (int16_t) (x[j*len+i] * 32767.0f);
           break;
          }
        //}}}
        //{{{
        case TW_SPLIT: {
          const float** const x = (const float** const)data;
          for (int i = 0, k = 0; i < len; ++i)
            for (int j = 0; j < mNumChannels; ++j)
              z[k++] = (int16_t) (x[j][i] * 32767.0f);

          break;
          }

        //}}}
        default: return 0;
        }

      mTotalFramesWritten += len;
      return (int)fwrite (z, sizeof(int16_t), mNumChannels * len, mFile);
      }

    case TW_FLOAT32: {
      size_t samples_read = 0;
      auto interleaved_data = (float*)alloca (mNumChannels * len * sizeof(float));
      samples_read = fread (interleaved_data, sizeof(float), mNumChannels * len, mFile);
      switch (mChannelFormat) {
        //{{{
        case TW_INTERLEAVED: {
          // channel buffer is interleaved e.g. [LRLRLRLR]
          memcpy (data, interleaved_data, mNumChannels * len * sizeof(float));
          return (int) (samples_read / mNumChannels);
          }
        //}}}
        //{{{
        case TW_INLINE: {
          // channel buffer is inlined e.g. [LLLLRRRR]
          for (int i = 0, pos = 0; i < mNumChannels; i++)
            for (int j = i; j < len * mNumChannels; j += mNumChannels, ++pos)
              ((float*)data)[pos] = interleaved_data[j];

          return (int) (samples_read/ mNumChannels);
          }
        //}}}
        //{{{
        case TW_SPLIT: {
          // channel buffer is split e.g. [[LLLL],[RRRR]]
          for (int i = 0, pos = 0; i < mNumChannels; i++)
            for (int j = 0; j < len; j++, ++pos)
              ((float**)data)[i][j] = interleaved_data[j* mNumChannels + i];

          return (int) (samples_read / mNumChannels);
          }
        //}}}
        default: return 0;
        }
      }

    default: return 0;
    }

  return len;
  }
//}}}
//{{{
void cTinyWav::closeRead() {

  fclose (mFile);
  mFile = NULL;
  }
//}}}

//{{{
int cTinyWav::openWrite (const char* path, int16_t numChannels, int32_t samplerate,
                         eSampleFormat sampleFormat, eChannelFormat channelFormat) {

  errno_t err = fopen_s (&mFile, path, "w");
  assert (err == 0);
  assert (mFile != NULL);

  mNumChannels = numChannels;
  mSampleFormat = sampleFormat;
  mChannelFormat = channelFormat;

  mTotalFramesWritten = 0;

  // prepare WAV header
  tWavHeader mHeader;
  mHeader.ChunkID = htonl (0x52494646); // "RIFF"
  mHeader.ChunkSize = 0; // fill this in on file-close
  mHeader.Format = htonl (0x57415645); // "WAVE"

  mHeader.Subchunk1ID = htonl (0x666d7420); // "fmt "
  mHeader.Subchunk1Size = 16; // PCM

  mHeader.AudioFormat = (sampleFormat-1); // 1 PCM, 3 IEEE float
  mHeader.NumChannels = numChannels;
  mHeader.SampleRate = samplerate;
  mHeader.ByteRate = samplerate * numChannels * sampleFormat;
  mHeader.BlockAlign = numChannels * sampleFormat;
  mHeader.BitsPerSample = 8*sampleFormat;

  mHeader.Subchunk2ID = htonl(0x64617461); // "data"
  mHeader.Subchunk2Size = 0; // fill this in on file-close

  // write WAV header
  fwrite (&mHeader, sizeof(tWavHeader), 1, mFile);

  return 0;
  }
//}}}
//{{{
size_t cTinyWav::write (void* data, int len) {

  switch (mSampleFormat) {
    case TW_INT16: {
      auto z = (int16_t*)alloca (mNumChannels * len * sizeof(int16_t));
      switch (mChannelFormat) {
        //{{{
        case TW_INTERLEAVED: {
          const float *const x = (const float *const)data;
          for (int i = 0; i < mNumChannels *len; ++i) {
            z[i] = (int16_t) (x[i] * 32767.0f);
            }
          break;
          }
        //}}}
        //{{{
        case TW_INLINE: {
          const float *const x = (const float *const)data;
          for (int i = 0, k = 0; i < len; ++i) {
            for (int j = 0; j < mNumChannels; ++j) {
              z[k++] = (int16_t) (x[j*len+i] * 32767.0f);
              }
            }
          break;
          }
        //}}}
        //{{{
        case TW_SPLIT: {
          const float **const x = (const float **const)data;
          for (int i = 0, k = 0; i < len; ++i) {
            for (int j = 0; j < mNumChannels; ++j) {
              z[k++] = (int16_t) (x[j][i] * 32767.0f);
              }
            }
          break;
          }
        //}}}
        default: return 0;
        }

      mTotalFramesWritten += len;
      return fwrite(z, sizeof(int16_t), mNumChannels * len, mFile);
      break;
      }

    case TW_FLOAT32: {
      auto z = (float*)alloca (mNumChannels * len * sizeof(float));
      switch (mChannelFormat) {
        //{{{
        case TW_INTERLEAVED: {
          mTotalFramesWritten += len;
          return fwrite (data, sizeof(float), mNumChannels *len, mFile);
          }
        //}}}
        //{{{
        case TW_INLINE: {
          const float *const x = (const float *const)data;
          for (int i = 0, k = 0; i < len; ++i) {
            for (int j = 0; j < mNumChannels; ++j) {
              z[k++] = x[j*len+i];
              }
            }
          break;
          }
        //}}}
        //{{{
        case TW_SPLIT: {
          const float **const x = (const float **const)data;
          for (int i = 0, k = 0; i < len; ++i) {
            for (int j = 0; j < mNumChannels; ++j) {
              z[k++] = x[j][i];
              }
            }
          break;
          }
        //}}}
        default: return 0;
        }

      mTotalFramesWritten += len;
      return fwrite (z, sizeof(float), mNumChannels*len, mFile);
      }

    default: return 0;
    }
  }
//}}}
//{{{
void cTinyWav::closeWrite() {

  uint32_t dataLen = mTotalFramesWritten * mNumChannels * mSampleFormat;

  // set length of data
  fseek (mFile, 4, SEEK_SET);
  uint32_t chunkSizeLen = 36 + dataLen;
  fwrite (&chunkSizeLen, sizeof(uint32_t), 1, mFile);

  fseek (mFile, 40, SEEK_SET);
  fwrite (&dataLen, sizeof(uint32_t), 1, mFile);

  fclose (mFile);
  mFile = NULL;
  }
//}}}
