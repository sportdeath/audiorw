#include <vector>
#include <string>
#include <stdexcept>
#include <ciso646>

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
};

#include "audiorw/audio.hpp"

#define OUTPUT_BIT_RATE 320000
#define DEFAULT_FRAME_SIZE 2048

void cleanup_write(
    AVCodecContext * av_context,
    AVFormatContext * output_format_context) {

    avcodec_free_context(&av_context);
    avio_closep(&output_format_context -> pb);
    avformat_free_context(output_format_context);
}

void audiorw::write(
    const std::vector<std::vector<double>> & audio,
    const std::string & filename,
    double sample_rate) {

  // Get a buffer for writing errors to
  size_t errbuf_size = 200;
  char errbuf[200];

  // Open the output file to write to it
  AVIOContext * output_io_context;
  int error = avio_open(
      &output_io_context, 
      filename.c_str(),
      AVIO_FLAG_WRITE);
  if (error < 0) {
    av_strerror(error, errbuf, errbuf_size);
    throw std::invalid_argument(
        "Could not open file:" + filename + "\n" + 
        "Error: " + std::string(errbuf));
  }

  // Create a format context for the output container format
  AVFormatContext * output_format_context = NULL;
  if (!(output_format_context = avformat_alloc_context())) {
    throw std::runtime_error(
        "Could not allocate output format context for file:" + filename);
  }

  // Associate the output context with the output file
  output_format_context -> pb = output_io_context;

  // Guess the desired output file type
  AVCodecContext * av_context = NULL;
  if (!(output_format_context->oformat = av_guess_format(NULL, filename.c_str(), NULL))) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not find output file format for file: " + filename);
  }

  // Add the file pathname to the output context
  if (!(output_format_context -> url = av_strdup(filename.c_str()))) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not process file path name for file: " + filename);
  }

  // Guess the encoder for the file
  AVCodecID codec_id = av_guess_codec(
      output_format_context -> oformat,
      NULL,
      filename.c_str(),
      NULL,
      AVMEDIA_TYPE_AUDIO);

  // Find an encoder based on the codec
  AVCodec * output_codec;
  if (!(output_codec = avcodec_find_encoder(codec_id))) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not open codec with ID, " + std::to_string(codec_id) + ", for file: " + filename);
  }

  // Create a new audio stream in the output file container
  AVStream * stream;
  if (!(stream = avformat_new_stream(output_format_context, NULL))) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not create new stream for output file: " + filename);
  }

  // Allocate an encoding context
  if (!(av_context = avcodec_alloc_context3(output_codec))) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not allocate an encoding context for output file: " + filename);
  }

  // Set the parameters of the stream
  av_context -> channels = audio.size();
  av_context -> channel_layout = av_get_default_channel_layout(audio.size());
  av_context -> sample_rate = sample_rate;
  av_context -> sample_fmt = output_codec -> sample_fmts[0];
  av_context -> bit_rate = OUTPUT_BIT_RATE;

  // Set the sample rate of the container
  stream -> time_base.den = sample_rate;
  stream -> time_base.num = 1;

  // Add a global header if necessary
  if (output_format_context -> oformat -> flags & AVFMT_GLOBALHEADER)
    av_context -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  // Open the encoder for the audio stream to use
  if ((error = avcodec_open2(av_context, output_codec, NULL)) < 0) {
    cleanup_write(av_context, output_format_context);
    av_strerror(error, errbuf, errbuf_size);
    throw std::runtime_error(
        "Could not open output codec for file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Make sure everything has been initialized correctly
  error = avcodec_parameters_from_context(stream->codecpar, av_context);
  if (error < 0) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not initialize stream parameters for file: " + filename);
  }

  // Initialize a resampler
  SwrContext * resample_context;
  resample_context = swr_alloc_set_opts(
      NULL,
      // Output
      av_context -> channel_layout,
      av_context -> sample_fmt,
      sample_rate,
      // Input
      av_context -> channel_layout,
      AV_SAMPLE_FMT_DBL,
      sample_rate,
      0, NULL);
  if (!resample_context) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not allocate resample context for file: " + filename);
  }

  // Open the context with the specified parameters
  if ((error = swr_init(resample_context)) < 0) {
    swr_free(&resample_context);
    throw std::runtime_error(
        "Could not open resample context for file: " + filename);
  }

  // Write the header to the output file
  if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not write output file header for file: " + filename);
  }

  // Construct a frame of audio
  AVFrame * output_frame;

  // Initialize the output frame
  if (!(output_frame = av_frame_alloc())) {
      cleanup_write(av_context, output_format_context);
      throw std::runtime_error(
          "Could not allocate output frame for file: " + filename);
  }
  // Allocate the frame size and format
  if (av_context -> frame_size == 0) {
    av_context -> frame_size = DEFAULT_FRAME_SIZE;
  }
  output_frame -> nb_samples     = av_context -> frame_size;
  output_frame -> channel_layout = av_context -> channel_layout;
  output_frame -> format         = av_context -> sample_fmt;
  output_frame -> sample_rate    = av_context -> sample_rate;
  // Allocate the samples in the frame
  if ((error = av_frame_get_buffer(output_frame, 0)) < 0) {
      av_frame_free(&output_frame);
      cleanup_write(av_context, output_format_context);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not allocate output frame samples for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
  }

  // Construct a packet for the encoded frame
  AVPacket output_packet;
  av_init_packet(&output_packet);
  output_packet.data = NULL;
  output_packet.size = 0;


  // Write the samples to the audio
  size_t sample = 0;
  double audio_data[audio.size() * av_context -> frame_size];
  while (sample < audio[0].size()) {
    // Determine how much data to send
    int frame_size = std::min(av_context -> frame_size, int(audio[0].size() - sample));
    output_frame -> nb_samples = frame_size;
    
    // Timestamp the frame
    output_frame -> pts = sample;

    // Choose a frame size of the audio
    for (size_t channel = 0; channel < audio.size(); channel++) {
      for (int s = 0; s < frame_size; s++) {
        audio_data[audio.size() * s + channel] = audio[channel][sample+s];
      }
    }

    // Increment
    sample += frame_size;

    // Fill the frame with audio data
    const uint8_t * audio_data_ = reinterpret_cast<uint8_t *>(audio_data);
    if ((error = swr_convert(resample_context,
                             output_frame -> extended_data, frame_size,
                             &audio_data_                 , frame_size)) < 0) {
      av_frame_free(&output_frame);
      av_packet_unref(&output_packet);
      cleanup_write(av_context, output_format_context);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not allocate output frame samples for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

    // Send a frame to the encoder to encode
    if ((error = avcodec_send_frame(av_context, output_frame)) < 0) {
      av_frame_free(&output_frame);
      av_packet_unref(&output_packet);
      cleanup_write(av_context, output_format_context);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not send packet for encoding for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

    // Receive the encoded frame from the encoder
    error = avcodec_receive_packet(av_context, &output_packet);
    if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
      continue;
    } else if (error < 0) {
      av_frame_free(&output_frame);
      av_packet_unref(&output_packet);
      cleanup_write(av_context, output_format_context);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not encode frame for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

    // Write the encoded frame to the file
    if ((error = av_write_frame(output_format_context, &output_packet)) < 0) {
      av_frame_free(&output_frame);
      av_packet_unref(&output_packet);
      cleanup_write(av_context, output_format_context);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not write frame for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }
  }

  // Free the output frame and packet
  av_frame_free(&output_frame);
  av_packet_unref(&output_packet);

  // Write the trailer to the output file
  if ((error = av_write_trailer(output_format_context)) < 0) {
    cleanup_write(av_context, output_format_context);
    throw std::runtime_error(
        "Could not write output file trailer for file: " + filename);
  }

  // Cleanup
  cleanup_write(av_context, output_format_context);
}
