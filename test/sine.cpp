#include <cmath>

#include <audiorw/audio.hpp>

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
  std::vector<double> s = sine(440, 2, sample_rate);
  std::vector<std::vector<double>> s_ = {s};

  audiorw::write(s_, "a.mp3", sample_rate);

  return 0;
}
