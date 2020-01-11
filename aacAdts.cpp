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
//{{{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
//}}}

#define ENCODER_BITRATE 64000
#define SAMPLE_RATE 16000
#define INPUT_SAMPLE_FMT AV_SAMPLE_FMT_FLT
#define CHANNELS 2

static int encoded_pkt_counter = 1;

//{{{
static int write_adts_muxed_data (void *opaque, uint8_t *adts_data, int size) {

  FILE* encoded_audio_file = (FILE*)opaque;
  fwrite (adts_data, 1, size, encoded_audio_file); //(f)
  return size;
  }
//}}}

//{{{
static int mux_aac_packet_to_adts (AVPacket *encoded_audio_packet, AVFormatContext *adts_container_ctx) {

  int ret_val;
  if ((ret_val = av_write_frame (adts_container_ctx, encoded_audio_packet)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error calling av_write_frame() (error '%s')\n", av_err2str(ret_val));
    }
  else {
    av_log(NULL, AV_LOG_INFO, "Encoded AAC packet %d, size=%d, pts_time=%s\n", encoded_pkt_counter, encoded_audio_packet->size, av_ts2timestr(encoded_audio_packet->pts, &adts_container_ctx->streams[0]->time_base));
    }
  return ret_val;
  }
//}}}

//{{{
static int check_if_samplerate_is_supported(AVCodec *audio_codec, int samplerate) {

  const int *samplerates_list = audio_codec->supported_samplerates;
  while (*samplerates_list) {
    if (*samplerates_list == samplerate)
      return 0;
    ++samplerates_list;
    }
  return 1;
  }
//}}}

//{{{
int main (int argc, char **argv) {

  FILE *input_audio_file = NULL, *encoded_audio_file = NULL;
  AVCodec *audio_codec = NULL;
  AVCodecContext *audio_encoder_ctx = NULL;
  AVFrame *input_audio_frame = NULL, *converted_audio_frame = NULL;
  SwrContext *audio_convert_context = NULL;
  AVOutputFormat *adts_container = NULL;
  AVFormatContext *adts_container_ctx = NULL;
  uint8_t *adts_container_buffer = NULL;
  size_t adts_container_buffer_size = 4096;
  AVIOContext *adts_avio_ctx = NULL;
  AVStream *adts_stream = NULL;
  AVPacket *encoded_audio_packet = NULL;
  int ret_val = 0;
  int audio_bytes_to_encode;
  int64_t curr_pts;

  if (argc != 2) {
    printf("Usage: %s <raw audio input file (CHANNELS, INPUT_SAMPLE_FMT, SAMPLE_RATE)>\n", argv[0]);
    return 1;
    }

  input_audio_file = fopen(argv[1], "rb");
  if (!input_audio_file) {
    av_log(NULL, AV_LOG_ERROR, "Could not open input audio file\n");
    return AVERROR_EXIT;
    }

  encoded_audio_file = fopen("out.aac", "wb");
  if (!encoded_audio_file) {
    av_log(NULL, AV_LOG_ERROR, "Could not open output audio file\n");
    fclose(input_audio_file);
    return AVERROR_EXIT;
    }

  av_register_all();

  /*Allocate the encoder's context and open the encoder */
  audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
  if (!audio_codec) {
    av_log(NULL, AV_LOG_ERROR, "Could not find aac codec\n");
    ret_val = AVERROR_EXIT;
    goto end;
    }
  if ((ret_val = check_if_samplerate_is_supported(audio_codec, SAMPLE_RATE)) != 0) {
    av_log(NULL, AV_LOG_ERROR, "Audio codec doesn't support input samplerate %d\n", SAMPLE_RATE);
    goto end;
    }
  audio_encoder_ctx = avcodec_alloc_context3(audio_codec);
  if (!audio_codec) {
    av_log(NULL, AV_LOG_ERROR, "Could not allocate the encoding context\n");
    ret_val = AVERROR_EXIT;
    goto end;
    }

  audio_encoder_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  audio_encoder_ctx->bit_rate = ENCODER_BITRATE;
  audio_encoder_ctx->sample_rate = SAMPLE_RATE;
  audio_encoder_ctx->channels = CHANNELS;
  audio_encoder_ctx->channel_layout = av_get_default_channel_layout(CHANNELS);
  audio_encoder_ctx->time_base = (AVRational){1, SAMPLE_RATE};
  audio_encoder_ctx->codec_type = AVMEDIA_TYPE_AUDIO ;
  if ((ret_val = avcodec_open2(audio_encoder_ctx, audio_codec, NULL)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not open input codec (error '%s')\n", av_err2str(ret_val));
    goto end;
    }

  // Allocate an AVFrame which will be filled with the input file's data.
  if (!(input_audio_frame = av_frame_alloc())) {
    av_log(NULL, AV_LOG_ERROR, "Could not allocate input frame\n");
    ret_val = AVERROR(ENOMEM);
    goto end;
    }

  input_audio_frame->nb_samples     = audio_encoder_ctx->frame_size;
  input_audio_frame->format         = INPUT_SAMPLE_FMT;
  input_audio_frame->channels       = CHANNELS;
  input_audio_frame->sample_rate    = SAMPLE_RATE;
  input_audio_frame->channel_layout = av_get_default_channel_layout(CHANNELS);

  // Allocate the frame's data buffer
  if ((ret_val = av_frame_get_buffer(input_audio_frame, 0)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not allocate container for input frame samples (error '%s')\n", av_err2str(ret_val));
    ret_val = AVERROR(ENOMEM);
    goto end;
    }

  // Input data must be converted to float-planar format, which is the format required by the AAC encoder. We allocate a SwrContext and an AVFrame (which will contain the converted samples)
  // for this task. The AVFrame will feed the encoding function (avcodec_send_frame())
  audio_convert_context = swr_alloc_set_opts(NULL, av_get_default_channel_layout(CHANNELS), AV_SAMPLE_FMT_FLTP, SAMPLE_RATE, av_get_default_channel_layout(CHANNELS), INPUT_SAMPLE_FMT, SAMPLE_RATE, 0, NULL);
  if (!audio_convert_context) {
    av_log(NULL, AV_LOG_ERROR, "Could not allocate resample context\n");
    ret_val = AVERROR(ENOMEM);
    goto end;
    }
  if (!(converted_audio_frame = av_frame_alloc())) {
    av_log(NULL, AV_LOG_ERROR, "Could not allocate resampled frame\n");
    ret_val = AVERROR(ENOMEM);
    goto end;
    }
  converted_audio_frame->nb_samples     = audio_encoder_ctx->frame_size;
  converted_audio_frame->format         = audio_encoder_ctx->sample_fmt;
  converted_audio_frame->channels       = audio_encoder_ctx->channels;
  converted_audio_frame->channel_layout = audio_encoder_ctx->channel_layout;
  converted_audio_frame->sample_rate    = SAMPLE_RATE;
  if ((ret_val = av_frame_get_buffer(converted_audio_frame, 0)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not allocate a buffer for resampled frame samples (error '%s')\n", av_err2str(ret_val));
    goto end;
    }

  //  Create the ADTS container for the encoded frames
  adts_container = av_guess_format ("adts", NULL, NULL);
  if (!adts_container) {
    av_log(NULL, AV_LOG_ERROR, "Could not find adts output format\n");
    ret_val = AVERROR_EXIT;
    goto end;
    }

  if ((ret_val = avformat_alloc_output_context2 (&adts_container_ctx, adts_container, "", NULL)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not create output context (error '%s')\n", av_err2str(ret_val));
    goto end;
    }
  if (!(adts_container_buffer = av_malloc (adts_container_buffer_size))) {
    av_log(NULL, AV_LOG_ERROR, "Could not allocate a buffer for the I/O output context\n");
    ret_val = AVERROR(ENOMEM);
    goto end;
    }

  // Create an I/O context for the adts container with a write callback (write_adts_muxed_data()), so that muxed data will be accessed through this function and can be managed by the user.
  if (!(adts_avio_ctx = avio_alloc_context (adts_container_buffer, adts_container_buffer_size, 1, encoded_audio_file, NULL , &write_adts_muxed_data, NULL))) {
    av_log (NULL, AV_LOG_ERROR, "Could not create I/O output context\n");
    ret_val = AVERROR_EXIT;
    goto end;
    }

  // Link the container's context to the previous I/O context
  adts_container_ctx->pb = adts_avio_ctx;
  if (!(adts_stream = avformat_new_stream (adts_container_ctx, NULL))) {
    av_log (NULL, AV_LOG_ERROR, "Could not create new stream\n");
    ret_val = AVERROR(ENOMEM);
    goto end;
    }
  adts_stream->id = adts_container_ctx->nb_streams-1;

  // Copy the encoder's parameters
  avcodec_parameters_from_context (adts_stream->codecpar, audio_encoder_ctx);

  // Allocate the stream private data and write the stream header
  if (avformat_write_header (adts_container_ctx, NULL) < 0) {
    av_log(NULL, AV_LOG_ERROR, "avformat_write_header() error\n");
    ret_val = AVERROR_EXIT;
    goto end;
    }

  /**
  * Fill the input frame's data buffer with input file data (a),
  * Convert the input frame to float-planar format (b),
  * Send the converted frame to the encoder (c),
  * Get the encoded packet (d),
  * Send the encoded packet to the adts muxer (e).
  * Muxed data is caught in write_adts_muxed_data() callback and it is written to the output audio file ( (f) : see above)
  */
  encoded_audio_packet = av_packet_alloc();
  while (1) {
    audio_bytes_to_encode = fread (input_audio_frame->data[0], 1, input_audio_frame->linesize[0], input_audio_file); //(a)
    if (audio_bytes_to_encode != input_audio_frame->linesize[0]) {
      break;
      }
    else {
      if ((ret_val = swr_convert_frame (audio_convert_context, converted_audio_frame, (const AVFrame *)input_audio_frame)) != 0) { //(b)
        av_log (NULL, AV_LOG_ERROR, "Error resampling input audio frame (error '%s')\n", av_err2str(ret_val));
        goto end;
        }

      if ((ret_val = avcodec_send_frame (audio_encoder_ctx, converted_audio_frame)) == 0)  //(c)
        ret_val = avcodec_receive_packet (audio_encoder_ctx, encoded_audio_packet); //(d)
      else {
        av_log (NULL, AV_LOG_ERROR, "Error encoding frame (error '%s')\n", av_err2str(ret_val));
        goto end;
        }

      if (ret_val == 0) {
        curr_pts = converted_audio_frame->nb_samples * (encoded_pkt_counter-1);
        encoded_audio_packet->pts = encoded_audio_packet->dts = curr_pts;
        if ((ret_val = mux_aac_packet_to_adts (encoded_audio_packet, adts_container_ctx)) < 0) //(e)
          goto end;
        ++encoded_pkt_counter;
        }
      else if (ret_val != AVERROR(EAGAIN)) {
        av_log (NULL, AV_LOG_ERROR, "Error receiving encoded packet (error '%s')\n", av_err2str(ret_val));
        goto end;
        }
      }
    }

  // Flush cached packets
  if ((ret_val = avcodec_send_frame (audio_encoder_ctx, NULL)) == 0) {
    do {
      ret_val = avcodec_receive_packet( audio_encoder_ctx, encoded_audio_packet);
      if (ret_val == 0) {
        curr_pts = converted_audio_frame->nb_samples * (encoded_pkt_counter-1);
        encoded_audio_packet->pts = encoded_audio_packet->dts = curr_pts;
        if ((ret_val = mux_aac_packet_to_adts (encoded_audio_packet, adts_container_ctx)) < 0)
          goto end;
        ++encoded_pkt_counter;
        }
      } while (ret_val == 0);
    }

  av_write_trailer (adts_container_ctx);

end:
  fclose(input_audio_file);
  fclose(encoded_audio_file);

  avcodec_free_context(&audio_encoder_ctx);
  av_frame_free(&input_audio_frame);
  swr_free(&audio_convert_context);
  av_frame_free(&converted_audio_frame);
  avformat_free_context(adts_container_ctx);
  av_freep(&adts_avio_ctx);
  av_freep(&adts_container_buffer);
  av_packet_free(&encoded_audio_packet);

  return ret_val;
  }
//}}}
