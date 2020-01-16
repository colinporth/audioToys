//{{{  based on transcode_aac.c example
/*
 * Copyright (c) 2013-2018 Andreas Unterweger
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
//}}}
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <windows.h>

extern "C" {
  #include "libavformat/avformat.h"
  #include "libavformat/avio.h"

  #include "libavcodec/avcodec.h"

  #include "libavutil/audio_fifo.h"
  #include "libavutil/avassert.h"
  #include "libavutil/avstring.h"
  #include "libavutil/frame.h"
  #include "libavutil/opt.h"

  #include "libswresample/swresample.h"
  }

#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cBipBuffer.h"
//}}}

#define OUTPUT_CHANNELS 2
#define OUTPUT_BIT_RATE 128000
#define OUTPUT_SAMPLE_RATE 44100

static int64_t pts = 0;

//{{{
bool openInFile (const char* filename, AVFormatContext*& inFormatContext, AVCodecContext*& inCodecContext) {

  // Open the input file to read from it. */
  int error = avformat_open_input (&inFormatContext, filename, NULL, NULL);
  if (error < 0) {
    //{{{
    cLog::log (LOGERROR, "Could not open input file");
    inFormatContext = NULL;
    return false;
    }
    //}}}

  // Get information on the input file (number of streams etc.). */
  error = avformat_find_stream_info (inFormatContext, NULL);
  if (error < 0) {
    //{{{
    cLog::log (LOGINFO, "Could not open find stream info");
    avformat_close_input (&inFormatContext);
    return false;
    }
    //}}}

  // Make sure that there is only one stream in the input file. */
  if (inFormatContext->nb_streams != 1) {
    //{{{
    cLog::log (LOGERROR, "Expected one audio input stream, but found %d", inFormatContext->nb_streams);
    avformat_close_input (&inFormatContext);
    return false;
    }
    //}}}

  // Find a decoder for the audio stream. */
  AVCodec* input_codec = avcodec_find_decoder (inFormatContext->streams[0]->codecpar->codec_id);
  if (!input_codec) {
    //{{{
    cLog::log (LOGERROR, "Could not find input codec");
    avformat_close_input (&inFormatContext);
    return false;
    }
    //}}}

  // Allocate a new decoding context. */
  inCodecContext = avcodec_alloc_context3 (input_codec);
  if (!inCodecContext) {
    //{{{
    cLog::log (LOGERROR, "Could not allocate a decoding context");
    avformat_close_input (&inFormatContext);
    return false;
    }
    //}}}

  // Initialize the stream parameters with demuxer information. */
  error = avcodec_parameters_to_context (inCodecContext, inFormatContext->streams[0]->codecpar);
  if (error < 0) {
    avformat_close_input (&inFormatContext);
    avcodec_free_context (&inCodecContext);
    return false;
    }

  // Open the decoder for the audio stream to use it later. */
  if ((error = avcodec_open2 (inCodecContext, input_codec, NULL)) < 0) {
    //{{{
    cLog::log (LOGERROR, "Could not open input codec ");
    avcodec_free_context (&inCodecContext);
    avformat_close_input (&inFormatContext);
    return false;
    }
    //}}}

  return true;
  }
