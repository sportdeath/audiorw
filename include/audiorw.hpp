#pragma once

#include <vector>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
};

namespace audiorw {

static const int OUTPUT_BIT_RATE = 320000;
static const int DEFAULT_FRAME_SIZE = 2048;

std::vector<std::vector<double>> read(
    const std::string & filename,
    double & sample_rate,
    double start_seconds=0,
    double end_seconds=-1);

void write(
    const std::vector<std::vector<double>> & audio,
    const std::string & filename,
    double sample_rate);

void cleanup(
    AVCodecContext * codec_context,
    AVFormatContext * format_context,
    SwrContext * resample_context,
    AVFrame * frame,
    AVPacket packet);

}
