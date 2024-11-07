#include <iostream>
#include <portaudio.h>
#include <vector>

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 1024
#define NUM_CHANNELS 2 // Stereo input: Noise Source (ch1) and Feedback (ch2)
#define PA_SAMPLE_TYPE paInt16
#define LMS_STEP_SIZE 0.001 // Step size for LMS adaptation
#define NOISE_THRESHOLD 200
#define GAIN 2

typedef short SAMPLE;

typedef struct
{
    std::vector<SAMPLE> weights; // Adaptive filter weights
    std::vector<SAMPLE> buffer;  // Buffer to store past samples for filtering
    unsigned int filterLength;   // Length of the adaptive filter
} LMSData;

// Initialize the LMS filter
void initializeLMS(LMSData *lmsData, unsigned int filterLength)
{
    lmsData->filterLength = filterLength;
    lmsData->weights.resize(filterLength, 0); // Start with zero weights
    lmsData->buffer.resize(filterLength, 0);  // Input signal buffer
}

// LMS adaptive filtering function
SAMPLE applyLMS(LMSData *lmsData, SAMPLE input, SAMPLE desired)
{
    // Shift buffer and insert the new input sample
    for (unsigned int i = lmsData->filterLength - 1; i > 0; i--)
    {
        lmsData->buffer[i] = lmsData->buffer[i - 1];
    }
    lmsData->buffer[0] = input;

    // Compute filter output
    SAMPLE output = 0;
    for (unsigned int i = 0; i < lmsData->filterLength; i++)
    {
        output += lmsData->weights[i] * lmsData->buffer[i];
    }

    // Compute error (desired - actual output)
    SAMPLE error = desired - output;

    // Update the weights using the LMS rule
    for (unsigned int i = 0; i < lmsData->filterLength; i++)
    {
        lmsData->weights[i] += LMS_STEP_SIZE * error * lmsData->buffer[i];
    }

    return output;
}

// Callback function for real-time ANC using LMS
static int realTimeANCLMS(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    LMSData *lmsData = (LMSData *)userData;
    const SAMPLE *input = (const SAMPLE *)inputBuffer;
    SAMPLE *output = (SAMPLE *)outputBuffer;

    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        // Channel 1 (even index) is the noise source mic input
        SAMPLE noiseSource = input[i * 2 + 1];

        // Channel 2 (odd index) is the feedback mic input
        SAMPLE feedbackMic = input[i * 2];
        
        std::cout << "Noise " << noiseSource << std::endl;
        std::cout << "Feedback " << feedbackMic << std::endl;
        
        if (abs(noiseSource) > NOISE_THRESHOLD) {

            // Apply LMS adaptive filtering to the noise source signal
            SAMPLE lmsOutput = GAIN * (feedbackMic - noiseSource);
            
            //std::cout << "LMS Output " << lmsOutput << std::endl;

            // Invert the LMS output and send it to the speaker
            output[i * 2] = -lmsOutput;    // Output to speaker (left channel)
            output[i * 2 + 1] = -lmsOutput; // Output to speaker (right channel)
        }
        
        else {
            output[i * 2] = 0;    // Output to speaker (left channel)
            output[i * 2 + 1] = 0; // Output to speaker (right channel)
        }
    }

    return paContinue;
}

int main()
{
    PaError err;
    PaStream *stream;
    LMSData lmsData;

    // Initialize LMS filter with a given filter length
    unsigned int filterLength = 128; // You can adjust this value as needed
    initializeLMS(&lmsData, filterLength);

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError)
    {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return -1;
    }

    // Open stream: input (two-channel mic) and output (speaker)
    err = Pa_OpenDefaultStream(&stream,
                               NUM_CHANNELS,  // Input channels (stereo)
                               1,  // Output channels (stereo)
                               PA_SAMPLE_TYPE, // Sample format
                               SAMPLE_RATE,   // Sample rate
                               FRAMES_PER_BUFFER, // Frames per buffer
                               realTimeANCLMS, // Callback function
                               &lmsData);  // LMS filter data

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

    std::cout << "Starting ANC with LMS..." << std::endl;
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

    std::cout << "ANC with LMS stopped." << std::endl;

    return 0;
}
