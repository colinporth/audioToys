//{{{
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

/**
 * @file
 * Simple audio converter
 *
 * @example transcode_aac.c
 * Convert an input audio file to AAC in an MP4 container using FFmpeg.
 * Formats other than MP4 are supported based on the output file extension.
 * @author Andreas Unterweger (dustsigns@gmail.com)
 */
//}}}
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>

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
//}}}

#define OUTPUT_CHANNELS 2
#define OUTPUT_BIT_RATE 128000
#define OUTPUT_SAMPLE_RATE 44100

/* Global timestamp for the audio frames. */
static int64_t pts = 0;

//{{{
int openInFile (const char* filename, AVFormatContext*& inFormatContext, AVCodecContext*& inCodecContext) {

  // Open the input file to read from it. */
  int error = avformat_open_input (&inFormatContext, filename, NULL, NULL);
  if (error < 0) {
    //{{{
    fprintf(stderr, "Could not open input file\n");
    inFormatContext = NULL;
    return error;
    }
    //}}}

  // Get information on the input file (number of streams etc.). */
  if ((error = avformat_find_stream_info (inFormatContext, NULL)) < 0) {
    //{{{
    fprintf(stderr, "Could not open find stream info\n");
    avformat_close_input (&inFormatContext);
    return error;
    }
    //}}}

  // Make sure that there is only one stream in the input file. */
  if (inFormatContext->nb_streams != 1) {
    //{{{
    fprintf(stderr, "Expected one audio input stream, but found %d\n", inFormatContext->nb_streams);
    avformat_close_input (&inFormatContext);
    return AVERROR_EXIT;
    }
    //}}}

  // Find a decoder for the audio stream. */
  AVCodec* input_codec = avcodec_find_decoder (inFormatContext->streams[0]->codecpar->codec_id);
  if (!input_codec) {
    //{{{
    fprintf(stderr, "Could not find input codec\n");
    avformat_close_input (&inFormatContext);
    return AVERROR_EXIT;
    }
    //}}}

  // Allocate a new decoding context. */
  inCodecContext = avcodec_alloc_context3 (input_codec);
  if (!inCodecContext) {
    //{{{
    fprintf(stderr, "Could not allocate a decoding context\n");
    avformat_close_input (&inFormatContext);
    return AVERROR(ENOMEM);
    }
    //}}}

  // Initialize the stream parameters with demuxer information. */
  error = avcodec_parameters_to_context (inCodecContext, inFormatContext->streams[0]->codecpar);
  if (error < 0) {
    avformat_close_input (&inFormatContext);
    avcodec_free_context (&inCodecContext);
    return error;
    }

  // Open the decoder for the audio stream to use it later. */
  if ((error = avcodec_open2 (inCodecContext, input_codec, NULL)) < 0) {
    //{{{
    fprintf(stderr, "Could not open input codec \n");
    avcodec_free_context (&inCodecContext);
    avformat_close_input (&inFormatContext);
    return error;
    }
    //}}}

  return 0;
  }
//}}}