//}}}
//{{{
bool openOutFile (const char* filename, AVFormatContext*& outFormatContext, AVCodecContext*& outCodecContext) {

  // Find the encoder to be used by its name
  AVCodec* output_codec = avcodec_find_encoder (AV_CODEC_ID_AAC);
  outCodecContext = avcodec_alloc_context3 (output_codec);

  // Set the basic encoder parameters, input file's sample rate is used to avoid a sample rate conversion
  outCodecContext->channels = OUTPUT_CHANNELS;
  outCodecContext->channel_layout = av_get_default_channel_layout (OUTPUT_CHANNELS);
  outCodecContext->sample_rate = OUTPUT_SAMPLE_RATE;
  outCodecContext->sample_fmt = output_codec->sample_fmts[0];
  outCodecContext->bit_rate = OUTPUT_BIT_RATE;
  outCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  // Open the output file to write to it. */
  AVIOContext* output_io_context;
  int error = avio_open (&output_io_context, filename, AVIO_FLAG_WRITE);

  // Create a new format context for the output container format
  outFormatContext = avformat_alloc_context();
  outFormatContext->pb = output_io_context;
  outFormatContext->oformat = av_guess_format (NULL, filename, NULL);

  // Create a new audio stream in the output file container. */
  AVStream* stream = avformat_new_stream (outFormatContext, NULL);
  stream->time_base.den = OUTPUT_SAMPLE_RATE;
  stream->time_base.num = 1;

  // Some container formats (like MP4) require global headers to be present
  // Mark the encoder so that it behaves accordingly
  if (outFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
    outCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  // Open the encoder for the audio stream to use it later
  //auto res = av_opt_set (codecContext->priv_data, "profile", "aac_he", 0);
  //printf ("setopt %x", res);
  error = avcodec_open2 (outCodecContext, output_codec, NULL);
  error = avcodec_parameters_from_context (stream->codecpar, outCodecContext);

  return true;
  }
//}}}

//{{{
bool decodeFrame (AVFrame* frame, AVFormatContext* inFormatContext, AVCodecContext* inCodecContext, bool& hasData, bool& done) {

  bool ok = false;
  hasData = false;
  done = false;

  // Packet used for temporary storage
  AVPacket input_packet;
  av_init_packet (&input_packet);
  input_packet.data = NULL;
  input_packet.size = 0;

  // Read one audio frame from the input file into a temporary packet
  int error = av_read_frame (inFormatContext, &input_packet);
  if (error < 0) {
    // If we are at the end of the file, flush the decoder below
    if (error == AVERROR_EOF)
      done = true;
    else {
      cLog::log (LOGERROR, "error read frame");
      return false;
      }
    }

  // Send the audio frame stored in the temporary packet to the decoder, input audio stream decoder is used to do this
  error = avcodec_send_packet (inCodecContext, &input_packet);
  if (error < 0)
    cLog::log (LOGERROR, "error send packet for decoding ");
  else {
    // Receive one frame from the decoder
    // If the decoder asks for more data to be able to decode a frame, return indicating that no data is present
    error = avcodec_receive_frame (inCodecContext, frame);
    if (error == AVERROR_EOF) {
      ok = true;
      done = true;
      }
    else if (error == AVERROR(EAGAIN))
      ok = true;
    else if (error < 0)
      cLog::log (LOGERROR, "error decode frame");
    else {
      ok = true;
      hasData = true;
      }
    }

  av_packet_unref (&input_packet);
  return ok;
  }
//}}}
//{{{
bool readFileDecodeFrameConvertStoreFifo (AVAudioFifo* fifo,
                                          AVFormatContext* inFormatContext, AVCodecContext* inCodecContext,
                                          AVCodecContext* outCodecContext,
                                          SwrContext* swrContext,
                                          bool& done) {

  bool ok = false;
  uint8_t** convertedSamples = NULL;

  // Initialize temporary storage for one input frame, decode it
  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    //{{{
    cLog::log (LOGERROR, "error allocate input frame");
    goto cleanup;
    }
    //}}}

  bool hasData;
  if (!decodeFrame (frame, inFormatContext, inCodecContext, hasData, done))
    goto cleanup;

  // EOF, no more samples in decoder delayed, we are done but no error
  if (!done) {
    // If decoded data, convert and store it
    if (hasData) {
      // calc max numConvertedSamples
      int numConvertedSamples = (int)av_rescale_rnd (
        frame->nb_samples + swr_get_delay (swrContext, frame->sample_rate),
        OUTPUT_SAMPLE_RATE, frame->sample_rate, AV_ROUND_UP);

      // allocate channel samples pointers, may point to planar data per channel or NULL if interleaved
      convertedSamples = (uint8_t**)calloc (outCodecContext->channels, sizeof(convertedSamples));

      // allocate memory for samples of all channels in one consecutive block
      av_samples_alloc (convertedSamples, NULL, outCodecContext->channels,
        numConvertedSamples, outCodecContext->sample_fmt, 0);

      // Convert input samples to output sample format
      numConvertedSamples = swr_convert (swrContext, convertedSamples, numConvertedSamples,
                                                     (const uint8_t**)frame->extended_data, frame->nb_samples);
      if (numConvertedSamples < 0) {
        //{{{
        cLog::log (LOGERROR, "error convert input samples");
        goto cleanup;
        }
        //}}}

      cLog::log (LOGINFO1, "converted inputSampleRate:%d samples %d to %d",
                           frame->sample_rate, frame->nb_samples, numConvertedSamples);

      // extend fifo to fit in converted samples, store them
      if (av_audio_fifo_realloc (fifo, av_audio_fifo_size (fifo) + numConvertedSamples) < 0) {
        //{{{
        cLog::log (LOGERROR, "error reallocate FIFO");
        goto cleanup;
        }
        //}}}
      if (av_audio_fifo_write (fifo, (void**)convertedSamples, numConvertedSamples) < numConvertedSamples) {
        //{{{
        cLog::log (LOGERROR, "error write FIFO");
        goto cleanup;
        }
        //}}}
      }
    }
  ok = true;

cleanup:
  if (convertedSamples) {
    av_freep (&convertedSamples[0]);
    free (convertedSamples);
    }

  av_frame_free (&frame);
  return ok;
  }
