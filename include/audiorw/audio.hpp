#pragma once

#include <vector>
#include <string>

namespace audiorw {

std::vector<std::vector<double>> read(
    const std::string & filename,
    double & sample_rate,
    double start_seconds=0,
    double end_seconds=0);

void write(
    const std::vector<std::vector<double>> & audio,
    const std::string & filename,
    double sample_rate);

}