//{{{
int openOutFile (const char* filename, int sampleRate, AVFormatContext*& outFormatContext, AVCodecContext*& outCodecContext) {

  // Find the encoder to be used by its name
  AVCodec* output_codec = avcodec_find_encoder (AV_CODEC_ID_AAC);
  outCodecContext = avcodec_alloc_context3 (output_codec);

  // Set the basic encoder parameters, input file's sample rate is used to avoid a sample rate conversion
  outCodecContext->channels = OUTPUT_CHANNELS;
  outCodecContext->channel_layout = av_get_default_channel_layout (OUTPUT_CHANNELS);
  outCodecContext->sample_rate = sampleRate;
  outCodecContext->sample_fmt = output_codec->sample_fmts[0];
  outCodecContext->bit_rate = OUTPUT_BIT_RATE;
  outCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  // Open the output file to write to it. */
  AVIOContext* output_io_context;
  avio_open (&output_io_context, filename, AVIO_FLAG_WRITE);

  // Create a new format context for the output container format
  outFormatContext = avformat_alloc_context();
  outFormatContext->pb = output_io_context;
  outFormatContext->oformat = av_guess_format (NULL, filename, NULL);

  // Create a new audio stream in the output file container. */
  AVStream* stream = avformat_new_stream (outFormatContext, NULL);
  stream->time_base.den = sampleRate;
  stream->time_base.num = 1;

  // Some container formats (like MP4) require global headers to be present
  // Mark the encoder so that it behaves accordingly
  if (outFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
    outCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  // Open the encoder for the audio stream to use it later
  //auto res = av_opt_set (codecContext->priv_data, "profile", "aac_he", 0);
  //printf ("setopt %x\n", res);
  avcodec_open2 (outCodecContext, output_codec, NULL);
  avcodec_parameters_from_context (stream->codecpar, outCodecContext);

  return 0;
  }
//}}}
//{{{
int writeOutFileHeader (AVFormatContext* outFormatContext) {

  int error;
  if ((error = avformat_write_header(outFormatContext, NULL)) < 0) {
    fprintf(stderr, "Could not write output file header\n");
    return error;
    }
  return 0;
  }
//}}}
//{{{
int writeOutFileTrailer (AVFormatContext* outFormatContext) {

  int error;
  if ((error = av_write_trailer (outFormatContext)) < 0) {
    fprintf (stderr, "Could not write output file trailer\n");
    return error;
    }

  return 0;
  }
//}}}

//{{{
int decodeFrame (AVFrame* frame, AVFormatContext* inFormatContext, AVCodecContext* inCodecContext, bool& hasData, bool& done) {

  /* Packet used for temporary storage. */
  int error;

  AVPacket input_packet;
  av_init_packet (&input_packet);
  input_packet.data = NULL;
  input_packet.size = 0;

  /* Read one audio frame from the input file into a temporary packet. */
  if ((error = av_read_frame(inFormatContext, &input_packet)) < 0) {
    /* If we are at the end of the file, flush the decoder below. */
    if (error == AVERROR_EOF)
      done = true;
    else {
      fprintf(stderr, "Could not read frame\n");
      return error;
      }
    }

  /* Send the audio frame stored in the temporary packet to the decoder.
   * The input audio stream decoder is used to do this. */
  if ((error = avcodec_send_packet(inCodecContext, &input_packet)) < 0) {
    fprintf(stderr, "Could not send packet for decoding \n");
    return error;
    }

  /* Receive one frame from the decoder. */
  error = avcodec_receive_frame(inCodecContext, frame);
  /* If the decoder asks for more data to be able to decode a frame,
   * return indicating that no data is present. */
  if (error == AVERROR(EAGAIN)) {
    error = 0;
    goto cleanup;
    /* If the end of the input file is reached, stop decoding. */
    }
  else if (error == AVERROR_EOF) {
    done = true;
    error = 0;
    goto cleanup;
    }
  else if (error < 0) {
    fprintf(stderr, "Could not decode frame\n");
    goto cleanup;
    /* Default case: Return decoded data. */
    }
  else {
    hasData = true;
    goto cleanup;
    }

  cleanup:
  av_packet_unref(&input_packet);
  return error;
  }
//}}}
//{{{
int readDecodeConvertStoreFifo (AVAudioFifo* fifo,
                                AVFormatContext* inFormatContext, AVCodecContext* inCodecContext,
                                AVCodecContext* outCodecContext,
                                SwrContext* resampler_context,
                                bool& done) {

  int ret = AVERROR_EXIT;
  uint8_t** convertedSamples = NULL;

  // Initialize temporary storage for one input frame, decode it
  AVFrame* input_frame = av_frame_alloc();
  if (!input_frame) {
    //{{{
    fprintf(stderr, "Could not allocate input frame\n");
    goto cleanup;
    }
    //}}}
  bool hasData = false;
  if (decodeFrame (input_frame, inFormatContext, inCodecContext, hasData, done))
    goto cleanup;

  // If at EOF and no more samples in decoder delayed, we are actually don but not an error
  if (done) {
    ret = 0;
    goto cleanup;
    }

  // If decoded data, convert and store it
  if (hasData) {
    // calc max size for converted frame samples
    int convertedFrameSize = (int)av_rescale_rnd (
      input_frame->nb_samples + swr_get_delay (resampler_context, input_frame->sample_rate),
                                               OUTPUT_SAMPLE_RATE, input_frame->sample_rate, AV_ROUND_UP);

    // Allocate audio channel pointers, may point to planar data per channel, NULL if interleaved
    convertedSamples = (uint8_t**)calloc (outCodecContext->channels, sizeof(convertedSamples));

    // Allocate memory for the samples of all channels in one consecutive block for convenience. */
    av_samples_alloc (convertedSamples, NULL, outCodecContext->channels,
                      convertedFrameSize, outCodecContext->sample_fmt, 0);

    // Convert input samples to output sample format.
    convertedFrameSize = swr_convert (resampler_context,
                                      convertedSamples, convertedFrameSize,
                                      (const uint8_t**)input_frame->extended_data, input_frame->nb_samples);
    if (convertedFrameSize < 0) {
      //{{{
      fprintf (stderr, "Could not convert input samples\n");
      goto cleanup;
      }
      //}}}

    printf ("converted inputSampleRate:%d from %d to %d\n",
            input_frame->sample_rate, input_frame->nb_samples, convertedFrameSize);

    // extend fifo to converted samples, and store them
    if (av_audio_fifo_realloc (fifo, av_audio_fifo_size (fifo) + convertedFrameSize) < 0) {
      //{{{
      fprintf (stderr, "Could not reallocate FIFO\n");
      goto cleanup;
      }
      //}}}
    if (av_audio_fifo_write (fifo, (void**)convertedSamples, convertedFrameSize) < convertedFrameSize) {
      //{{{
      fprintf (stderr, "Could not write data to FIFO\n");
      goto cleanup;
      }
      //}}}
    }

  ret = 0;

cleanup:
  if (convertedSamples) {
    av_freep (&convertedSamples[0]);
    free (convertedSamples);
    }

  av_frame_free (&input_frame);
  return ret;
  }
//}}}

//{{{
int encodeFrame (AVFrame* frame, AVFormatContext* outFormatContext, AVCodecContext* outCodecContext, bool& hasData) {

  // Packet used for temporary storage. */
  AVPacket output_packet;
  av_init_packet (&output_packet);
  output_packet.data = NULL;
  output_packet.size = 0;

  // Set a timestamp based on the sample rate for the container. */
  if (frame) {
    frame->pts = pts;
    pts += frame->nb_samples;
    }

  // Send the audio frame stored in the temporary packet to the encoder.
  // The output audio stream encoder is used to do this. */
  // The encoder signals that it has nothing more to encode. */
  int error = avcodec_send_frame (outCodecContext, frame);
  if (error == AVERROR_EOF) {
    error = 0;
    goto cleanup;
    }
  else if (error < 0) {
    fprintf(stderr, "Could not send packet for encoding\n");
    return error;
    }

  // Receive one encoded frame from the encoder. */
  // If the encoder asks for more data to be able to provide an
  // encoded frame, return indicating that no data is present. */
  error = avcodec_receive_packet (outCodecContext, &output_packet);
  if (error == AVERROR(EAGAIN)) {
    error = 0;
    goto cleanup;
    // If the last frame has been encoded, stop encoding. */
    }
  else if (error == AVERROR_EOF) {
    error = 0;
    goto cleanup;
    }
  else if (error < 0) {
    fprintf (stderr, "Could not encode frame\n");
    goto cleanup;
    // Default case: Return encoded data. */
    }
  else {
    hasData = true;
    }

  // Write one audio frame from the temporary packet to the output file. */
  if (hasData && (error = av_write_frame (outFormatContext, &output_packet)) < 0) {
    fprintf (stderr, "Could not write frame\n");
    goto cleanup;
    }

cleanup:
  av_packet_unref (&output_packet);
  return error;
  }
//}}}
//{{{
bool readFifoEncodeWrite (AVAudioFifo* fifo, AVFormatContext* outFormatContext, AVCodecContext* outCodecContext) {

  //* Temporary storage of the output samples of the frame written to the file
  // Use the maximum number of possible samples per frame.
  // If there is less than the maximum possible frame size in the FIFO
  // buffer use this number. Otherwise, use the maximum possible frame size
  const int frame_size = FFMIN (av_audio_fifo_size(fifo), outCodecContext->frame_size);

  // Initialize temporary storage for one output frame
  // Set the frame's parameters, especially its size and format.
  // av_frame_get_buffer needs this to allocate memory for the audio samples of the frame.
  // Default channel layouts based on the number of channels are assumed for simplicity
  AVFrame* output_frame = av_frame_alloc();
  if (!output_frame) {
    //{{{
    fprintf (stderr, "Could not allocate output frame\n");
    return false;
    }
    //}}}
  output_frame->nb_samples = frame_size;
  output_frame->channel_layout = outCodecContext->channel_layout;
  output_frame->format = outCodecContext->sample_fmt;
  output_frame->sample_rate = outCodecContext->sample_rate;

  // Allocate the samples of the created frame. This call will make
  // sure that the audio frame can hold as many samples as specified
  int error = av_frame_get_buffer (output_frame, 0);
  if (error < 0) {
    //{{{
    fprintf (stderr, "Could not allocate output frame samples\n");
    av_frame_free (&output_frame);
    return false;
    }
    //}}}

  // Read as many samples from the FIFO buffer as required to fill the frame.
  // The samples are stored in the frame temporarily
  if (av_audio_fifo_read (fifo, (void**)output_frame->data, frame_size) < frame_size) {
    //{{{
    fprintf (stderr, "Could not read data from FIFO\n");
    av_frame_free (&output_frame);
    return false;
    }
    //}}}

  // Encode one frame worth of audio samples
  bool hasData = false;
  if (encodeFrame (output_frame, outFormatContext, outCodecContext, hasData)) {
    av_frame_free (&output_frame);
    return false;
    }

  av_frame_free (&output_frame);

  return true;
  }
//}}}

//{{{
int main (int argc, char **argv) {

  AVFormatContext* inFormatContext = NULL;
  AVFormatContext* outFormatContext = NULL;
  AVCodecContext* inCodecContext = NULL;
  AVCodecContext* outCodecContext = NULL;
  SwrContext* swrContext = NULL;
  AVAudioFifo* fifo = NULL;

  int ret = AVERROR_EXIT;

  if (argc != 2) {
    //{{{
    fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
    exit(1);
    }
    //}}}

  auto len = strlen (argv[1]);
  auto outFilename = (char*)malloc (len);
  strcpy (outFilename, argv[1]);
  outFilename[len-3] = 'a';
  outFilename[len-2] = 'a';
  outFilename[len-1] = 'c';

  // Open the input file for reading
  if (openInFile (argv[1], inFormatContext, inCodecContext))
    goto cleanup;

  // Open the output file for writing
  if (openOutFile (outFilename, OUTPUT_SAMPLE_RATE, outFormatContext, outCodecContext))
    goto cleanup;

  // Create a resampler context for the input to output conversion
  swrContext = swr_alloc_set_opts (NULL,
    av_get_default_channel_layout(outCodecContext->channels), outCodecContext->sample_fmt, outCodecContext->sample_rate,
    av_get_default_channel_layout(inCodecContext->channels), inCodecContext->sample_fmt, inCodecContext->sample_rate,
    0, NULL);
  if (!swrContext) {
    //{{{
    fprintf (stderr, "Could not allocate resample context\n");
    goto cleanup;
    }
    //}}}
  if (swr_init (swrContext) < 0) {
    //{{{
    fprintf (stderr, "Could not open resample context\n");
    swr_free (&swrContext);
    goto cleanup;
    }
    //}}}

  // Initialize sample FIFO
  fifo = av_audio_fifo_alloc (outCodecContext->sample_fmt, outCodecContext->channels, 1);
  if (!fifo) {
    //{{{
    fprintf (stderr, "Could not allocate FIFO\n");
    goto cleanup;
    }
    //}}}

  // Write the header of the output file container. */
  if (writeOutFileHeader (outFormatContext))
    goto cleanup;

  // Loop as long as we have input samples to read or output samples to write; abort as soon as we have neither. */
  while (true) {
    // Use the encoder's desired frame size for processing. */
    const int output_frame_size = outCodecContext->frame_size;
    bool done = false;

    // Make sure that there is one frame worth of samples in the FIFO buffer so that the encoder can do its work.
    // Since the decoder's and the encoder's frame size may differ, we need to FIFO buffer to store as many frames worth of input samples
    // that they make up at least one frame worth of output samples. */
    while (av_audio_fifo_size(fifo) < output_frame_size) {
      // Decode one frame worth of audio samples, convert it to the output sample format and put it into the FIFO buffer. */
      if (readDecodeConvertStoreFifo (
            fifo, inFormatContext, inCodecContext, outCodecContext, swrContext, done))
        goto cleanup;

      // If we are at the end of the input file, we continue  encoding the remaining audio samples to the output file. */
      if (done)
        break;
      }

    // If we have enough samples for the encoder, we encode them.
    // At the end of the file, we pass the remaining samples to the encoder. */
    while ((av_audio_fifo_size (fifo) >= output_frame_size) || (done && (av_audio_fifo_size (fifo) > 0)))
      // process a frame of audio samples from FIFO
      if (!readFifoEncodeWrite (fifo, outFormatContext, outCodecContext))
        goto cleanup;

    // If we are at the end of the input file and have encoded all remaining samples, we can exit this loop and finish. */
    if (done) {
      bool data_written;
      do {
        // Flush the encoder as it may have delayed frames. */
        data_written = false;
        if (encodeFrame (NULL, outFormatContext, outCodecContext, data_written))
          goto cleanup;
        } while (data_written);
      break;
      }
    }

  if (writeOutFileTrailer (outFormatContext))
    goto cleanup;

  ret = 0;

cleanup:
  if (fifo)
    av_audio_fifo_free(fifo);

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

  return ret;
  }
//}}}
