#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
extern "C" {
	#include <libcsdr.h>
}
#include "dsp.h"

static pthread_t dsp_tx_thread;
static volatile bool dsp_tx_running;

#define TX_BUF_SIZE 4*1024*1024

// reusable buffers
static buffer_t buff1(TX_BUF_SIZE);
static buffer_t buff2(TX_BUF_SIZE);


BandpassFilter::BandpassFilter(float low_cut, float high_cut, float transition_bw)
{
    window_t window = WINDOW_DEFAULT;
    //calculate the FFT size and the other length parameters
    int taps_length = firdes_filter_len(transition_bw); //the number of non-zero taps
    int fft_size = next_pow2(taps_length); //we will have to pad the taps with zeros until the next power of 2 for FFT
    //the number of padding zeros is the number of output samples we will be able to take away after every processing step, and it looks sane to check if it is large enough.
    if (fft_size-taps_length<200) fft_size<<=1;
    input_size = fft_size - taps_length + 1;
    overlap_length = taps_length - 1;
    fprintf(stderr,"bandpass_fir_fft_cc: (fft_size = %d) = (taps_length = %d) + (input_size = %d) - 1\n(overlap_length = %d) = taps_length - 1\n", fft_size, taps_length, input_size, overlap_length);

    //prepare making the filter and doing FFT on it
    complexf* taps=(complexf*)calloc(sizeof(complexf),fft_size); //initialize to zero
    taps_fft=(complexf*)malloc(sizeof(complexf)*fft_size);
    FFT_PLAN_T* plan_taps = make_fft_c2c(fft_size, taps, taps_fft, 1, 0); //forward, don't benchmark (we need this only once)

    //make FFT plans for continuously processing the input
    input = (complexf*) fft_malloc(fft_size*sizeof(complexf));
    complexf* input_fourier = (complexf*) fft_malloc(fft_size*sizeof(complexf));
    plan_forward = make_fft_c2c(fft_size, input, input_fourier, 1, 1); //forward, do benchmark

    complexf* output_fourier = (complexf*) fft_malloc(fft_size*sizeof(complexf));
    complexf* output_1 = (complexf*) fft_malloc(fft_size*sizeof(complexf));
    complexf* output_2 = (complexf*) fft_malloc(fft_size*sizeof(complexf));
    //we create 2x output buffers so that one will preserve the previous overlap:
    plan_inverse_1 = make_fft_c2c(fft_size, output_fourier, output_1, 0, 1); //inverse, do benchmark
    plan_inverse_2 = make_fft_c2c(fft_size, output_fourier, output_2, 0, 1);
    //we initialize this buffer to 0 as it will be taken as the overlap source for the first time:
    for(int i=0;i<fft_size;i++) iof(plan_inverse_2->output,i)=qof(plan_inverse_2->output,i)=0;

    for(int i=input_size;i<fft_size;i++) iof(input,i)=qof(input,i)=0; //we pre-pad the input buffer with zeros

    //make the filter
    fprintf(stderr,"bandpass_fir_fft_cc: filter initialized, low_cut = %g, high_cut = %g\n",low_cut,high_cut);
    firdes_bandpass_c(taps, taps_length, low_cut, high_cut, window);
    fft_execute(plan_taps);

    // initialize the state of the work() method
    odd = 0;
    leftover = new buffer_t(32768);
}

BandpassFilter::~BandpassFilter()
{
    delete leftover;
    // TODO deallocate FFT stuff
}

