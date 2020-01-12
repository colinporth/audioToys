//{{{
/*
 * Copyright (c) 2017 Paolo Prete (p4olo_prete@yahoo.it)
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
 * API example for adts-aac encoding raw audio files.
 * This example reads a raw audio input file, converts it to float-planar format, performs aac encoding and puts the encoded frames into an ADTS container. The encoded stream is written to
 * a file named "out.aac"
 * The raw input audio file can be created with: ffmpeg -i some_audio_file -f f32le -acodec pcm_f32le -ac 2 -ar 16000 raw_audio_file.raw
 *
 * @example encode_raw_audio_file_to_aac.c
 */
//}}}
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/timestamp.h>
  #include <libswresample/swresample.h>
  }
//}}}

#define CHANNELS 2
#define SAMPLE_RATE 44100
#define ENCODER_BITRATE 128000

//{{{
int writeData (void* file, uint8_t* data, int size) {

  fwrite (data, 1, size, (FILE*)file);
  return size;
  }
//}}}

int main() {

  FILE* file = fopen ("out.aac", "wb");

  av_register_all();

  AVCodec* codec = avcodec_find_encoder (AV_CODEC_ID_AAC);

  AVCodecContext* encoderContext = avcodec_alloc_context3 (codec);
  encoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
  encoderContext->bit_rate = ENCODER_BITRATE;
  encoderContext->sample_rate = SAMPLE_RATE;
  encoderContext->channels = CHANNELS;
  encoderContext->channel_layout = av_get_default_channel_layout (CHANNELS);
  encoderContext->time_base.num = 1;
  encoderContext->time_base.den = SAMPLE_RATE;
  encoderContext->codec_type = AVMEDIA_TYPE_AUDIO ;
  avcodec_open2 (encoderContext, codec, NULL);

  // create ADTS container for encoded frames
  AVOutputFormat* outputFormat = av_guess_format ("adts", NULL, NULL);
  AVFormatContext* outputFormatContext = NULL;
  avformat_alloc_output_context2 (&outputFormatContext, outputFormat, "", NULL);

  // create ioContext for adts container with writeData callback
  int outBufferSize = 4096;
  uint8_t* outBuffer = (uint8_t*)av_malloc (outBufferSize);
  AVIOContext* ioContext = avio_alloc_context (outBuffer, outBufferSize, 1, file, NULL, &writeData, NULL);

  // link container's context to the previous I/O context
  outputFormatContext->pb = ioContext;
  AVStream* adts_stream = avformat_new_stream (outputFormatContext, NULL);
  adts_stream->id = outputFormatContext->nb_streams-1;

  // copy encoder's parameters
  avcodec_parameters_from_context (adts_stream->codecpar, encoderContext);

  // allocate stream private data and write the stream header
  avformat_write_header (outputFormatContext, NULL);

  // allocate an frame to be filled with input data.
  AVFrame* frame = av_frame_alloc();
  frame->format = AV_SAMPLE_FMT_FLTP;
  frame->channels = CHANNELS;
  frame->nb_samples = encoderContext->frame_size;
  frame->sample_rate = SAMPLE_RATE;
  frame->channel_layout = av_get_default_channel_layout (CHANNELS);

  // allocate the frame's data buffer
  av_frame_get_buffer (frame, 0);

  AVPacket* packet = av_packet_alloc();

  double t = 0.f;
  double inc = 2.0 * M_PI * 440.0 / encoderContext->sample_rate;
  for (int i = 0; i < 200; i++) {
    auto samples0 = (float*)frame->data[0];
    auto samples1 = (float*)frame->data[1];
    for (auto j = 0; j < encoderContext->frame_size; j++) {
      samples0[j] = float(sin(t));
      samples1[j] = float(sin(t));
      t += inc;
      }

    if (avcodec_send_frame (encoderContext, frame) == 0)
      while (avcodec_receive_packet (encoderContext, packet) == 0)
        if (av_write_frame (outputFormatContext, packet) < 0)
          exit(0);
    }

  // Flush cached packets
  if (avcodec_send_frame (encoderContext, NULL) == 0)
    while (avcodec_receive_packet (encoderContext, packet) == 0)
      if (av_write_frame (outputFormatContext, packet) < 0)
        exit(0);

  av_write_trailer (outputFormatContext);
  fclose (file);

  avcodec_free_context (&encoderContext);
  av_frame_free (&frame);
  avformat_free_context (outputFormatContext);
  av_freep (&ioContext);
  av_freep (&outBuffer);
  av_packet_free (&packet);

  return 0;
  }
