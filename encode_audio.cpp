//{{{
/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * audio encoding with libavcodec API example.
 *
 * @example encode_audio.c
 */
//}}}
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

//#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/channel_layout.h>
  #include <libavutil/common.h>
  #include <libavutil/frame.h>
  #include <libavutil/samplefmt.h>
  }
//}}}

//{{{
int write_adts_muxed_data (void* opaque, uint8_t* adts_data, int size) {

  FILE* encoded_audio_file = (FILE*)opaque;
  fwrite (adts_data, 1, size, encoded_audio_file);
  return size;
  }
//}}}
//{{{
int mux_aac_packet_to_adts (AVPacket* pkt, AVFormatContext* adts_container_ctx) {

  int ret_val;
  if ((ret_val = av_write_frame (adts_container_ctx, pkt)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error calling av_write_frame()%d')\n", ret_val);
    }
  else {
    av_log(NULL, AV_LOG_INFO, "Encoded AAC packet %d, size=%d", pkt, pkt->size);
    }
  return ret_val;
  }
//}}}

//{{{
int check_sample_fmt (const AVCodec* codec, enum AVSampleFormat sample_fmt) {

  int res = 0;
  const enum AVSampleFormat* p = codec->sample_fmts;
  while (*p != AV_SAMPLE_FMT_NONE) {
    printf ("sample format %d %d\n", *p, sample_fmt);
    if (*p == sample_fmt)
      res = 1;
    p++;
    }

  return res;
  }
//}}}
//{{{
/* just pick the highest supported samplerate */
int select_sample_rate (const AVCodec* codec) {

  const int *p;
  int best_samplerate = 0;

  if (!codec->supported_samplerates)
    return 44100;

  p = codec->supported_samplerates;
  while (*p) {
    printf ("sampleRates %d\n", *p);
    if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
      best_samplerate = *p;
    p++;
    }

  return best_samplerate;
  }
//}}}
//{{{
/* select layout with the highest channel count */
int select_channel_layout (const AVCodec* codec) {

  if (!codec->channel_layouts)
    return AV_CH_LAYOUT_STEREO;

  int best_nb_channels = 0;
  uint64_t best_ch_layout = 0;
  const uint64_t* p = codec->channel_layouts;
  while (*p) {
    int nb_channels = av_get_channel_layout_nb_channels (*p);
    printf ("channel layouts %lld %d\n", *p, nb_channels);
    if (nb_channels > best_nb_channels) {
      best_ch_layout = *p;
      best_nb_channels = nb_channels;
      }
    p++;
    }

  return best_ch_layout;
  }
//}}}
//{{{
void encode (AVCodecContext* context, AVFrame* frame, AVPacket* pkt, FILE* f, AVFormatContext* adts_container_ctx) {

  if (avcodec_send_frame (context, frame) >= 0) {
    int ret = 0;
    while (ret >= 0) {
      ret = avcodec_receive_packet (context, pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return;
      else if (ret < 0) {
        fprintf (stderr, "Error encoding audio frame\n");
        exit(1);
        }

      if ((mux_aac_packet_to_adts (pkt, adts_container_ctx)) < 0) {
        fprintf (stderr, "Error mux_aac_packet_to_adts\n");
        exit(1);
        }
      //fwrite (pkt->data, 1, pkt->size, f);
      av_packet_unref (pkt);
      }
    }
  }
//}}}

int main (int argc, char **argv) {

  //auto f = fopen ("nnn.mp3", "wb");
  auto f = fopen ("nnn.aac", "wb");

  av_register_all();
  auto codec = avcodec_find_encoder (AV_CODEC_ID_AAC);
  //auto codec = avcodec_find_encoder (AV_CODEC_ID_MP3);
  auto context = avcodec_alloc_context3 (codec);

  // put sample parameters */
  context->bit_rate = 64000;
  //context->sample_fmt = AV_SAMPLE_FMT_S16P;
  context->sample_fmt = AV_SAMPLE_FMT_FLTP;
  if (!check_sample_fmt (codec, context->sample_fmt)) {
    //{{{  error
    fprintf(stderr, "Encoder does not support sample format %s", av_get_sample_fmt_name(context->sample_fmt));
    exit(1);
    }
    //}}}
  context->sample_rate = select_sample_rate (codec);
  context->channel_layout = select_channel_layout (codec);
  context->channels = av_get_channel_layout_nb_channels (context->channel_layout);

  avcodec_open2 (context, codec, NULL);

  auto pkt = av_packet_alloc();

  auto  frame = av_frame_alloc();
  frame->nb_samples = context->frame_size;
  frame->format = context->sample_fmt;
  frame->channel_layout = context->channel_layout;
  av_frame_get_buffer (frame, 0);

  //  Create the ADTS container for the encoded frames
  AVOutputFormat* adts_container = av_guess_format ("adts", NULL, NULL);
  if (!adts_container) {
    //{{{
    av_log (NULL, AV_LOG_ERROR, "Could not find adts output format\n");
    exit (1);
    }
    //}}}

  AVFormatContext* adts_container_ctx = NULL;
  if ((avformat_alloc_output_context2 (&adts_container_ctx, adts_container, "", NULL)) < 0) {
    //{{{
    av_log (NULL, AV_LOG_ERROR, "Could not create output context\n");
    exit(1);
    }
    //}}}

  uint8_t* adts_container_buffer = NULL;
  size_t adts_container_buffer_size = 4096;
  adts_container_buffer = (uint8_t*)av_malloc (adts_container_buffer_size);

  // Create an I/O context for the adts container with a write callback (write_adts_muxed_data()), so that muxed data will be accessed through this function and can be managed by the user.
  AVIOContext* adts_avio_ctx = avio_alloc_context (adts_container_buffer, adts_container_buffer_size,
                                                   1, f, NULL, &write_adts_muxed_data, NULL);

  // Link the container's context to the previous I/O context
  adts_container_ctx->pb = adts_avio_ctx;
  AVStream* adts_stream = adts_stream = avformat_new_stream (adts_container_ctx, NULL);
  adts_stream->id = adts_container_ctx->nb_streams-1;

  auto t = 0.f;
  auto tincr = 2.f * M_PI * 440.0f / context->sample_rate;
  for (auto i = 0; i < 200; i++) {
    av_frame_make_writable (frame);
    //auto samples0 = (int16_t*)frame->data[0];
    //auto samples1 = (int16_t*)frame->data[1];
    auto samples0 = (float*)frame->data[0];
    auto samples1 = (float*)frame->data[1];
    for (auto j = 0; j < context->frame_size; j++) {
      //samples0[j] = int16_t(sin(t) * 10000);
      //samples1[j] = int16_t(sin(t) * 10000);
      samples0[j] = float(sin(t));
      samples1[j] = float(sin(t));
      t += tincr;
      }
    encode (context, frame, pkt, f, adts_container_ctx);
    }
  encode (context, NULL, pkt, f, adts_container_ctx);

  fclose (f);

  av_frame_free (&frame);
  av_packet_free (&pkt);
  avcodec_free_context (&context);

  return 0;
  }
