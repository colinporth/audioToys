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
#define OUTPUT_BIT_RATE 64000
#define OUTPUT_SAMPLE_RATE 44100

/* Global timestamp for the audio frames. */
static int64_t pts = 0;

//{{{
void init_packet (AVPacket* packet) {

  av_init_packet (packet);
  packet->data = NULL;
  packet->size = 0;
  }
//}}}
//{{{
int init_input_frame (AVFrame** frame) {

  if (!(*frame = av_frame_alloc())) {
    fprintf(stderr, "Could not allocate input frame\n");
    return AVERROR(ENOMEM);
    }

  return 0;
  }
//}}}
//{{{
int init_output_frame (AVFrame** frame, AVCodecContext* output_codec_context, int frame_size) {


  /* Create a new frame to store the audio samples. */
  if (!(*frame = av_frame_alloc())) {
    fprintf(stderr, "Could not allocate output frame\n");
    return AVERROR_EXIT;
    }

  /* Set the frame's parameters, especially its size and format.
   * av_frame_get_buffer needs this to allocate memory for the
   * audio samples of the frame.
   * Default channel layouts based on the number of channels
   * are assumed for simplicity. */
  (*frame)->nb_samples     = frame_size;
  (*frame)->channel_layout = output_codec_context->channel_layout;
  (*frame)->format         = output_codec_context->sample_fmt;
  (*frame)->sample_rate    = output_codec_context->sample_rate;

  /* Allocate the samples of the created frame. This call will make
   * sure that the audio frame can hold as many samples as specified. */
  int error;
  if ((error = av_frame_get_buffer (*frame, 0)) < 0) {
    fprintf(stderr, "Could not allocate output frame samples\n");
    av_frame_free (frame);
    return error;
    }

  return 0;
  }
//}}}
//{{{
int init_resampler (AVCodecContext* input_codec_context, AVCodecContext* output_codec_context, SwrContext** resample_context) {

  /*
   * Create a resampler context for the conversion.
   * Set the conversion parameters.
   * Default channel layouts based on the number of channels are assumed for simplicity (they are sometimes not detected
   * properly by the demuxer and/or decoder).
   */
  *resample_context = swr_alloc_set_opts (NULL,
    av_get_default_channel_layout(output_codec_context->channels), output_codec_context->sample_fmt, output_codec_context->sample_rate,
    av_get_default_channel_layout(input_codec_context->channels), input_codec_context->sample_fmt, input_codec_context->sample_rate,
    0, NULL);

  if (!*resample_context) {
    fprintf (stderr, "Could not allocate resample context\n");
    return AVERROR(ENOMEM);
    }

  /*
  * Perform a sanity check so that the number of converted samples is
  * not greater than the number of samples to be converted.
  * If the sample rates differ, this case has to be handled differently
  */
  //av_assert0 (output_codec_context->sample_rate == input_codec_context->sample_rate);

  /* Open the resampler with the specified parameters. */
  int error;
  if ((error = swr_init(*resample_context)) < 0) {
    fprintf(stderr, "Could not open resample context\n");
    swr_free(resample_context);
    return error;
    }

  return 0;
  }
//}}}
//{{{
int init_fifo (AVAudioFifo** fifo, AVCodecContext* output_codec_context) {

  /* Create the FIFO buffer based on the specified output sample format. */
  if (!(*fifo = av_audio_fifo_alloc (output_codec_context->sample_fmt, output_codec_context->channels, 1))) {
    fprintf (stderr, "Could not allocate FIFO\n");
    return AVERROR (ENOMEM);
    }

  return 0;
  }
//}}}
//{{{
int init_converted_samples (uint8_t*** convertedSamples, AVCodecContext* output_codec_context, int frame_size)
{
  int error;

  /* Allocate as many pointers as there are audio channels.
   * Each pointer will later point to the audio samples of the corresponding channels (although it may be NULL for interleaved formats).
   */
  if (!(*convertedSamples = (uint8_t**)calloc (output_codec_context->channels, sizeof(**convertedSamples)))) {
    fprintf(stderr, "Could not allocate converted input sample pointers\n");
    return AVERROR(ENOMEM);
    }

  /* Allocate memory for the samples of all channels in one consecutive block for convenience. */
  if ((error = av_samples_alloc (*convertedSamples, NULL, output_codec_context->channels,
                                 frame_size, output_codec_context->sample_fmt, 0)) < 0) {
    fprintf (stderr, "Could not allocate converted input samples\n" );
    av_freep (&(*convertedSamples)[0]);
    free (*convertedSamples);
    return error;
    }

  return 0;
  }
