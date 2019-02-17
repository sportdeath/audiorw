#include <vector>
#include <string>
#include <stdexcept>
#include <ciso646>

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
};

#include "audiorw/audiorw.hpp"

using namespace audiorw;

void audiorw::cleanup(
    AVCodecContext * codec_context,
    AVFormatContext * format_context,
    SwrContext * resample_context,
    AVFrame * frame,
    AVPacket packet) {
  // Properly free any allocated space
  avcodec_close(codec_context);
  avcodec_free_context(&codec_context);
  avio_closep(&format_context -> pb);
  avformat_free_context(format_context);
  swr_free(&resample_context);
  av_frame_free(&frame);
  av_packet_unref(&packet);
}

std::vector<std::vector<double>> audiorw::read(
    const std::string & filename,
    double & sample_rate,
    double start_seconds,
    double end_seconds) {

  // Get a buffer for writing errors to
  size_t errbuf_size = 200;
  char errbuf[200];
  
  // Initialize variables
  AVCodecContext * codec_context = NULL;
  AVFormatContext * format_context = NULL;
  SwrContext * resample_context = NULL;
  AVFrame * frame = NULL;
  AVPacket packet;

  // Open the file and get format information
  int error = avformat_open_input(&format_context, filename.c_str(), NULL, 0);
  if (error != 0) {
    av_strerror(error, errbuf, errbuf_size);
    throw std::invalid_argument(
        "Could not open audio file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Get stream info
  if ((error = avformat_find_stream_info(format_context, NULL)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::runtime_error(
        "Could not get information about the stream in file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Find an audio stream and its decoder
  AVCodec * codec = NULL;
  int audio_stream_index = av_find_best_stream(
      format_context, 
      AVMEDIA_TYPE_AUDIO, 
      -1, -1, &codec, 0);
  if (audio_stream_index < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not determine the best stream to use in the file: " + filename);
  }

  // Allocate context for decoding the codec
  codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate a decoding context for file: " + filename);
  }

  // Fill the codecContext with parameters of the codec
  if ((error = avcodec_parameters_to_context(
        codec_context, 
        format_context -> streams[audio_stream_index] -> codecpar
        )) != 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not set codec context parameters for file: " + filename);
  }

  // Initialize the decoder
  if ((error = avcodec_open2(codec_context, codec, NULL)) != 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::runtime_error(
        "Could not initialize the decoder for file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Initialize a resampler
  resample_context = swr_alloc_set_opts(
      NULL,
      // Output
      codec_context -> channel_layout,
      AV_SAMPLE_FMT_DBL,
      sample_rate,
      // Input
      codec_context -> channel_layout,
      codec_context -> sample_fmt,
      sample_rate,
      0, NULL);
  if (!resample_context) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate resample context for file: " + filename);
  }

  // Open the resampler context with the specified parameters
  if ((error = swr_init(resample_context)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not open resample context for file: " + filename);
  }

  // Initialize the input frame
  if (!(frame = av_frame_alloc())) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate audio frame for file: " + filename);
  }

  // prepare a packet
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  // fetch the sample rate
  sample_rate = codec_context -> sample_rate;

  // Get start and end values in samples
  end_seconds = std::min(end_seconds, (format_context -> duration)/(double)AV_TIME_BASE);
  start_seconds = std::max(start_seconds, 0.);
  double start_sample = start_seconds * sample_rate;
  double end_sample   = end_seconds   * sample_rate;

  // Allocate the output vector
  std::vector<std::vector<double>> audio(
      codec_context -> channels, 
      std::vector<double>(end_sample - start_sample));

  // Make sure the frame size is nonzero
  if (codec_context -> frame_size <= 0) {
    codec_context -> frame_size = DEFAULT_FRAME_SIZE;
  }

  // Read the file until either nothing is left
  // or we reach desired end of sample
  int sample = 0;
  double audio_data[audio.size() * codec_context -> frame_size];
  while (sample < end_sample) {
    // Read from the frame
    if ((error = av_read_frame(format_context, &packet)) < 0) {
      cleanup(codec_context, format_context, resample_context, frame, packet);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Error reading from file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

    // Is this the correct stream?
    if (packet.stream_index != audio_stream_index) {
      // Otherwise move on
      continue;
    }

    // Send the packet to the decoder
    if ((error = avcodec_send_packet(codec_context, &packet)) < 0) {
      cleanup(codec_context, format_context, resample_context, frame, packet);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not send packet to decoder for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

    // Receive a decoded frame from the decoder
    while ((error = avcodec_receive_frame(codec_context, frame)) == 0) {
      // Send the frame to the resampler
      uint8_t ** audio_data_ = reinterpret_cast<uint8_t **>(audio_data);
      const uint8_t * frame_data = *(frame -> extended_data);
      if ((error = swr_convert(resample_context,
                               audio_data_, frame -> nb_samples,
                               &frame_data, frame -> nb_samples)) < 0) {
        cleanup(codec_context, format_context, resample_context, frame, packet);
        av_strerror(error, errbuf, errbuf_size);
        throw std::runtime_error(
            "Could not resample frame for file: " + filename + "\n" +
            "Error: " + std::string(errbuf));
      }

      // Update the frame
      for (int s = 0; s < frame -> nb_samples; s++) {
        for (int channel = 0; channel < (int) audio.size(); channel++) {
          int index = sample + s - start_sample;
          if ((0 <= index) and (index < (int) audio[0].size())) {
            audio[channel][index] = audio_data[audio.size() * s + channel];
          }
        }
      }

      // Increment the stamp
      sample += frame -> nb_samples;
    }

    // Check if the decoder had any errors
    if (error != AVERROR(EAGAIN)) {
      cleanup(codec_context, format_context, resample_context, frame, packet);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Error receiving packet from decoder for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }
  }

  // Cleanup
  cleanup(codec_context, format_context, resample_context, frame, packet);

  return audio;
}
