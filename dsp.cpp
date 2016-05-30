#include "dsp.h"
#include <malloc.h>
#include <string.h>
extern "C" {
	#include <libcsdr.h>
}

void bandpass_fir(buffer_t *in, buffer_t *out, float low_cut, float high_cut, float transition_bw)
{
	window_t window = WINDOW_DEFAULT;
	//calculate the FFT size and the other length parameters
	int taps_length = firdes_filter_len(transition_bw); //the number of non-zero taps
	int fft_size = next_pow2(taps_length); //we will have to pad the taps with zeros until the next power of 2 for FFT
	//the number of padding zeros is the number of output samples we will be able to take away after every processing step, and it looks sane to check if it is large enough.
	if (fft_size-taps_length<200) fft_size<<=1;
	int input_size = fft_size - taps_length + 1;
	int overlap_length = taps_length - 1;
	fprintf(stderr,"bandpass_fir_fft_cc: (fft_size = %d) = (taps_length = %d) + (input_size = %d) - 1\n(overlap_length = %d) = taps_length - 1\n", fft_size, taps_length, input_size, overlap_length);

	//prepare making the filter and doing FFT on it
	complexf* taps=(complexf*)calloc(sizeof(complexf),fft_size); //initialize to zero
	complexf* taps_fft=(complexf*)malloc(sizeof(complexf)*fft_size);
	FFT_PLAN_T* plan_taps = make_fft_c2c(fft_size, taps, taps_fft, 1, 0); //forward, don't benchmark (we need this only once)

	//make FFT plans for continuously processing the input
	complexf* input = (complexf*) fft_malloc(fft_size*sizeof(complexf));
	complexf* input_fourier = (complexf*) fft_malloc(fft_size*sizeof(complexf));
	FFT_PLAN_T* plan_forward = make_fft_c2c(fft_size, input, input_fourier, 1, 1); //forward, do benchmark

	complexf* output_fourier = (complexf*) fft_malloc(fft_size*sizeof(complexf));
	complexf* output_1 = (complexf*) fft_malloc(fft_size*sizeof(complexf));
	complexf* output_2 = (complexf*) fft_malloc(fft_size*sizeof(complexf));
	//we create 2x output buffers so that one will preserve the previous overlap:
	FFT_PLAN_T* plan_inverse_1 = make_fft_c2c(fft_size, output_fourier, output_1, 0, 1); //inverse, do benchmark
	FFT_PLAN_T* plan_inverse_2 = make_fft_c2c(fft_size, output_fourier, output_2, 0, 1);
	//we initialize this buffer to 0 as it will be taken as the overlap source for the first time:
	for(int i=0;i<fft_size;i++) iof(plan_inverse_2->output,i)=qof(plan_inverse_2->output,i)=0;

	for(int i=input_size;i<fft_size;i++) iof(input,i)=qof(input,i)=0; //we pre-pad the input buffer with zeros

    //make the filter
    fprintf(stderr,"bandpass_fir_fft_cc: filter initialized, low_cut = %g, high_cut = %g\n",low_cut,high_cut);
    firdes_bandpass_c(taps, taps_length, low_cut, high_cut, window);
    fft_execute(plan_taps);

    for(int odd=0; in->ind + input_size < in->size; odd=!odd) //the processing loop
    {
        memcpy(input, in->ptr, input_size * sizeof(complexf));
        in->ptr += input_size * 2;
        FFT_PLAN_T* plan_inverse = (odd)?plan_inverse_2:plan_inverse_1;
        FFT_PLAN_T* plan_contains_last_overlap = (odd)?plan_inverse_1:plan_inverse_2; //the other
        complexf* last_overlap = (complexf*)plan_contains_last_overlap->output + input_size; //+ fft_size - overlap_length;
        apply_fir_fft_cc (plan_forward, plan_inverse, taps_fft, last_overlap, overlap_length);
        memcpy(out->ptr, plan_inverse->output, input_size * sizeof(complexf));
        out->ptr += input_size * 2;
    }
}