//}}}

//{{{
int open_input_file (const char* filename, AVFormatContext** input_format_context, AVCodecContext** input_codec_context)
{
  AVCodecContext *avctx;
  AVCodec *input_codec;
  int error;

  /* Open the input file to read from it. */
  if ((error = avformat_open_input (input_format_context, filename, NULL, NULL)) < 0) {
    //{{{
    fprintf(stderr, "Could not open input file\n");
    *input_format_context = NULL;
    return error;
    }
    //}}}

  /* Get information on the input file (number of streams etc.). */
  if ((error = avformat_find_stream_info (*input_format_context, NULL)) < 0) {
    //{{{
    fprintf(stderr, "Could not open find stream info\n");
    avformat_close_input(input_format_context);
    return error;
    }
    //}}}

  /* Make sure that there is only one stream in the input file. */
  if ((*input_format_context)->nb_streams != 1) {
    //{{{
    fprintf(stderr, "Expected one audio input stream, but found %d\n", (*input_format_context)->nb_streams);
    avformat_close_input(input_format_context);
    return AVERROR_EXIT;
    }
    //}}}

  /* Find a decoder for the audio stream. */
  if (!(input_codec = avcodec_find_decoder ((*input_format_context)->streams[0]->codecpar->codec_id))) {
    //{{{
    fprintf(stderr, "Could not find input codec\n");
    avformat_close_input(input_format_context);
    return AVERROR_EXIT;
    }
    //}}}

  /* Allocate a new decoding context. */
  avctx = avcodec_alloc_context3 (input_codec);
  if (!avctx) {
    //{{{
    fprintf(stderr, "Could not allocate a decoding context\n");
    avformat_close_input(input_format_context);
    return AVERROR(ENOMEM);
    }
    //}}}

  /* Initialize the stream parameters with demuxer information. */
  error = avcodec_parameters_to_context (avctx, (*input_format_context)->streams[0]->codecpar);
  if (error < 0) {
    //{{{
    avformat_close_input (input_format_context);
    avcodec_free_context(&avctx);
    return error;
    }
    //}}}

  /* Open the decoder for the audio stream to use it later. */
  if ((error = avcodec_open2 (avctx, input_codec, NULL)) < 0) {
    //{{{
    fprintf(stderr, "Could not open input codec \n");
    avcodec_free_context(&avctx);
    avformat_close_input(input_format_context);
    return error;
    }
    //}}}

  /* Save the decoder context for easier access later. */
  *input_codec_context = avctx;

  return 0;
}
//}}}
//{{{
int decode_audio_frame (AVFrame* frame, AVFormatContext* input_format_context, AVCodecContext* input_codec_context,
                        int *data_present, int *finished) {

  /* Packet used for temporary storage. */
  AVPacket input_packet;
  int error;
  init_packet(&input_packet);

  /* Read one audio frame from the input file into a temporary packet. */
  if ((error = av_read_frame(input_format_context, &input_packet)) < 0) {
    /* If we are at the end of the file, flush the decoder below. */
    if (error == AVERROR_EOF)
      *finished = 1;
    else {
      fprintf(stderr, "Could not read frame\n");
      return error;
      }
    }

  /* Send the audio frame stored in the temporary packet to the decoder.
   * The input audio stream decoder is used to do this. */
  if ((error = avcodec_send_packet(input_codec_context, &input_packet)) < 0) {
    fprintf(stderr, "Could not send packet for decoding \n");
    return error;
    }

  /* Receive one frame from the decoder. */
  error = avcodec_receive_frame(input_codec_context, frame);
  /* If the decoder asks for more data to be able to decode a frame,
   * return indicating that no data is present. */
  if (error == AVERROR(EAGAIN)) {
    error = 0;
    goto cleanup;
    /* If the end of the input file is reached, stop decoding. */
    }
  else if (error == AVERROR_EOF) {
    *finished = 1;
    error = 0;
    goto cleanup;
    }
  else if (error < 0) {
    fprintf(stderr, "Could not decode frame\n");
    goto cleanup;
    /* Default case: Return decoded data. */
    }
  else {
    *data_present = 1;
    goto cleanup;
    }

  cleanup:
  av_packet_unref(&input_packet);
  return error;
  }
