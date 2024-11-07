#include <iostream>
#include <portaudio.h>
#include <algorithm>
#include <cmath>

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 1024
#define NUM_CHANNELS 2 // Stereo input: Noise Source (ch1) and Feedback (ch2)
#define PA_SAMPLE_TYPE paInt16
#define NOISE_THRESHOLD 500 // Threshold to filter out ambient noise

typedef short SAMPLE;

// PID controller parameters
float Kp = 0.25;  // Proportional gain
float Ki = 0.0; // Integral gain
float Kd = 0.05; // Derivative gain

float previousError = 0.0;
float integral = 0.0;

// PID control function for adaptive noise cancellation
float pidControl(float noiseSource, float feedbackMic)
{
    std::cout << "Noise " << noiseSource << " Feedback " << feedbackMic << std::endl;
    
    // Compute the error between the noise source and feedback microphone
    float error = feedbackMic - noiseSource;

    // Proportional term
    float proportional = Kp * error;

    // Integral term (accumulates past errors)
    integral += error * Ki;

    // Derivative term (based on the rate of error change)
    float derivative = Kd * (error - previousError);

    // Compute the control output (adjustment for the cancellation signal)
    float pidOutput = proportional + integral + derivative;

    // Update previous error for the next cycle
    previousError = error;

    // Return the PID-adjusted error signal
    return pidOutput;
}

// Callback function for real-time ANC using PID
static int realTimeANC_PID(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
    const SAMPLE *input = (const SAMPLE *)inputBuffer;
    SAMPLE *output = (SAMPLE *)outputBuffer;

    for (unsigned int i = 0; i < framesPerBuffer; i++)
    {
        // Channel 1 (even index) is the noise source mic input
        SAMPLE noiseSource = input[i * 2];

        // Channel 2 (odd index) is the feedback mic input
        SAMPLE feedbackMic = input[i * 2 + 1];

        // Apply the PID control algorithm if the noise level is above the threshold
        if (abs(noiseSource) > NOISE_THRESHOLD)
        {
            // Calculate the cancellation signal using PID control
            float pidError = pidControl((float)noiseSource, (float)feedbackMic);

            // Clamp the PID output to avoid overflow
            float clampedPidError = std::min(std::max(pidError, -32768.0f), 32767.0f);
            //float clampedPidError = pidError*1000;
            // Output the inverse of the PID-adjusted error signal to the speaker
            output[i * 2] = -(SAMPLE)(clampedPidError);    // Left channel (speaker)
            output[i * 2 + 1] = -(SAMPLE)(clampedPidError); // Right channel (speaker)
        }
        else
        {
            // If the noise is below the threshold, output silence
            output[i * 2] = 0;
            output[i * 2 + 1] = 0;
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

    // Open stream: input (two-channel mic) and output (speaker)
    err = Pa_OpenDefaultStream(&stream,
                               NUM_CHANNELS,  // Input channels (stereo)
                               NUM_CHANNELS,  // Output channels (stereo)
                               PA_SAMPLE_TYPE, // Sample format
                               SAMPLE_RATE,   // Sample rate
                               FRAMES_PER_BUFFER, // Frames per buffer
                               realTimeANC_PID, // Callback function for ANC
                               nullptr);  // No user data needed

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

    std::cout << "Starting real-time ANC with PID control..." << std::endl;
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

    std::cout << "ANC with PID control stopped." << std::endl;

    return 0;
}