//}}}

//{{{
bool encodeFrame (AVFrame* frame, AVFormatContext* outFormatContext, AVCodecContext* outCodecContext, bool& hasData) {

  bool ok = false;
  hasData = false;

  // Packet used for temporary storage
  AVPacket output_packet;
  av_init_packet (&output_packet);
  output_packet.data = NULL;
  output_packet.size = 0;

  // Set a timestamp based on the sample rate for the container
  if (frame) {
    frame->pts = pts;
    pts += frame->nb_samples;
    }

  // Send the audio frame stored in the temporary packet to the encoder
  // The output audio stream encoder is used to do this The encoder signals that it has nothing more to encode
  int error = avcodec_send_frame (outCodecContext, frame);
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
  error = avcodec_receive_packet (outCodecContext, &output_packet);
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
    if (av_write_frame (outFormatContext, &output_packet) < 0) {
      ok = false;
      cLog::log (LOGERROR, "error write frame");
      }

cleanup:
  av_packet_unref (&output_packet);
  return ok;
  }
//}}}
//{{{
bool readFifoEncodeFrameWriteFile (AVAudioFifo* fifo, AVFormatContext* outFormatContext, AVCodecContext* outCodecContext) {

  //* Temporary storage of the output samples of the frame written to the file
  // Use the maximum number of possible samples per frame.
  // If there is less than the maximum possible frame size in the FIFO
  // buffer use this number. Otherwise, use the maximum possible frame size
  const int frame_size = FFMIN (av_audio_fifo_size(fifo), outCodecContext->frame_size);

  // Initialize temporary storage for one output frame
  // Set the frame's parameters, especially its size and format.
  // av_frame_get_buffer needs this to allocate memory for the audio samples of the frame.
  // Default channel layouts based on the number of channels are assumed for simplicity
  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    //{{{
    cLog::log (LOGERROR, "Could not allocate output frame");
    return false;
    }
    //}}}

  frame->nb_samples = frame_size;
  frame->channel_layout = outCodecContext->channel_layout;
  frame->format = outCodecContext->sample_fmt;
  frame->sample_rate = outCodecContext->sample_rate;

  // Allocate the samples of the created frame. This call will make
  // sure that the audio frame can hold as many samples as specified
  int error = av_frame_get_buffer (frame, 0);
  if (error < 0) {
    //{{{
    cLog::log (LOGERROR, "Could not allocate output frame samples");
    av_frame_free (&frame);
    return false;
    }
    //}}}

  // Read as many samples from the FIFO buffer as required to fill the frame.
  // The samples are stored in the frame temporarily
  if (av_audio_fifo_read (fifo, (void**)frame->data, frame_size) < frame_size) {
    //{{{
    cLog::log (LOGERROR, "Could not read data from FIFO");
    av_frame_free (&frame);
    return false;
    }
    //}}}

  // Encode one frame worth of audio samples
  bool hasData = false;
  if (!encodeFrame (frame, outFormatContext, outCodecContext, hasData)) {
    av_frame_free (&frame);
    return false;
    }

  av_frame_free (&frame);
  return true;
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

  // should convert level ???
  cLog::log (LOGINFO, str);
  }