//}}}

//{{{
int open_output_file (const char* filename, int sampleRate,
                      AVFormatContext** output_format_context, AVCodecContext** output_codec_context) {

  /* Find the encoder to be used by its name. */
  AVCodec* output_codec = avcodec_find_encoder (AV_CODEC_ID_AAC);
  AVCodecContext* codecContext = avcodec_alloc_context3 (output_codec);

  /* Set the basic encoder parameters, input file's sample rate is used to avoid a sample rate conversion. */
  codecContext->channels = OUTPUT_CHANNELS;
  codecContext->channel_layout = av_get_default_channel_layout (OUTPUT_CHANNELS);
  codecContext->sample_rate = sampleRate;
  codecContext->sample_fmt = output_codec->sample_fmts[0];
  codecContext->bit_rate = OUTPUT_BIT_RATE;
  codecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  /* Open the output file to write to it. */
  AVIOContext* output_io_context;
  avio_open (&output_io_context, filename, AVIO_FLAG_WRITE);

  /* Create a new format context for the output container format. */
  AVFormatContext* format_context = avformat_alloc_context();
  format_context->pb = output_io_context;
  format_context->oformat = av_guess_format (NULL, filename, NULL);

  /* Create a new audio stream in the output file container. */
  AVStream* stream = avformat_new_stream (format_context, NULL);
  stream->time_base.den = sampleRate;
  stream->time_base.num = 1;

  /* Some container formats (like MP4) require global headers to be present.
   * Mark the encoder so that it behaves accordingly. */
  if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
    codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  /* Open the encoder for the audio stream to use it later. */
  avcodec_open2 (codecContext, output_codec, NULL);
  avcodec_parameters_from_context (stream->codecpar, codecContext);

  /* Save the encoder context for easier access later. */
  *output_codec_context = codecContext;
  *output_format_context = format_context;

  return 0;
  }
//}}}
//{{{
int write_output_file_header (AVFormatContext* output_format_context) {

  int error;
  if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
    fprintf(stderr, "Could not write output file header\n");
    return error;
    }
  return 0;
  }
//}}}
//{{{
int write_output_file_trailer (AVFormatContext* output_format_context) {

  int error;
  if ((error = av_write_trailer (output_format_context)) < 0) {
    fprintf (stderr, "Could not write output file trailer\n");
    return error;
    }

  return 0;
  }
//}}}

//{{{
int convert_samples (const uint8_t** input_data, const int input_frame_size,
                     uint8_t** converted_data, const int output_frame_size,
                     SwrContext* resample_context) {

  int error;
  if ((error = swr_convert (resample_context, converted_data, output_frame_size, input_data, input_frame_size)) < 0) {
    fprintf (stderr, "Could not convert input samples\n");
    return error;
    }

  return 0;
  }
