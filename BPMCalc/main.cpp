// not sure if audio stream is there !??!?!?
// compile with: 
// g++ -o audioBPM main.cpp lib/kiss_fft130/kiss_fft.c -lportaudio -lBTrack -lsamplerate -L./lib -I./include -I./lib/kiss_fft130

#include <iostream>
#include <portaudio.h>
#include "BTrack.h"
#include "OnsetDetectionFunction.h"
#include "CircularBuffer.h"
#include "lib/kiss_fft130/kiss_fft.h"
#include <iomanip>  // for std::setprecision

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512

BTrack btrack(256, 2048);  // Initialize BTrack with a hop size and frame size

void listDevices() {
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "ERROR: Pa_CountDevices returned " << numDevices << std::endl;
        return;
    }

    const PaDeviceInfo *deviceInfo;
    for (int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo) {
            std::cout << "Device " << i << ": " << deviceInfo->name;
            if (deviceInfo->maxInputChannels > 0)
                std::cout << " (Input Channels: " << deviceInfo->maxInputChannels << ")";
            std::cout << " // Default Sample Rate: " << deviceInfo->defaultSampleRate << std::endl;
        }
    }
}

static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData) {
    const float* input = static_cast<const float*>(inputBuffer);
    double frame[FRAMES_PER_BUFFER];
    double newSample = 0.0; // Initialize to 0.0
    double maxAmplitude = 0.0;  // Track maximum amplitude

    if (input == nullptr) {
        std::cerr << "Error: Input buffer is NULL." << std::endl;
        return paContinue;
    }

    for (int i = 0; i < framesPerBuffer; i++) {
        frame[i] = static_cast<double>(input[i]);
        if (std::abs(input[i]) > maxAmplitude) {
            maxAmplitude = std::abs(input[i]);
        }
    }

    btrack.processAudioFrame(frame);

    btrack.processOnsetDetectionFunctionSample(newSample);

    if (btrack.beatDueInCurrentFrame() && maxAmplitude > 0.05) {
        // Log the beat detected and the max amplitude seen in this frame
        double bpm = btrack.getCurrentTempoEstimate();
        std::cout << "Beat detected! BPM: " << std::fixed << std::setprecision(0) << bpm << " Max Ampl.: " << std::fixed << std::setprecision(2) << maxAmplitude << std::endl;
        // std::cout << "Beat detected! BPM: " << static_cast<int>(std::round(btrack.getCurrentTempoEstimate())) << " Max Ampl.: " << maxAmplitude << std::endl;
    }

    return paContinue;
}

int main() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    listDevices();  // List all available devices

    int deviceIndex = 0; // Set this to the index of your microphone
    std::cout << "Enter the index of the device you want to use for audio input: ";
    std::cin >> deviceIndex;

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo) {
        std::cerr << "Error: Could not retrieve information for device." << std::endl;
        Pa_Terminate();
        return 1;
    }
    std::cout << "Using device: " << deviceInfo->name << std::endl;

    PaStreamParameters inputParameters;
    inputParameters.device = deviceIndex;
    inputParameters.channelCount = 1; // Adjust this based on your microphone's capabilities
    inputParameters.sampleFormat = paFloat32; // This should match the format BTrack expects
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream;
    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        nullptr,  // No output
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,  // We won't output any audio, so clipping is not an issue
        audioCallback,
        nullptr
    );

    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return 1;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear the input buffer
    std::cin.get();

    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    }

    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    }

    Pa_Terminate();
    return 0;
}
