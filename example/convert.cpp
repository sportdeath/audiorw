#include <iostream>
#include <audiorw.hpp>

int main(int argc, char ** argv) {

  if (argc != 3) {
    std::cout <<
      "Usage: " << argv[0] << " input_file output_file"
      << std::endl;
    return 1;
  }

  // Read the file
  std::cout << "Reading from file " << argv[1] << std::endl;
  double sample_rate;
  std::vector<std::vector<double>> audio =
    audiorw::read(argv[1], sample_rate);

  // Write the file
  std::cout << "Writing to file " << argv[2] << std::endl;
  audiorw::write(audio, argv[2], sample_rate);
  
  return 0;
}