//}}}
//{{{
int read_decode_convert_and_store (AVAudioFifo* fifo,
                                   AVFormatContext* input_format_context, AVCodecContext* input_codec_context,
                                   AVCodecContext* output_codec_context, 
                                   SwrContext* resampler_context,
                                   int* finished) {

  /* Temporary storage for the converted input samples. */
  int ret = AVERROR_EXIT;

  uint8_t** convertedSamples = NULL;

  /* Initialize temporary storage for one input frame. */
  AVFrame* input_frame = NULL;
  if (init_input_frame (&input_frame))
    goto cleanup;

  /* Decode one frame worth of audio samples. */
  int data_present = 0;
  if (decode_audio_frame (input_frame, input_format_context, input_codec_context, &data_present, finished))
    goto cleanup;

  /* If we are at the end of the file and there are no more samples
   * in the decoder which are delayed, we are actually finished.
   * This must not be treated as an error. */
  if (*finished) {
    ret = 0;
    goto cleanup;
    }

  /* If there is decoded data, convert and store it. */
  if (data_present) {
    /* Initialize the temporary storage for the converted input samples. */
    const int converted_frame_size = input_frame->nb_samples;
    if (init_converted_samples (&convertedSamples, output_codec_context, converted_frame_size))
      goto cleanup;

    /* Convert the input samples to the desired output sample format.
     * This requires a temporary storage provided by convertedSamples. */
    /* Add the converted input samples to the FIFO buffer for later processing. */
    if (convert_samples ((const uint8_t**)input_frame->extended_data, input_frame->nb_samples,
                         convertedSamples, converted_frame_size,
                         resampler_context))
      goto cleanup;


    /* Make the FIFO as large as it needs to be to hold both, * the old and the new samples. */
    int error;
    if ((error = av_audio_fifo_realloc (fifo, av_audio_fifo_size (fifo) + converted_frame_size)) < 0) {
      fprintf (stderr, "Could not reallocate FIFO\n");
      goto cleanup;
      }

    /* Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write (fifo, (void**)convertedSamples, converted_frame_size) < converted_frame_size) {
      fprintf (stderr, "Could not write data to FIFO\n");
      goto cleanup;
      }
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
int encode_audio_frame (AVFrame* frame,
                        AVFormatContext* output_format_context, AVCodecContext* output_codec_context,
                        int* data_present) {

  /* Packet used for temporary storage. */
  AVPacket output_packet;
  init_packet (&output_packet);

  /* Set a timestamp based on the sample rate for the container. */
  if (frame) {
    frame->pts = pts;
    pts += frame->nb_samples;
    }

  /* Send the audio frame stored in the temporary packet to the encoder.
   * The output audio stream encoder is used to do this. */
  /* The encoder signals that it has nothing more to encode. */
  int error = avcodec_send_frame (output_codec_context, frame);
  if (error == AVERROR_EOF) {
    error = 0;
    goto cleanup;
    }
  else if (error < 0) {
    fprintf(stderr, "Could not send packet for encoding\n");
    return error;
    }

  /* Receive one encoded frame from the encoder. */
  /* If the encoder asks for more data to be able to provide an
   * encoded frame, return indicating that no data is present. */
  error = avcodec_receive_packet (output_codec_context, &output_packet);
  if (error == AVERROR(EAGAIN)) {
    error = 0;
    goto cleanup;
    /* If the last frame has been encoded, stop encoding. */
    }
  else if (error == AVERROR_EOF) {
    error = 0;
    goto cleanup;
    }
  else if (error < 0) {
    fprintf (stderr, "Could not encode frame\n");
    goto cleanup;
    /* Default case: Return encoded data. */
    }
  else {
    *data_present = 1;
    }

  /* Write one audio frame from the temporary packet to the output file. */
  if (*data_present &&
    (error = av_write_frame (output_format_context, &output_packet)) < 0) {
    fprintf (stderr, "Could not write frame\n");
    goto cleanup;
    }

cleanup:
  av_packet_unref (&output_packet);
  return error;
  }
//}}}
//{{{
int load_encode_and_write (AVAudioFifo* fifo, AVFormatContext* output_format_context, AVCodecContext* output_codec_context) {

  /* Temporary storage of the output samples of the frame written to the file. */

  /* Use the maximum number of possible samples per frame.
   * If there is less than the maximum possible frame size in the FIFO
   * buffer use this number. Otherwise, use the maximum possible frame size. */
  const int frame_size = FFMIN(av_audio_fifo_size(fifo), output_codec_context->frame_size);

  /* Initialize temporary storage for one output frame. */
  AVFrame* output_frame;
  if (init_output_frame (&output_frame, output_codec_context, frame_size))
    return AVERROR_EXIT;

  /* Read as many samples from the FIFO buffer as required to fill the frame.
   * The samples are stored in the frame temporarily. */
  if (av_audio_fifo_read (fifo, (void **)output_frame->data, frame_size) < frame_size) {
    fprintf(stderr, "Could not read data from FIFO\n");
    av_frame_free (&output_frame);
    return AVERROR_EXIT;
    }

  /* Encode one frame worth of audio samples. */
  int data_written;
  if (encode_audio_frame (output_frame, output_format_context, output_codec_context, &data_written)) {
    av_frame_free (&output_frame);
    return AVERROR_EXIT;
    }

  av_frame_free (&output_frame);
  return 0;
  }
//}}}

//{{{
int main (int argc, char **argv) {

  AVFormatContext *input_format_context = NULL, *output_format_context = NULL;
  AVCodecContext *input_codec_context = NULL, *output_codec_context = NULL;
  SwrContext *resample_context = NULL;
  AVAudioFifo *fifo = NULL;
  int ret = AVERROR_EXIT;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
    exit(1);
    }

  auto len = strlen (argv[1]);
  auto outFilename = (char*)malloc (len);
  strcpy (outFilename, argv[1]);
  outFilename[len-3] = 'a';
  outFilename[len-2] = 'a';
  outFilename[len-1] = 'c';

  /* Open the input file for reading. */
  if (open_input_file (argv[1], &input_format_context, &input_codec_context))
    goto cleanup;

  /* Open the output file for writing. */
  if (open_output_file (outFilename, input_codec_context->sample_rate, &output_format_context, &output_codec_context))
    goto cleanup;

  /* Initialize the resampler to be able to convert audio sample formats. */
  if (init_resampler (input_codec_context, output_codec_context, &resample_context))
    goto cleanup;

  /* Initialize the FIFO buffer to store audio samples to be encoded. */
  if (init_fifo (&fifo, output_codec_context))
    goto cleanup;

  /* Write the header of the output file container. */
  if (write_output_file_header (output_format_context))
    goto cleanup;

  /* Loop as long as we have input samples to read or output samples to write; abort as soon as we have neither. */
  while (true) {
    /* Use the encoder's desired frame size for processing. */
    const int output_frame_size = output_codec_context->frame_size;
    int finished = 0;

    /* Make sure that there is one frame worth of samples in the FIFO buffer so that the encoder can do its work.
     * Since the decoder's and the encoder's frame size may differ, we need to FIFO buffer to store as many frames worth of input samples
     * that they make up at least one frame worth of output samples. */
    while (av_audio_fifo_size(fifo) < output_frame_size) {
      /* Decode one frame worth of audio samples, convert it to the output sample format and put it into the FIFO buffer. */
      if (read_decode_convert_and_store (fifo,
                                         input_format_context, input_codec_context,
                                         output_codec_context, resample_context,
                                         &finished))
        goto cleanup;

      /* If we are at the end of the input file, we continue  encoding the remaining audio samples to the output file. */
      if (finished)
        break;
      }

    /* If we have enough samples for the encoder, we encode them.
     * At the end of the file, we pass the remaining samples to the encoder. */
    while (av_audio_fifo_size(fifo) >= output_frame_size || (finished && av_audio_fifo_size(fifo) > 0))
      /* Take one frame worth of audio samples from the FIFO bufferencode it and write it to the output file. */
      if (load_encode_and_write (fifo, output_format_context, output_codec_context))
        goto cleanup;

    /* If we are at the end of the input file and have encoded all remaining samples, we can exit this loop and finish. */
    if (finished) {
      int data_written;
      /* Flush the encoder as it may have delayed frames. */
      do {
        data_written = 0;
        if (encode_audio_frame (NULL, output_format_context, output_codec_context, &data_written))
          goto cleanup;
        } while (data_written);
      break;
      }
    }

  /* Write the trailer of the output file container. */
  if (write_output_file_trailer (output_format_context))
    goto cleanup;

  ret = 0;

cleanup:
  if (fifo)
    av_audio_fifo_free(fifo);

  swr_free(&resample_context);

  if (output_codec_context)
    avcodec_free_context(&output_codec_context);

  if (output_format_context) {
    avio_closep(&output_format_context->pb);
    avformat_free_context(output_format_context);
    }

  if (input_codec_context)
    avcodec_free_context(&input_codec_context);

  if (input_format_context)
    avformat_close_input(&input_format_context);

  return ret;
  }
//}}}
