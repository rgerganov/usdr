#pragma once

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

void bandpass_fir(buffer_t *in, buffer_t *out, float low_cut, float high_cut, float transition_bw);
void dsb(buffer_t *in, buffer_t *out);
