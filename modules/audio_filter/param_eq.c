/*****************************************************************************
 * param_eq.c:
 *****************************************************************************
 * Copyright © 2006 VLC authors and VideoLAN
 *
 * Authors: Antti Huovilainen
 *          Sigmund A. Helberg <dnumgis@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static void CalcPeakEQCoeffs( float, float, float, float, float * );
static void CalcShelfEQCoeffs( float, float, float, int, float, float * );
static void ProcessEQ( const float *, float *, float *, unsigned, unsigned,
                       const float *, unsigned );
static block_t *DoWork( filter_t *, block_t * );

vlc_module_begin ()
    set_description( N_("Parametric Equalizer") )
    set_shortname( N_("Parametric Equalizer" ) )
    set_capability( "audio filter", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )

    add_float( "param-eq-lowf", 100, N_("Low freq (Hz)"),NULL, false )
    add_float_with_range( "param-eq-lowgain", 0, -20.0, 20.0,
                          N_("Low freq gain (dB)"), NULL,false )
    add_float( "param-eq-highf", 10000, N_("High freq (Hz)"),NULL, false )
    add_float_with_range( "param-eq-highgain", 0, -20.0, 20.0,
                          N_("High freq gain (dB)"),NULL,false )
    add_float( "param-eq-f1", 300, N_("Freq 1 (Hz)"),NULL, false )
    add_float_with_range( "param-eq-gain1", 0, -20.0, 20.0,
                          N_("Freq 1 gain (dB)"), NULL,false )
    add_float_with_range( "param-eq-q1", 3, 0.1, 100.0,
                          N_("Freq 1 Q"), NULL,false )
    add_float( "param-eq-f2", 1000, N_("Freq 2 (Hz)"),NULL, false )
    add_float_with_range( "param-eq-gain2", 0, -20.0, 20.0,
                          N_("Freq 2 gain (dB)"),NULL,false )
    add_float_with_range( "param-eq-q2", 3, 0.1, 100.0,
                          N_("Freq 2 Q"),NULL,false )
    add_float( "param-eq-f3", 3000, N_("Freq 3 (Hz)"),NULL, false )
    add_float_with_range( "param-eq-gain3", 0, -20.0, 20.0,
                          N_("Freq 3 gain (dB)"),NULL,false )
    add_float_with_range( "param-eq-q3", 3, 0.1, 100.0,
                          N_("Freq 3 Q"),NULL,false )

    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    /* Filter static config */
    float   f_lowf, f_lowgain;
    float   f_f1, f_Q1, f_gain1;
    float   f_f2, f_Q2, f_gain2;
    float   f_f3, f_Q3, f_gain3;
    float   f_highf, f_highgain;
    /* Filter computed coeffs */
    float   coeffs[5*5];
    /* State */
    float  *p_state;
} filter_sys_t;




/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    unsigned     i_samplerate;

    /* Allocate structure */
    filter_sys_t *p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_EGENERIC;

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    p_sys->f_lowf = var_InheritFloat( p_this, "param-eq-lowf");
    p_sys->f_lowgain = var_InheritFloat( p_this, "param-eq-lowgain");
    p_sys->f_highf = var_InheritFloat( p_this, "param-eq-highf");
    p_sys->f_highgain = var_InheritFloat( p_this, "param-eq-highgain");
 
    p_sys->f_f1 = var_InheritFloat( p_this, "param-eq-f1");
    p_sys->f_Q1 = var_InheritFloat( p_this, "param-eq-q1");
    p_sys->f_gain1 = var_InheritFloat( p_this, "param-eq-gain1");
 
    p_sys->f_f2 = var_InheritFloat( p_this, "param-eq-f2");
    p_sys->f_Q2 = var_InheritFloat( p_this, "param-eq-q2");
    p_sys->f_gain2 = var_InheritFloat( p_this, "param-eq-gain2");

    p_sys->f_f3 = var_InheritFloat( p_this, "param-eq-f3");
    p_sys->f_Q3 = var_InheritFloat( p_this, "param-eq-q3");
    p_sys->f_gain3 = var_InheritFloat( p_this, "param-eq-gain3");
 

    i_samplerate = p_filter->fmt_in.audio.i_rate;
    CalcPeakEQCoeffs(p_sys->f_f1, p_sys->f_Q1, p_sys->f_gain1,
                     i_samplerate, p_sys->coeffs+0*5);
    CalcPeakEQCoeffs(p_sys->f_f2, p_sys->f_Q2, p_sys->f_gain2,
                     i_samplerate, p_sys->coeffs+1*5);
    CalcPeakEQCoeffs(p_sys->f_f3, p_sys->f_Q3, p_sys->f_gain3,
                     i_samplerate, p_sys->coeffs+2*5);
    CalcShelfEQCoeffs(p_sys->f_lowf, 1, p_sys->f_lowgain, 0,
                      i_samplerate, p_sys->coeffs+3*5);
    CalcShelfEQCoeffs(p_sys->f_highf, 1, p_sys->f_highgain, 0,
                      i_samplerate, p_sys->coeffs+4*5);
    p_sys->p_state = (float*)calloc( p_filter->fmt_in.audio.i_channels*5*4,
                                     sizeof(float) );

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    free( p_sys->p_state );
    free( p_sys );
}

/*****************************************************************************
 * DoWork: process samples buffer
 *****************************************************************************
 *
 *****************************************************************************/
static block_t *DoWork( filter_t * p_filter, block_t * p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    ProcessEQ( (float*)p_in_buf->p_buffer, (float*)p_in_buf->p_buffer,
               p_sys->p_state,
               p_filter->fmt_in.audio.i_channels, p_in_buf->i_nb_samples,
               p_sys->coeffs, 5 );
    return p_in_buf;
}

