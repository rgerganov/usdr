#include <hackrf.h>
#include <portaudio.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "dsp.h"
#include "pa_ringbuffer.h"
#include "fake_capture.h"

#define SAMPLE_RATE  (48000)
#define FRAMES_PER_BUFFER (512)

hackrf_device *device = NULL;

static int record_callback(const void *in_buffer, void *out_buffer,
                           unsigned long frames_per_buffer,
                           const PaStreamCallbackTimeInfo* time_info,
                           PaStreamCallbackFlags status_flags,
                           void *user_data)
{
    PaUtilRingBuffer *rbuf = (PaUtilRingBuffer*)user_data;
    const float *rptr = (const float*)in_buffer;

    unsigned long written = PaUtil_WriteRingBuffer(rbuf, rptr, frames_per_buffer);
    if (written != frames_per_buffer) {
        fprintf(stderr, "Audio ring buffer overflow!\n");
        return paComplete;
    }
    return paContinue;
}

void save_to_file(buffer_t *buff)
{
    FILE *fid = fopen("record.raw", "ab");
    if (fid == NULL) {
        printf("Could not open file.");
    } else {
        fwrite(buff->ptr, sizeof(float), buff->ind, fid);
        fclose(fid);
        printf("Wrote data to record.raw\n");
    }
}

bool start_capture(PaStream **stream, PaUtilRingBuffer *buff)
{
    PaStreamParameters inputParameters;
    //inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.device = 3;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    PaUtil_FlushRingBuffer(buff);

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

void stop_capture(PaStream *stream)
{
    PaError err = Pa_CloseStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "stop_capture error: %s\n", Pa_GetErrorText(err));
        return;
    }
}

int rx_callback(hackrf_transfer* transfer) {
    return 0;
}

bool init_hackrf()
{
    int result = hackrf_init();
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_init() failed: (%d)\n", result);
        return false;
    }
    result = hackrf_open_by_serial(NULL, &device);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_open() failed: (%d)\n", result);
        return false;
    }
    uint32_t sample_rate_hz = 8000000;
    result = hackrf_set_sample_rate_manual(device, sample_rate_hz, 1);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_sample_rate_set() failed: (%d)\n", result);
        return false;
    }
    uint32_t baseband_filter_bw_hz = hackrf_compute_baseband_filter_bw_round_down_lt(sample_rate_hz);
    result = hackrf_set_baseband_filter_bandwidth(device, baseband_filter_bw_hz);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_baseband_filter_bandwidth_set() failed: (%d)\n", result);
        return false;
    }
    unsigned int lna_gain=8, vga_gain=20;
    result = hackrf_set_vga_gain(device, vga_gain);
    result |= hackrf_set_lna_gain(device, lna_gain);
    result |= hackrf_start_rx(device, rx_callback, NULL);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_s?() failed: (%d)\n", result);
        return false;
    }
    uint64_t freq_hz = 100000000;
    result = hackrf_set_freq(device, freq_hz);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_set_freq() failed: (%d)\n", result);
        return false;
    }
    return true;
}

void close_hackrf()
{
    int result = hackrf_stop_rx(device);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_stop_rx() failed: (%d)\n", result);
    }
    result = hackrf_close(device);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_close() failed: (%d)\n", result);
    }
    hackrf_exit();
}

void process_audio(PaUtilRingBuffer *rbuf)
{
    static BandpassFilter bp_filter(0, 0.1, 0.01);

    buffer_t buff1(65536*256);
    buffer_t buff2(65536*256);

    int32_t count = PaUtil_GetRingBufferReadAvailable(rbuf);
    buff1.ind = PaUtil_ReadRingBuffer(rbuf, buff1.ptr, count);
    dsb(&buff1, &buff2);
    buff1.ind = 0;
    bp_filter.work(&buff2, &buff1);
    save_to_file(&buff1);
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
    //init_hackrf();

    float *rb_data = new float[32768]();
    PaUtilRingBuffer rbuf;
    if (PaUtil_InitializeRingBuffer(&rbuf, sizeof(float), 32768, rb_data) < 0) {
        fprintf(stderr, "Cannot init ring buffer\n");
        return 1;
    }

    SDL_Event e;
    bool quit = false;
    bool recording = false;
    PaStream *stream;
    uint32_t frames_count = 0;
    uint32_t start = SDL_GetTicks();
    uint32_t last_process = 0;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) {
                if (!recording) {
                    printf("start recording\n");
                    //hackrf_stop_rx(device);
                    //recording = start_capture(&stream, &rbuf);
                    recording = start_capture_fake("data/speech48k-float.raw", &rbuf);
                    last_process = SDL_GetTicks();
                    fflush(stdout);
                }
            } else if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_r) {
                if (recording) {
                    printf("stop recording\n");
                    //stop_capture(stream);
                    stop_capture_fake();
                    process_audio(&rbuf);
                    //hackrf_start_rx(device, rx_callback, NULL);
                    recording = false;
                    fflush(stdout);
                }
            }
        }
        uint32_t now = SDL_GetTicks();
        if (recording) {
            // process audio every 300ms
            if (now - last_process > 300) {
                process_audio(&rbuf);
                last_process = now;
            }
        }
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        float fps = frames_count / ((SDL_GetTicks() - start) / 1000.f);
        printf("fps = %.4f     \r", fps); fflush(stdout);
        ++frames_count;
    }
    delete[] rb_data;
    //close_hackrf();
    Pa_Terminate();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
