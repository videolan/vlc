/*****************************************************************************
 * fft.h: Headers for iterative implementation of a FFT
 *****************************************************************************
 * $Id$
 *
 * Mainly taken from XMMS's code
 *
 * Authors: Richard Boulton <richard@tartarus.org>
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

#ifndef _FFT_H_
#define _FFT_H_

#define FFT_BUFFER_SIZE_LOG 9

#define FFT_BUFFER_SIZE (1 << FFT_BUFFER_SIZE_LOG)

/* sound sample - should be an signed 16 bit value */
typedef short int sound_sample;

struct _struct_fft_state {
     /* Temporary data stores to perform FFT in. */
     float real[FFT_BUFFER_SIZE];
     float imag[FFT_BUFFER_SIZE];

     /* */
     unsigned int bitReverse[FFT_BUFFER_SIZE];

     /* The next two tables could be made to use less space in memory, since they
      * overlap hugely, but hey. */
     float sintable[FFT_BUFFER_SIZE / 2];
     float costable[FFT_BUFFER_SIZE / 2];
};

/* FFT prototypes */
typedef struct _struct_fft_state fft_state;
fft_state *visual_fft_init (void);
void fft_perform (const sound_sample *input, float *output, fft_state *state);
void fft_close (fft_state *state);


#endif /* _FFT_H_ */
