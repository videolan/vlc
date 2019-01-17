/*****************************************************************************
 * fft.c: Iterative implementation of a FFT
 *****************************************************************************
 *
 * Mainly taken from XMMS's code
 *
 * Authors: Richard Boulton <richard@tartarus.org>
 *          Ralph Loader <suckfish@ihug.co.nz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdlib.h>
#include "fft.h"

#include <math.h>
#ifndef PI
 #ifdef M_PI
  #define PI M_PI
 #else
  #define PI            3.14159265358979323846  /* pi */
 #endif
#endif

/******************************************************************************
 * Local prototypes
 *****************************************************************************/
static void fft_prepare(const sound_sample *input, float * re, float * im,
                        const unsigned int *bitReverse);
static void fft_calculate(float * re, float * im,
                          const float *costable, const float *sintable );
static void fft_output(const float *re, const float *im, float *output);
static int reverseBits(unsigned int initial);

/*****************************************************************************
 * These functions are the ones called externally
 *****************************************************************************/

/*
 * Initialisation routine - sets up tables and space to work in.
 * Returns a pointer to internal state, to be used when performing calls.
 * On error, returns NULL.
 * The pointer should be freed when it is finished with, by fft_close().
 */
fft_state *visual_fft_init(void)
{
    fft_state *p_state;
    unsigned int i;

    p_state = malloc( sizeof(*p_state) );
    if(! p_state )
        return NULL;

    for(i = 0; i < FFT_BUFFER_SIZE; i++)
    {
        p_state->bitReverse[i] = reverseBits(i);
    }
    for(i = 0; i < FFT_BUFFER_SIZE / 2; i++)
    {
        float j = 2 * PI * i / FFT_BUFFER_SIZE;
        p_state->costable[i] = cos(j);
        p_state->sintable[i] = sin(j);
    }

    return p_state;
}

/*
 * Do all the steps of the FFT, taking as input sound data (as described in
 * sound.h) and returning the intensities of each frequency as floats in the
 * range 0 to ((FFT_BUFFER_SIZE / 2) * 32768) ^ 2
 *
 * The input array is assumed to have FFT_BUFFER_SIZE elements,
 * and the output array is assumed to have (FFT_BUFFER_SIZE / 2 + 1) elements.
 * state is a (non-NULL) pointer returned by visual_fft_init.
 */
void fft_perform(const sound_sample *input, float *output, fft_state *state) {
    /* Convert data from sound format to be ready for FFT */
    fft_prepare(input, state->real, state->imag, state->bitReverse );

    /* Do the actual FFT */
    fft_calculate(state->real, state->imag, state->costable, state->sintable);

    /* Convert the FFT output into intensities */
    fft_output(state->real, state->imag, output);
}

/*
 * Free the state.
 */
void fft_close(fft_state *state) {
    free( state );
}

/*****************************************************************************
 * These functions are called from the other ones
 *****************************************************************************/

/*
 * Prepare data to perform an FFT on
 */
static void fft_prepare( const sound_sample *input, float * re, float * im,
                         const unsigned int *bitReverse ) {
    unsigned int i;
    float *p_real = re;
    float *p_imag = im;

    /* Get input, in reverse bit order */
    for(i = 0; i < FFT_BUFFER_SIZE; i++)
    {
        *p_real++ = input[bitReverse[i]];
        *p_imag++ = 0;
    }
}

/*
 * Take result of an FFT and calculate the intensities of each frequency
 * Note: only produces half as many data points as the input had.
 */
static void fft_output(const float * re, const float * im, float *output)
{
    float *p_output = output;
    const float *p_real   = re;
    const float *p_imag   = im;
    float *p_end    = output + FFT_BUFFER_SIZE / 2;

    while(p_output <= p_end)
    {
        *p_output = (*p_real * *p_real) + (*p_imag * *p_imag);
        p_output++; p_real++; p_imag++;
    }
    /* Do divisions to keep the constant and highest frequency terms in scale
     * with the other terms. */
    *output /= 4;
    *p_end /= 4;
}


/*
 * Actually perform the FFT
 */
static void fft_calculate(float * re, float * im, const float *costable, const float *sintable )
{
    unsigned int i, j, k;
    unsigned int exchanges;
    float fact_real, fact_imag;
    float tmp_real, tmp_imag;
    unsigned int factfact;

    /* Set up some variables to reduce calculation in the loops */
    exchanges = 1;
    factfact = FFT_BUFFER_SIZE / 2;

    /* Loop through the divide and conquer steps */
    for(i = FFT_BUFFER_SIZE_LOG; i != 0; i--) {
        /* In this step, we have 2 ^ (i - 1) exchange groups, each with
         * 2 ^ (FFT_BUFFER_SIZE_LOG - i) exchanges
         */
        /* Loop through the exchanges in a group */
        for(j = 0; j != exchanges; j++) {
            /* Work out factor for this exchange
             * factor ^ (exchanges) = -1
             * So, real = cos(j * PI / exchanges),
             *     imag = sin(j * PI / exchanges)
             */
            fact_real = costable[j * factfact];
            fact_imag = sintable[j * factfact];

            /* Loop through all the exchange groups */
            for(k = j; k < FFT_BUFFER_SIZE; k += exchanges << 1) {
                int k1 = k + exchanges;
                tmp_real = fact_real * re[k1] - fact_imag * im[k1];
                tmp_imag = fact_real * im[k1] + fact_imag * re[k1];
                re[k1] = re[k] - tmp_real;
                im[k1] = im[k] - tmp_imag;
                re[k]  += tmp_real;
                im[k]  += tmp_imag;
            }
        }
        exchanges <<= 1;
        factfact >>= 1;
    }
}

static int reverseBits(unsigned int initial)
{
    unsigned int reversed = 0, loop;
    for(loop = 0; loop < FFT_BUFFER_SIZE_LOG; loop++) {
        reversed <<= 1;
        reversed += (initial & 1);
        initial >>= 1;
    }
    return reversed;
}
