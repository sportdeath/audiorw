#include <iostream>
#include <cmath>

#include <audiorw/audiorw.hpp>

std::vector<double> sine(double frequency, double duration, double sample_rate) {
  size_t length = duration * sample_rate;

  std::vector<double> s(length);

  for (size_t n = 0; n < length; n++) {
    double t = n/sample_rate;
    s[n] = std::sin(2 * M_PI * frequency * t);
  }

  return s;
}

int main() {
  double sample_rate = 44100;
  std::vector<double> s_left  = sine(440, 2, sample_rate);
  std::vector<double> s_right = sine(660, 2, sample_rate);

  std::vector<std::vector<double>> s = {s_left, s_right};

  audiorw::write(s, "a.mp3", sample_rate);

  return 0;
}
