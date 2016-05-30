#include <portaudio.h>
#include <stdio.h>
#include "dsp.h"

#define NUM_SECONDS     (5)
#define SAMPLE_RATE  (48000)
#define FRAMES_PER_BUFFER (512)
#define SAMPLE_SILENCE  (0.0f)

static int record_callback(const void *in_buffer, void *out_buffer,
                           unsigned long frames_per_buffer,
                           const PaStreamCallbackTimeInfo* time_info,
                           PaStreamCallbackFlags status_flags,
                           void *user_data)
{
    buffer_t *buff = (buffer_t*)user_data;
    const float *rptr = (const float*)in_buffer;
    float *wptr = &buff->ptr[buff->ind];
    int frames = frames_per_buffer;
    int result = paContinue;
    unsigned long frames_left = buff->size - buff->ind;

    if(frames_left < frames_per_buffer) {
        frames = frames_left;
        result = paComplete;
    }

    if(in_buffer == NULL) {
        for(int i = 0; i < frames; i++) {
            *wptr++ = SAMPLE_SILENCE;
        }
    } else {
        for(int i = 0; i < frames; i++) {
            *wptr++ = *rptr++;
        }
    }
    buff->ind += frames;
    return result;
}

int main(int argc, char *argv[])
{
    FILE *fid = NULL;

    int totalFrames = NUM_SECONDS * SAMPLE_RATE;
    buffer_t buff(totalFrames);

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        goto done;
    }

    PaStreamParameters inputParameters;
    // inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.device = 3;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    PaStream* stream;
    err = Pa_OpenStream(&stream,
                        &inputParameters,
                        NULL,
                        SAMPLE_RATE,
                        FRAMES_PER_BUFFER,
                        paClipOff,
                        record_callback,
                        &buff);
    if (err != paNoError) goto done;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto done;

    printf("\n=== Now recording!! Please speak into the microphone. ===\n");
    fflush(stdout);

    while ((err = Pa_IsStreamActive(stream)) == 1) {
        Pa_Sleep(1000);
        printf("index = %d\n", buff.ind);
        fflush(stdout);
    }
    if (err < 0) goto done;

    err = Pa_CloseStream(stream);
    if (err != paNoError) goto done;

    fid = fopen("recorded.raw", "wb");
    if (fid == NULL) {
        printf("Could not open file.");
    } else {
        fwrite(buff.ptr, sizeof(float), totalFrames, fid);
        fclose(fid);
        printf("Wrote data to 'recorded.raw'\n");
    }

    // bandpass_fir(NULL, NULL, 0.0, 0.1, 0.001);

done:
    Pa_Terminate();
    if (err != paNoError) {
        fprintf(stderr, "An error occurred while using the portaudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
        err = 1;
    }
    return err;
}
