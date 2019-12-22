#include <vector>
#include <string>
#include <stdexcept>
#include <ciso646>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
};

#include "audiorw.hpp"

using namespace audiorw;

void audiorw::write(
    const std::vector<std::vector<double>> & audio,
    const std::string & filename,
    double sample_rate) {

  // Get a buffer for writing errors to
  size_t errbuf_size = 200;
  char errbuf[200];

  AVCodecContext * codec_context = NULL;
  AVFormatContext * format_context = NULL;
  SwrContext * resample_context = NULL;
  AVFrame * frame = NULL;
  AVPacket packet;

  // Open the output file to write to it
  AVIOContext * output_io_context;
  int error = avio_open(
      &output_io_context, 
      filename.c_str(),
      AVIO_FLAG_WRITE);
  if (error < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::invalid_argument(
        "Could not open file:" + filename + "\n" + 
        "Error: " + std::string(errbuf));
  }

  // Create a format context for the output container format
  if (!(format_context = avformat_alloc_context())) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate output format context for file:" + filename);
  }

  // Associate the output context with the output file
  format_context -> pb = output_io_context;

  // Guess the desired output file type
  if (!(format_context->oformat = av_guess_format(NULL, filename.c_str(), NULL))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not find output file format for file: " + filename);
  }

  // Add the file pathname to the output context
  if (!(format_context -> url = av_strdup(filename.c_str()))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not process file path name for file: " + filename);
  }

  // Guess the encoder for the file
  AVCodecID codec_id = av_guess_codec(
      format_context -> oformat,
      NULL,
      filename.c_str(),
      NULL,
      AVMEDIA_TYPE_AUDIO);

  // Find an encoder based on the codec
  AVCodec * output_codec;
  if (!(output_codec = avcodec_find_encoder(codec_id))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not open codec with ID, " + std::to_string(codec_id) + ", for file: " + filename);
  }

  // Create a new audio stream in the output file container
  AVStream * stream;
  if (!(stream = avformat_new_stream(format_context, NULL))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not create new stream for output file: " + filename);
  }

  // Allocate an encoding context
  if (!(codec_context = avcodec_alloc_context3(output_codec))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate an encoding context for output file: " + filename);
  }

  // Set the parameters of the stream
  codec_context -> channels = audio.size();
  codec_context -> channel_layout = av_get_default_channel_layout(audio.size());
  codec_context -> sample_rate = sample_rate;
  codec_context -> sample_fmt = output_codec -> sample_fmts[0];
  codec_context -> bit_rate = OUTPUT_BIT_RATE;

  // Set the sample rate of the container
  stream -> time_base.den = sample_rate;
  stream -> time_base.num = 1;

  // Add a global header if necessary
  if (format_context -> oformat -> flags & AVFMT_GLOBALHEADER)
    codec_context -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  // Open the encoder for the audio stream to use
  if ((error = avcodec_open2(codec_context, output_codec, NULL)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::runtime_error(
        "Could not open output codec for file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Make sure everything has been initialized correctly
  error = avcodec_parameters_from_context(stream->codecpar, codec_context);
  if (error < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not initialize stream parameters for file: " + filename);
  }

  // Initialize a resampler
  resample_context = swr_alloc_set_opts(
      NULL,
      // Output
      codec_context -> channel_layout,
      codec_context -> sample_fmt,
      sample_rate,
      // Input
      codec_context -> channel_layout,
      AV_SAMPLE_FMT_DBL,
      sample_rate,
      0, NULL);
  if (!resample_context) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate resample context for file: " + filename);
  }

  // Open the context with the specified parameters
  if ((error = swr_init(resample_context)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not open resample context for file: " + filename);
  }

  // Write the header to the output file
  if ((error = avformat_write_header(format_context, NULL)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not write output file header for file: " + filename);
  }

  // Initialize the output frame
  if (!(frame = av_frame_alloc())) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate output frame for file: " + filename);
  }
  // Allocate the frame size and format
  if (codec_context -> frame_size <= 0) {
    codec_context -> frame_size = DEFAULT_FRAME_SIZE;
  }
  frame -> nb_samples     = codec_context -> frame_size;
  frame -> channel_layout = codec_context -> channel_layout;
  frame -> format         = codec_context -> sample_fmt;
  frame -> sample_rate    = codec_context -> sample_rate;
  // Allocate the samples in the frame
  if ((error = av_frame_get_buffer(frame, 0)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::runtime_error(
        "Could not allocate output frame samples for file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Construct a packet for the encoded frame
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  // Write the samples to the audio
  int sample = 0;
  double audio_data[audio.size() * codec_context -> frame_size];
  while (true) {
    if (sample < (int) audio[0].size()) {
      // Determine how much data to send
      int frame_size = std::min(codec_context -> frame_size, int(audio[0].size() - sample));
      frame -> nb_samples = frame_size;
      
      // Timestamp the frame
      frame -> pts = sample;

      // Choose a frame size of the audio
      for (int s = 0; s < frame_size; s++) {
        for (int channel = 0; channel < (int) audio.size(); channel++) {
          audio_data[audio.size() * s + channel] = audio[channel][sample+s];
        }
      }

      // Increment
      sample += frame_size;

      // Fill the frame with audio data
      const uint8_t * audio_data_ = reinterpret_cast<uint8_t *>(audio_data);
      if ((error = swr_convert(resample_context,
                               frame -> extended_data, frame_size,
                               &audio_data_          , frame_size)) < 0) {
        cleanup(codec_context, format_context, resample_context, frame, packet);
        av_strerror(error, errbuf, errbuf_size);
        throw std::runtime_error(
            "Could not resample frame for file: " + filename + "\n" +
            "Error: " + std::string(errbuf));
      }
    } else {
      // Enter draining mode
      av_frame_free(&frame);
      frame = NULL;
    }

    // Send a frame to the encoder to encode
    if ((error = avcodec_send_frame(codec_context, frame)) < 0) {
      cleanup(codec_context, format_context, resample_context, frame, packet);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not send packet for encoding for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

    // Receive the encoded frame from the encoder
    while ((error = avcodec_receive_packet(codec_context, &packet)) == 0) {
      // Write the encoded frame to the file
      if ((error = av_write_frame(format_context, &packet)) < 0) {
        cleanup(codec_context, format_context, resample_context, frame, packet);
        av_strerror(error, errbuf, errbuf_size);
        throw std::runtime_error(
            "Could not write frame for file: " + filename + "\n" +
            "Error: " + std::string(errbuf));
      }
    }

    // If we drain to the end, end the loop 
    if (error == AVERROR_EOF) {
      break;
    // If there was an error with the decoder
    } else if (error != AVERROR(EAGAIN)) {
      cleanup(codec_context, format_context, resample_context, frame, packet);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not encode frame for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

  }

  // Write the trailer to the output file
  if ((error = av_write_trailer(format_context)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not write output file trailer for file: " + filename);
  }

  // Cleanup
  cleanup(codec_context, format_context, resample_context, frame, packet);
}
