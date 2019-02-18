#include <iostream>
#include <cmath>

#include <audiorw.hpp>

std::vector<double> sine(double frequency, double duration, double sample_rate) {
  size_t length = duration * sample_rate;

  std::vector<double> s(length);

  for (size_t n = 0; n < length; n++) {
    double t = n/sample_rate;
    s[n] = std::sin(2 * M_PI * frequency * t);
  }

  return s;
}

void a(std::string extension) {
  double sample_rate = 44100;
  std::vector<double> s_left  = sine(440, 2.346738, sample_rate);
  std::vector<double> s_right  = sine(880, 2.346738, sample_rate);

  std::vector<std::vector<double>> s = {s_left, s_right};

  audiorw::write(s, "a." + extension, sample_rate);
}

int main() {
  a("wav");
  a("aif");
  a("au");
  a("flac");
  a("ogg");
  a("mp3");

  return 0;
}
