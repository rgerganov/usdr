#pragma once
#include <libcsdr.h>
#include "pa_ringbuffer.h"

struct buffer_t {
    buffer_t(int buff_size) : ind(0), size(buff_size) {
        ptr = new float[buff_size]();
    }
    ~buffer_t() {
        delete[] ptr;
    }
    int ind;
    int size;
    float *ptr;
};

class BandpassFilter {
public:
    BandpassFilter(float low_cut, float high_cut, float transition_bw);
    void work(buffer_t *in, buffer_t *out);
    ~BandpassFilter();

private:
    int odd;
    buffer_t *leftover;
    int input_size;
    complexf *input;
    FFT_PLAN_T *plan_inverse_1;
    FFT_PLAN_T *plan_inverse_2;
    FFT_PLAN_T *plan_forward;
    complexf *taps_fft;
    int overlap_length;
};

bool start_dsp_tx(PaUtilRingBuffer *buff);
void stop_dsp_tx();
