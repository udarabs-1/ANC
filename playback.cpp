#include <iostream>
#include <portaudio.h>

// Audio Stream Parameters
#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 1024
#define NUM_CHANNELS 2
#define PA_SAMPLE_TYPE paInt16

typedef short SAMPLE;

// Function to capture and playback audio in real-time
static int realTimeAudioCallback(const void *inputBuffer, void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo *timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData)
{
    // Simply copy input buffer (captured audio) to output buffer (playback)
    const SAMPLE *input = (const SAMPLE *)inputBuffer;
    SAMPLE *output = (SAMPLE *)outputBuffer;

    if (input == NULL)
    {
        // If no input, send silence (0)
        for (unsigned int i = 0; i < framesPerBuffer * NUM_CHANNELS; i++)
        {
            *output++ = 0;
        }
    }
    else
    {
        // Copy audio input to output (real-time playback)
        for (unsigned int i = 0; i < framesPerBuffer * NUM_CHANNELS; i++)
        {
            //*output++ = (*input + -(*input)) / 2;
            *output++ = -(*input);
            input++;
            std::cout << *output << std::endl;
        }
    }

    return paContinue;
}

int main()
{
    PaError err;
    PaStream *stream;

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError)
    {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return -1;
    }

    // Open stream: both input (microphone) and output (speaker)
    err = Pa_OpenDefaultStream(&stream,
                               NUM_CHANNELS,  // Input channels (stereo)
                               NUM_CHANNELS,  // Output channels (stereo)
                               PA_SAMPLE_TYPE, // Sample format
                               SAMPLE_RATE,   // Sample rate
                               FRAMES_PER_BUFFER, // Frames per buffer
                               realTimeAudioCallback, // Callback function
                               NULL);  // No user data needed

    if (err != paNoError)
    {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return -1;
    }

    // Start the audio stream
    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return -1;
    }

    std::cout << "Starting real-time audio playback..." << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    // Keep running until interrupted
    while (Pa_IsStreamActive(stream))
    {
        Pa_Sleep(100); // Sleep for a short duration (100ms) between iterations
    }

    // Stop the stream
    err = Pa_StopStream(stream);
    if (err != paNoError)
    {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    }

    // Close the stream
    err = Pa_CloseStream(stream);
    if (err != paNoError)
    {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    }

    // Terminate PortAudio
    Pa_Terminate();

    std::cout << "Real-time audio playback stopped." << std::endl;

    return 0;
}