/*
 * Calculate direct form IIR coefficients for peaking EQ
 * coeffs[0] = b0
 * coeffs[1] = b1
 * coeffs[2] = b2
 * coeffs[3] = a1
 * coeffs[4] = a2
 *
 * Equations taken from RBJ audio EQ cookbook
 * (http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt)
 */
static void CalcPeakEQCoeffs( float f0, float Q, float gainDB, float Fs,
                              float *coeffs )
{
    float A;
    float w0;
    float alpha;
    float b0, b1, b2;
    float a0, a1, a2;

    // Provide sane limits to avoid overflow
    if (Q < 0.1f) Q = 0.1f;
    if (Q > 100) Q = 100;
    if (f0 > Fs/2*0.95f) f0 = Fs/2*0.95f;
    if (gainDB < -40) gainDB = -40;
    if (gainDB > 40) gainDB = 40;
 
    A = powf(10, gainDB/40);
    w0 = 2*((float)M_PI)*f0/Fs;
    alpha = sinf(w0)/(2*Q);
 
    b0 = 1 + alpha*A;
    b1 = -2*cosf(w0);
    b2 = 1 - alpha*A;
    a0 = 1 + alpha/A;
    a1 = -2*cosf(w0);
    a2 = 1 - alpha/A;
 
    // Store values to coeffs and normalize by 1/a0
    coeffs[0] = b0/a0;
    coeffs[1] = b1/a0;
    coeffs[2] = b2/a0;
    coeffs[3] = a1/a0;
    coeffs[4] = a2/a0;
}

/*
 * Calculate direct form IIR coefficients for low/high shelf EQ
 * coeffs[0] = b0
 * coeffs[1] = b1
 * coeffs[2] = b2
 * coeffs[3] = a1
 * coeffs[4] = a2
 *
 * Equations taken from RBJ audio EQ cookbook
 * (http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt)
 */
static void CalcShelfEQCoeffs( float f0, float slope, float gainDB, int high,
                               float Fs, float *coeffs )
{
    float A;
    float w0;
    float alpha;
    float b0, b1, b2;
    float a0, a1, a2;

    // Provide sane limits to avoid overflow
    if (f0 > Fs/2*0.95f) f0 = Fs/2*0.95f;
    if (gainDB < -40) gainDB = -40;
    if (gainDB > 40) gainDB = 40;

    A = powf(10, gainDB/40);
    w0 = 2*3.141593f*f0/Fs;
    alpha = sinf(w0)/2 * sqrtf( (A + 1/A)*(1/slope - 1) + 2 );

    if (high)
    {
        b0 =    A*( (A+1) + (A-1)*cosf(w0) + 2*sqrtf(A)*alpha );
        b1 = -2*A*( (A-1) + (A+1)*cosf(w0) );
        b2 =    A*( (A+1) + (A-1)*cosf(w0) - 2*sqrtf(A)*alpha );
        a0 =        (A+1) - (A-1)*cosf(w0) + 2*sqrtf(A)*alpha;
        a1 =    2*( (A-1) - (A+1)*cosf(w0) );
        a2 =        (A+1) - (A-1)*cosf(w0) - 2*sqrtf(A)*alpha;
    }
    else
    {
        b0 =    A*( (A+1) - (A-1)*cosf(w0) + 2*sqrtf(A)*alpha );
        b1 =  2*A*( (A-1) - (A+1)*cosf(w0));
        b2 =    A*( (A+1) - (A-1)*cosf(w0) - 2*sqrtf(A)*alpha );
        a0 =        (A+1) + (A-1)*cosf(w0) + 2*sqrtf(A)*alpha;
        a1 =   -2*( (A-1) + (A+1)*cosf(w0));
        a2 =        (A+1) + (A-1)*cosf(w0) - 2*sqrtf(A)*alpha;
    }
    // Store values to coeffs and normalize by 1/a0
    coeffs[0] = b0/a0;
    coeffs[1] = b1/a0;
    coeffs[2] = b2/a0;
    coeffs[3] = a1/a0;
    coeffs[4] = a2/a0;
}

/*
  src is assumed to be interleaved
  dest is assumed to be interleaved
  size of state is 4*channels*eqCount
  samples is not premultiplied by channels
  size of coeffs is 5*eqCount
*/
void ProcessEQ( const float *src, float *dest, float *state,
                unsigned channels, unsigned samples, const float *coeffs,
                unsigned eqCount )
{
    unsigned i, chn, eq;
    float   b0, b1, b2, a1, a2;
    float   x, y = 0;
    const float *src1 = src;
    float *dest1 = dest;

    for (i = 0; i < samples; i++)
    {
        float *state1 = state;
        for (chn = 0; chn < channels; chn++)
        {
            const float *coeffs1 = coeffs;
            x = *src1++;
            /* Direct form 1 IIRs */
            for (eq = 0; eq < eqCount; eq++)
            {
                b0 = coeffs1[0];
                b1 = coeffs1[1];
                b2 = coeffs1[2];
                a1 = coeffs1[3];
                a2 = coeffs1[4];
                coeffs1 += 5;
                y = x*b0 + state1[0]*b1 + state1[1]*b2 - state1[2]*a1 - state1[3]*a2;
                state1[1] = state1[0];
                state1[0] = x;
                state1[3] = state1[2];
                state1[2] = y;
                x = y;
                state1 += 4;
            }
            *dest1++ = y;
        }
    }
}

