#include <portaudio.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "dsp.h"
#include "pa_ringbuffer.h"

#define NUM_SECONDS     (5)
#define SAMPLE_RATE  (48000)
#define FRAMES_PER_BUFFER (512)
#define SAMPLE_SILENCE  (0.0f)

float *audio_data;
PaUtilRingBuffer rb_audio;

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

void save_to_file(buffer_t *buff)
{
    char fname[256];
    sprintf(fname, "record-%d.raw", SDL_GetTicks());
    FILE *fid = fopen(fname, "wb");
    if (fid == NULL) {
        printf("Could not open file.");
    } else {
        fwrite(buff->ptr, sizeof(float), buff->ind, fid);
        fclose(fid);
        printf("Wrote data to %s\n", fname);
    }
}

bool start_capture(PaStream **stream, buffer_t *buff)
{
    PaStreamParameters inputParameters;
    // inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.device = 3;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    buff->ind = 0;

    PaError err = Pa_OpenStream(stream,
                                &inputParameters,
                                NULL,
                                SAMPLE_RATE,
                                FRAMES_PER_BUFFER,
                                paClipOff,
                                record_callback,
                                buff);
    if (err == paNoError) {
        err = Pa_StartStream(*stream);
    }
    if (err != paNoError) {
        fprintf(stderr, "start_capture error: %s\n", Pa_GetErrorText(err));
        return false;
    }
    return true;
}

void stop_capture(PaStream *stream, buffer_t *buff)
{
    PaError err = Pa_CloseStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "stop_capture error: %s\n", Pa_GetErrorText(err));
        return;
    }
    save_to_file(buff);
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("uSDR", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        fprintf(stderr, "CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PaInit error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    SDL_Event e;
    bool quit = false;
    bool recording = false;
    PaStream *stream;
    buffer_t buff(NUM_SECONDS * SAMPLE_RATE);
    int frames_count = 0;
    int start = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) {
                if (!recording) {
                    printf("start recording\n");
                    recording = start_capture(&stream, &buff);
                    fflush(stdout);
                }
            } else if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_r) {
                if (recording) {
                    printf("stop recording\n");
                    stop_capture(stream, &buff);
                    recording = false;
                    fflush(stdout);
                }
            }
        }
        float fps = frames_count / ((SDL_GetTicks() - start) / 1000.f);
        printf("fps = %.4f     \r", fps); fflush(stdout);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        ++frames_count;
    }

    Pa_Terminate();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