//}}}
//{{{
int main (int argc, char **argv) {

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "transcode");

  av_log_set_level (AV_LOG_ERROR);
  av_log_set_callback (avLogCallback);

  AVFormatContext* inFormatContext = NULL;
  AVFormatContext* outFormatContext = NULL;
  AVCodecContext* inCodecContext = NULL;
  AVCodecContext* outCodecContext = NULL;
  SwrContext* swrContext = NULL;
  AVAudioFifo* fifo = NULL;

  int ret = AVERROR_EXIT;

  if (argc != 2) {
    //{{{
    cLog::log (LOGINFO, "Usage: %s <input file> <output file>", argv[0]);
    exit(1);
    }
    //}}}

  if (!openInFile (argv[1], inFormatContext, inCodecContext))
    goto cleanup;

  auto len = strlen (argv[1]);
  auto outFilename = (char*)malloc (len);
  strcpy (outFilename, argv[1]);
  outFilename[len-3] = 'a';
  outFilename[len-2] = 'a';
  outFilename[len-1] = 'c';
  if (!openOutFile (outFilename, outFormatContext, outCodecContext))
    goto cleanup;

  // create swr_context for input to output conversion
  swrContext = swr_alloc_set_opts (NULL,
    av_get_default_channel_layout (outCodecContext->channels), outCodecContext->sample_fmt, outCodecContext->sample_rate,
    av_get_default_channel_layout (inCodecContext->channels), inCodecContext->sample_fmt, inCodecContext->sample_rate,
    0, NULL);
  if (!swrContext) {
    //{{{
    cLog::log (LOGERROR, "Could not allocate swr_contextt");
    goto cleanup;
    }
    //}}}
  if (swr_init (swrContext) < 0) {
    //{{{
    cLog::log (LOGERROR, "Could not init swr_context");
    swr_free (&swrContext);
    goto cleanup;
    }
    //}}}

  // init converted samples FIFO
  fifo = av_audio_fifo_alloc (outCodecContext->sample_fmt, outCodecContext->channels, 1);
  if (!fifo) {
    //{{{
    cLog::log (LOGERROR, "Could not allocate FIFO");
    goto cleanup;
    }
    //}}}

  if (avformat_write_header (outFormatContext, NULL) < 0)
    goto cleanup;

  bool inputDone = false;
  while (!inputDone) {
    // while some input and not enough output samples
    while (!inputDone && (av_audio_fifo_size (fifo) < outCodecContext->frame_size))
      if (!readFileDecodeFrameConvertStoreFifo (fifo, inFormatContext, inCodecContext, outCodecContext, swrContext, inputDone))
        goto cleanup;

    // while enough output samples
    while ((inputDone && (av_audio_fifo_size (fifo) > 0) || (av_audio_fifo_size (fifo) >= outCodecContext->frame_size)))
      if (!readFifoEncodeFrameWriteFile (fifo, outFormatContext, outCodecContext))
        goto cleanup;
    }

  // done, flush encoder delayed frames
  bool flushingEncoder = true;
  while (flushingEncoder)
    if (!encodeFrame (NULL, outFormatContext, outCodecContext, flushingEncoder))
      goto cleanup;

  if (av_write_trailer (outFormatContext) >= 0)
    ret = 0;

cleanup:
  cLog::log (LOGINFO, "done");

  if (fifo)
    av_audio_fifo_free (fifo);

  swr_free (&swrContext);

  if (outCodecContext)
    avcodec_free_context (&outCodecContext);

  if (outFormatContext) {
    avio_closep (&outFormatContext->pb);
    avformat_free_context (outFormatContext);
    }

  if (inCodecContext)
    avcodec_free_context (&inCodecContext);

  if (inFormatContext)
    avformat_close_input (&inFormatContext);

  Sleep (5000);
  return ret;
  }
//}}}