void BandpassFilter::work(buffer_t *in, buffer_t *out)
{
    int ind = 0;

    if (leftover->ind > 0) {
        memmove(in->ptr + leftover->ind, in->ptr, in->ind * sizeof(float));
        memcpy(in->ptr, leftover->ptr, leftover->ind * sizeof(float));
        in->ind += leftover->ind;
        leftover->ind = 0;
    }
    for(; ind + input_size*2 < in->ind; odd=!odd) //the processing loop
    {
        memcpy(input, in->ptr + ind, input_size * sizeof(complexf));
        ind += input_size * 2;
        FFT_PLAN_T* plan_inverse = (odd)?plan_inverse_2:plan_inverse_1;
        FFT_PLAN_T* plan_contains_last_overlap = (odd)?plan_inverse_1:plan_inverse_2; //the other
        complexf* last_overlap = (complexf*)plan_contains_last_overlap->output + input_size; //+ fft_size - overlap_length;
        apply_fir_fft_cc(plan_forward, plan_inverse, taps_fft, last_overlap, overlap_length);
        memcpy(out->ptr + out->ind, plan_inverse->output, input_size * sizeof(complexf));
        out->ind += input_size * 2;
    }
    if (ind < in->ind) {
        leftover->ind = in->ind - ind;
        memcpy(leftover->ptr, in->ptr + ind, leftover->ind * sizeof(float));
    }
}

RationalResampler::RationalResampler(int inter, int decim)
{
    interpolation = inter;
    decimation = decim;
    float transition_bw=0.05;
    window_t window = WINDOW_DEFAULT;

    taps_length = firdes_filter_len(transition_bw);
    taps = (float*)malloc(sizeof(float)*taps_length);
    rational_resampler_get_lowpass_f(taps, taps_length, interpolation, decimation, window);
    d.input_processed = 0;
    d.last_taps_delay = 0;
    d.output_size = 0;
}

RationalResampler::~RationalResampler()
{
    // TODO
}

void RationalResampler::work(buffer_t *in, buffer_t *out)
{
    // TODO check ranges
    int output_size = in->ind*interpolation / decimation;
    if (output_size > out->size) {
        fprintf(stderr, "[resampler] buffer overflow!\n");
        return;
    }
    d = rational_resampler_ff(in->ptr, out->ptr, in->ind, interpolation, decimation, taps, taps_length, d.last_taps_delay);
    out->ind = output_size;
    if (d.input_processed != in->ind) {
        fprintf(stderr, ">> [resampler] processed: %d total: %d\n", d.input_processed, in->ind);
    }
}

static void dsb(buffer_t *in, buffer_t *out)
{
    for (int i = 0 ; i < in->ind ; i++) {
        out->ptr[i * 2] = in->ptr[i];
        out->ptr[i * 2 + 1] = 0;
    }
    out->ind = in->ind * 2;
}

static void save_to_file(const char *fname, buffer_t *buff)
{
    FILE *fid = fopen(fname, "ab");
    if (fid == NULL) {
        printf("Could not open file.");
    } else {
        fwrite(buff->ptr, sizeof(float), buff->ind, fid);
        fclose(fid);
        printf("Wrote data to %s\n", fname);
    }
}

static void *dsp_tx(void *arg)
{
    PaUtilRingBuffer *ring_buff = (PaUtilRingBuffer*) arg;
    BandpassFilter bp_filter(0, 0.1, 0.01);
    RationalResampler resampler(50, 1);
    while (dsp_tx_running) {
        usleep(300 * 1000); // sleep for 300ms
        if (!dsp_tx_running) {
            break;
        }
        int32_t count = PaUtil_GetRingBufferReadAvailable(ring_buff);
        buff1.ind = PaUtil_ReadRingBuffer(ring_buff, buff1.ptr, count);
        dsb(&buff1, &buff2);
        buff1.ind = 0;
        bp_filter.work(&buff2, &buff1);
        for (int i = 0 ; i < buff1.ind ; i++) {
            buff1.ptr[i] *= 2.0;
        }
        resampler.work(&buff1, &buff2);
        save_to_file("record.cfile", &buff2);
    }
    return NULL;
}

bool start_dsp_tx(PaUtilRingBuffer *buff)
{
    if (pthread_create(&dsp_tx_thread, NULL, dsp_tx, buff)) {
        fprintf(stderr, "start_dsp_tx: cannot start thread\n");
        return false;
    }
    dsp_tx_running = true;
    return true;
}

void stop_dsp_tx()
{
    dsp_tx_running = false;
    if (pthread_join(dsp_tx_thread, NULL)) {
        fprintf(stderr, "stop_dsp_tx: error joining thread\n");
    }
}
