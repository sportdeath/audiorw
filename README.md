# audiorw

A minimal C++ library to read and write audio files powered by FFMPEG.

```audiorw``` supports reading from and writing to the following file types:

- ```.wav```
- ```.aif```
- ```.au```
- ```.flac```
- ```.ogg```

Additionally the following files can be read from but not written to:

- ```.mp3```
- ```.mp4```
- ```.m4a```

The interface is intended to be as simple as possible and automatically selects the right
encoder or decoder based on an audio file's extension and contents.

## Installation

To install the library, clone it, build it with ```cmake```, then install it:

    git clone git@github.com:sportdeath/audiorw.git
    mkdir audiorw/build
    cd audiorw/build
    cmake ..
    make
    sudo make install

## API

The library comes with two functions, ```read``` and ```write``` which are intended to be as simple as possible.
Both functions will automatically choose the right decoder or encoder based on the file extension and file contexts.
To include the library, simply put this at the header of your document:

    #include <audiorw/audiorw.hpp>

### Read

The ```read``` function takes the audio filename and returns both the audio vector and the sample rate.
The audio is a 2D vector of doubles where the first dimension selects the channel and the second dimension selects the sample.
Each element of the vector will be in the range [-1,1].

    std::vector<std::vector<double>> audiorw::read(
        const std::string & filename,
        double & sample_rate)

For example if we wanted to read some samples from a stereo audio file called ```example.wav``` we could do:

    #include <vector>
    #include <audiorw/audiorw.hpp>

    double sample_rate;
    std::vector<std::vector<double>> audio = audiorw::read("example.wav", sample_rate);

    // Read the 10th sample from the left channel
    double left_sample = audio[0][10];

    // Read the 10th sample from the right channel
    double right_sample = audio[1][10];

### Write

The ```write``` function takes an audio vector, an audio filename and a sample rate and writes the audio to the specified file.
Again, the audio is a 2D vector of doubles where the first dimension selects the channel and the second dimension selects the sample.
The output will be clipped to the range [-1,1] when written.

    void write(
        const std::vector<std::vector<double>> & audio,
        const std::string & filename,
        double sample_rate);

In the example below we construct a stereo audio vector that is 1 second long with a sample rate of 44100.
In the left channel a tone plays at 440hz (A4) and in the right channel a tone plays at 880hz (A5).
The result is written to a file called "example.flac".

    #include <vector>
    #include <cmath>
    #include <audiorw/audiorw.hpp>

    double sample_rate = 44100; // samples per second
    double duration = 1; // second

    // Initialize a stereo audio file to the proper length
    std::vector<std::vector<double>> audio(2, std::vector<double>(duration * sample_rate));

    for (int i = 0; i < duration * sample_rate; i++) {
      // Write 440 hz in the left channel
      audio[0][i] = std::sin(2 * M_PI * 440 * i/sample_rate);

      // Write 880 hz in the right channel
      audio[1][i] = std::sin(2 * M_PI * 880 * i/sample_rate);
    }

    audiorw::write(audio, "example.flac", sample_rate);

## Examples

Two simple examples are included in the ```example``` folder. They can be build with

    cd audiorw/build
    cmake -DBUILD_EXAMPLES=ON ..
    make

The ```convert``` example converts audio from one format to another:

    ./convert input.ext1 output.ext2

The ```generate_a``` example generates stereo audio files similar to those described in the Write section in a variety of audio formats.

    ./generate_a
