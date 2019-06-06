/*****************************************************************************
 * effects.c : Effects for the visualization system
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
 *          Adrien Maglo <magsoft@videolan.org>
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

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_block.h>

#include "visual.h"
#include <math.h>

#include "fft.h"
#include "window.h"

#define PEAK_SPEED 1
#define BAR_DECREASE_SPEED 5

#define GRAD_ANGLE_MIN 0.2
#define GRAD_ANGLE_MAX 0.5
#define GRAD_INCR 0.01

/*****************************************************************************
 * dummy_Run
 *****************************************************************************/
static int dummy_Run( visual_effect_t * p_effect, vlc_object_t *p_aout,
                      const block_t * p_buffer , picture_t * p_picture)
{
    VLC_UNUSED(p_effect); VLC_UNUSED(p_aout); VLC_UNUSED(p_buffer);
    VLC_UNUSED(p_picture);
    return 0;
}

static void dummy_Free( void *data )
{
    VLC_UNUSED(data);
}


/*****************************************************************************
 * spectrum_Run: spectrum analyser
 *****************************************************************************/
typedef struct spectrum_data
{
    int *peaks;
    int *prev_heights;

    unsigned i_prev_nb_samples;
    int16_t *p_prev_s16_buff;

    window_param wind_param;
} spectrum_data;

static int spectrum_Run(visual_effect_t * p_effect, vlc_object_t *p_aout,
                        const block_t * p_buffer , picture_t * p_picture)
{
    spectrum_data *p_data = p_effect->p_data;
    float p_output[FFT_BUFFER_SIZE];  /* Raw FFT Result  */
    int *height;                      /* Bar heights */
    int *peaks;                       /* Peaks */
    int *prev_heights;                /* Previous bar heights */
    int i_80_bands;                   /* number of bands : 80 if true else 20 */
    int i_nb_bands;                   /* number of bands : 80 or 20 */
    int i_band_width;                 /* width of bands */
    int i_start;                      /* first band horizontal position */
    int i_peak;                       /* Should we draw peaks ? */

    /* Horizontal scale for 20-band equalizer */
    const int xscale1[]={0,1,2,3,4,5,6,7,8,11,15,20,27,
                        36,47,62,82,107,141,184,255};

    /* Horizontal scale for 80-band equalizer */
    const int xscale2[] =
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,
     19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,
     35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
     52,53,54,55,56,57,58,59,61,63,67,72,77,82,87,93,99,105,
     110,115,121,130,141,152,163,174,185,200,255};
    const int *xscale;

    fft_state *p_state;                 /* internal FFT data */
    DEFINE_WIND_CONTEXT( wind_ctx );    /* internal window data */

    int i , j , y , k;
    int i_line;
    int16_t p_dest[FFT_BUFFER_SIZE];      /* Adapted FFT result */
    int16_t p_buffer1[FFT_BUFFER_SIZE];   /* Buffer on which we perform
                                             the FFT (first channel) */

    float *p_buffl =                     /* Original buffer */
            (float*)p_buffer->p_buffer;

    int16_t  *p_buffs;                    /* int16_t converted buffer */
    int16_t  *p_s16_buff;                 /* int16_t converted buffer */

    if (!p_buffer->i_nb_samples) {
        msg_Err(p_aout, "no samples yet");
        return -1;
    }

    /* Create p_data if needed */
    if( !p_data )
    {
        p_effect->p_data = p_data = malloc( sizeof( spectrum_data ) );
        if( !p_data )
            return -1;

        p_data->peaks = calloc( 80, sizeof(int) );
        p_data->prev_heights = calloc( 80, sizeof(int) );

        p_data->i_prev_nb_samples = 0;
        p_data->p_prev_s16_buff = NULL;

        window_get_param( p_aout, &p_data->wind_param );
    }
    peaks = (int *)p_data->peaks;
    prev_heights = (int *)p_data->prev_heights;

    /* Allocate the buffer only if the number of samples change */
    if( p_buffer->i_nb_samples != p_data->i_prev_nb_samples )
    {
        free( p_data->p_prev_s16_buff );
        p_data->p_prev_s16_buff = vlc_alloc( p_buffer->i_nb_samples *
                                             p_effect->i_nb_chans,
                                             sizeof(int16_t));
        p_data->i_prev_nb_samples = p_buffer->i_nb_samples;
        if( !p_data->p_prev_s16_buff )
            return -1;
    }
    p_buffs = p_s16_buff = p_data->p_prev_s16_buff;

    i_80_bands = var_InheritInteger( p_aout, "visual-80-bands" );
    i_peak     = var_InheritInteger( p_aout, "visual-peaks" );

    if( i_80_bands != 0)
    {
        xscale = xscale2;
        i_nb_bands = 80;
    }
    else
    {
        xscale = xscale1;
        i_nb_bands = 20;
    }

    height = vlc_alloc( i_nb_bands, sizeof(int) );
    if( !height )
    {
        return -1;
    }
    /* Convert the buffer to int16_t  */
    /* Pasted from float32tos16.c */
    for (i = p_buffer->i_nb_samples * p_effect->i_nb_chans; i--; )
    {
        union { float f; int32_t i; } u;
        u.f = *p_buffl + 384.0;
        if(u.i >  0x43c07fff ) * p_buffs = 32767;
        else if ( u.i < 0x43bf8000 ) *p_buffs = -32768;
        else *p_buffs = u.i - 0x43c00000;

        p_buffl++ ; p_buffs++ ;
    }
    p_state  = visual_fft_init();
    if( !p_state)
    {
        free( height );
        msg_Err(p_aout,"unable to initialize FFT transform");
        return -1;
    }
    if( !window_init( FFT_BUFFER_SIZE, &p_data->wind_param, &wind_ctx ) )
    {
        fft_close( p_state );
        free( height );
        msg_Err(p_aout,"unable to initialize FFT window");
        return -1;
    }
    p_buffs = p_s16_buff;
    for ( i = 0 ; i < FFT_BUFFER_SIZE ; i++)
    {
        p_output[i]  = 0;
        p_buffer1[i] = *p_buffs;

        p_buffs += p_effect->i_nb_chans;
        if( p_buffs >= &p_s16_buff[p_buffer->i_nb_samples * p_effect->i_nb_chans] )
            p_buffs = p_s16_buff;

    }
    window_scale_in_place( p_buffer1, &wind_ctx );
    fft_perform( p_buffer1, p_output, p_state);
    for( i = 0; i< FFT_BUFFER_SIZE ; i++ )
        p_dest[i] = p_output[i] *  ( 2 ^ 16 ) / ( ( FFT_BUFFER_SIZE / 2 * 32768 ) ^ 2 );

    /* Compute the horizontal position of the first band */
    i_band_width = floor( p_effect->i_width / i_nb_bands);
    i_start = ( p_effect->i_width - i_band_width * i_nb_bands ) / 2;

    for ( i = 0 ; i < i_nb_bands ;i++)
    {
        /* We search the maximum on one scale */
        for( j = xscale[i], y = 0; j< xscale[ i + 1 ]; j++ )
        {
            if ( p_dest[j] > y )
                 y = p_dest[j];
        }
        /* Calculate the height of the bar */
        if( y != 0 )
        {
            height[i] = log( y ) * 30;
            if( height[i] > 380 )
                height[i] = 380;
        }
        else
            height[ i ] = 0;

        /* Draw the bar now */

        if( height[i] > peaks[i] )
        {
            peaks[i] = height[i];
        }
        else if( peaks[i] > 0 )
        {
            peaks[i] -= PEAK_SPEED;
            if( peaks[i] < height[i] )
            {
                peaks[i] = height[i];
            }
            if( peaks[i] < 0 )
            {
                peaks[i] = 0;
            }
        }

        /* Decrease the bars if needed */
        if( height[i] <= prev_heights[i] - BAR_DECREASE_SPEED )
        {
            height[i] = prev_heights[i];
            height[i] -= BAR_DECREASE_SPEED;
        }
        prev_heights[i] = height[i];

        if( peaks[i] > 0 && i_peak )
        {
            if( peaks[i] >= p_effect->i_height )
                peaks[i] = p_effect->i_height - 2;
            i_line = peaks[i];

            for( j = 0; j < i_band_width - 1; j++ )
            {
               for( k = 0; k < 3; k ++ )
               {
                   /* Draw the peak */
                   *(p_picture->p[0].p_pixels +
                    ( p_effect->i_height - i_line -1 -k ) *
                     p_picture->p[0].i_pitch +
                     ( i_start + i_band_width*i + j ) )
                                    = 0xff;

                   *(p_picture->p[1].p_pixels +
                    ( ( p_effect->i_height - i_line ) / 2 - 1 -k/2 ) *
                     p_picture->p[1].i_pitch +
                     ( ( i_start + i_band_width * i + j ) /2  ) )
                                    = 0x00;

                   if( i_line + k - 0x0f > 0 )
                   {
                       if ( i_line + k - 0x0f < 0xff )
                           *(p_picture->p[2].p_pixels  +
                            ( ( p_effect->i_height - i_line ) / 2 - 1 -k/2 ) *
                             p_picture->p[2].i_pitch +
                             ( ( i_start + i_band_width * i + j ) /2  ) )
                                    = ( i_line + k ) - 0x0f;
                       else
                           *(p_picture->p[2].p_pixels  +
                            ( ( p_effect->i_height - i_line ) / 2 - 1 -k/2 ) *
                             p_picture->p[2].i_pitch +
                             ( ( i_start + i_band_width * i + j ) /2  ) )
                                    = 0xff;
                   }
                   else
                   {
                        *(p_picture->p[2].p_pixels  +
                         ( ( p_effect->i_height - i_line ) / 2 - 1 -k/2 ) *
                         p_picture->p[2].i_pitch +
                         ( ( i_start + i_band_width * i + j ) /2  ) )
                               = 0x10 ;
                   }
               }
            }
        }

        if(height[i] > p_effect->i_height)
            height[i] = floor(p_effect->i_height );

        for( i_line = 0; i_line < height[i]; i_line++ )
        {
            for( j = 0 ; j < i_band_width - 1; j++)
            {
               *(p_picture->p[0].p_pixels +
                 (p_effect->i_height - i_line - 1) *
                  p_picture->p[0].i_pitch +
                  ( i_start + i_band_width*i + j ) ) = 0xff;

               *(p_picture->p[1].p_pixels +
                 ( ( p_effect->i_height - i_line ) / 2 - 1) *
                 p_picture->p[1].i_pitch +
                 ( ( i_start + i_band_width * i + j ) /2  ) ) = 0x00;

               if( i_line - 0x0f > 0 )
               {
                    if( i_line - 0x0f < 0xff )
                         *(p_picture->p[2].p_pixels  +
                           ( ( p_effect->i_height - i_line ) / 2 - 1) *
                           p_picture->p[2].i_pitch +
                           ( ( i_start + i_band_width * i + j ) /2  ) ) =
                               i_line - 0x0f;
                    else
                         *(p_picture->p[2].p_pixels  +
                           ( ( p_effect->i_height - i_line ) / 2  - 1) *
                           p_picture->p[2].i_pitch +
                           ( ( i_start + i_band_width * i + j ) /2  ) ) =
                                       0xff;
               }
               else
               {
                    *(p_picture->p[2].p_pixels  +
                      ( ( p_effect->i_height - i_line ) / 2  - 1) *
                      p_picture->p[2].i_pitch +
                      ( ( i_start + i_band_width * i + j ) /2  ) ) =
                            0x10;
               }
            }
        }
    }

    window_close( &wind_ctx );

    fft_close( p_state );

    free( height );

    return 0;
}

static void spectrum_Free( void *data )
{
    spectrum_data *p_data = data;

    if( p_data != NULL )
    {
        free( p_data->peaks );
        free( p_data->prev_heights );
        free( p_data->p_prev_s16_buff );
        free( p_data );
    }
}


/*****************************************************************************
 * spectrometer_Run: derivative spectrum analysis
 *****************************************************************************/
typedef struct
{
    int *peaks;

    unsigned i_prev_nb_samples;
    int16_t *p_prev_s16_buff;

    window_param wind_param;
} spectrometer_data;

static int spectrometer_Run(visual_effect_t * p_effect, vlc_object_t *p_aout,
                            const block_t * p_buffer , picture_t * p_picture)
{
#define Y(R,G,B) ((uint8_t)( (R * .299) + (G * .587) + (B * .114) ))
#define U(R,G,B) ((uint8_t)( (R * -.169) + (G * -.332) + (B * .500) + 128 ))
#define V(R,G,B) ((uint8_t)( (R * .500) + (G * -.419) + (B * -.0813) + 128 ))
    float p_output[FFT_BUFFER_SIZE];  /* Raw FFT Result  */
    int *height;                      /* Bar heights */
    int *peaks;                       /* Peaks */
    int i_80_bands;                   /* number of bands : 80 if true else 20 */
    int i_nb_bands;                   /* number of bands : 80 or 20 */
    int i_band_width;                 /* width of bands */
    int i_separ;                      /* Should we let blanks ? */
    int i_amp;                        /* Vertical amplification */
    int i_peak;                       /* Should we draw peaks ? */

    int i_original;          /* original spectrum graphic routine */
    int i_rad;               /* radius of circle of base of bands */
    int i_sections;          /* sections of spectranalysis */
    int i_extra_width;       /* extra width on peak */
    int i_peak_height;       /* height of peak */
    int c;                   /* sentinel container of total spectral sections */
    double band_sep_angle;   /* angled separation between beginning of each band */
    double section_sep_angle;/* "   "    '     "    '    "     "    spectrum section */
    int max_band_length;     /* try not to go out of screen */
    int i_show_base;         /* Should we draw base of circle ? */
    int i_show_bands;        /* Should we draw bands ? */
    //int i_invert_bands;      /* do the bands point inward ? */
    double a;                /* for various misc angle situations in radians */
    int x,y,xx,yy;           /* various misc x/y */
    char color1;             /* V slide on a YUV color cube */
    //char color2;             /* U slide.. ?  color2 fade color ? */

    /* Horizontal scale for 20-band equalizer */
    const int xscale1[]={0,1,2,3,4,5,6,7,8,11,15,20,27,
                        36,47,62,82,107,141,184,255};

    /* Horizontal scale for 80-band equalizer */
    const int xscale2[] =
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,
     19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,
     35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
     52,53,54,55,56,57,58,59,61,63,67,72,77,82,87,93,99,105,
     110,115,121,130,141,152,163,174,185,200,255};
    const int *xscale;
    const double y_scale =  3.60673760222;  /* (log 256) */

    fft_state *p_state;                 /* internal FFT data */
    DEFINE_WIND_CONTEXT( wind_ctx );    /* internal window data */

    int i , j , k;
    int i_line = 0;
    int16_t p_dest[FFT_BUFFER_SIZE];      /* Adapted FFT result */
    int16_t p_buffer1[FFT_BUFFER_SIZE];   /* Buffer on which we perform
                                             the FFT (first channel) */
    float *p_buffl =                     /* Original buffer */
            (float*)p_buffer->p_buffer;

    int16_t  *p_buffs;                    /* int16_t converted buffer */
    int16_t  *p_s16_buff;                /* int16_t converted buffer */

    if (!p_buffer->i_nb_samples) {
        msg_Err(p_aout, "no samples yet");
        return -1;
    }

    /* Create the data struct if needed */
    spectrometer_data *p_data = p_effect->p_data;
    if( !p_data )
    {
        p_data = malloc( sizeof(spectrometer_data) );
        if( !p_data )
            return -1;
        p_data->peaks = calloc( 80, sizeof(int) );
        if( !p_data->peaks )
        {
            free( p_data );
            return -1;
        }
        p_data->i_prev_nb_samples = 0;
        p_data->p_prev_s16_buff = NULL;
        window_get_param( p_aout, &p_data->wind_param );
        p_effect->p_data = (void*)p_data;
    }
    peaks = p_data->peaks;

    /* Allocate the buffer only if the number of samples change */
    if( p_buffer->i_nb_samples != p_data->i_prev_nb_samples )
    {
        free( p_data->p_prev_s16_buff );
        p_data->p_prev_s16_buff = vlc_alloc( p_buffer->i_nb_samples *
                                             p_effect->i_nb_chans,
                                             sizeof(int16_t));
        p_data->i_prev_nb_samples = p_buffer->i_nb_samples;
        if( !p_data->p_prev_s16_buff )
            return -1;
    }
    p_buffs = p_s16_buff = p_data->p_prev_s16_buff;

    i_original     = var_InheritInteger( p_aout, "spect-show-original" );
    i_80_bands     = var_InheritInteger( p_aout, "spect-80-bands" );
    i_separ        = var_InheritInteger( p_aout, "spect-separ" );
    i_amp          = var_InheritInteger( p_aout, "spect-amp" );
    i_peak         = var_InheritInteger( p_aout, "spect-show-peaks" );
    i_show_base    = var_InheritInteger( p_aout, "spect-show-base" );
    i_show_bands   = var_InheritInteger( p_aout, "spect-show-bands" );
    i_rad          = var_InheritInteger( p_aout, "spect-radius" );
    i_sections     = var_InheritInteger( p_aout, "spect-sections" );
    i_extra_width  = var_InheritInteger( p_aout, "spect-peak-width" );
    i_peak_height  = var_InheritInteger( p_aout, "spect-peak-height" );
    color1         = var_InheritInteger( p_aout, "spect-color" );

    if( i_80_bands != 0)
    {
        xscale = xscale2;
        i_nb_bands = 80;
    }
    else
    {
        xscale = xscale1;
        i_nb_bands = 20;
    }

    height = vlc_alloc( i_nb_bands, sizeof(int) );
    if( !height)
        return -1;

    /* Convert the buffer to int16_t  */
    /* Pasted from float32tos16.c */
    for (i = p_buffer->i_nb_samples * p_effect->i_nb_chans; i--; )
    {
        union { float f; int32_t i; } u;
        u.f = *p_buffl + 384.0;
        if(u.i >  0x43c07fff ) * p_buffs = 32767;
        else if ( u.i < 0x43bf8000 ) *p_buffs = -32768;
        else *p_buffs = u.i - 0x43c00000;

        p_buffl++ ; p_buffs++ ;
    }
    p_state  = visual_fft_init();
    if( !p_state)
    {
        msg_Err(p_aout,"unable to initialize FFT transform");
        free( height );
        return -1;
    }
    if( !window_init( FFT_BUFFER_SIZE, &p_data->wind_param, &wind_ctx ) )
    {
        fft_close( p_state );
        free( height );
        msg_Err(p_aout,"unable to initialize FFT window");
        return -1;
    }
    p_buffs = p_s16_buff;
    for ( i = 0 ; i < FFT_BUFFER_SIZE; i++)
    {
        p_output[i]    = 0;
        p_buffer1[i] = *p_buffs;

        p_buffs += p_effect->i_nb_chans;
        if( p_buffs >= &p_s16_buff[p_buffer->i_nb_samples * p_effect->i_nb_chans] )
            p_buffs = p_s16_buff;
    }
    window_scale_in_place( p_buffer1, &wind_ctx );
    fft_perform( p_buffer1, p_output, p_state);
    for(i = 0; i < FFT_BUFFER_SIZE; i++)
    {
        int sqrti = sqrt(p_output[i]);
        p_dest[i] = sqrti >> 8;
    }

    i_nb_bands *= i_sections;

    for ( i = 0 ; i< i_nb_bands/i_sections ;i++)
    {
        /* We search the maximum on one scale */
        for( j = xscale[i] , y=0 ; j< xscale[ i + 1 ] ; j++ )
        {
            if ( p_dest[j] > y )
                 y = p_dest[j];
        }
        /* Calculate the height of the bar */
        y >>=7;/* remove some noise */
        if( y != 0)
        {
            int logy = log(y);
            height[i] = logy * y_scale;
            if(height[i] > 150)
                height[i] = 150;
        }
        else
        {
            height[i] = 0 ;
        }

        /* Draw the bar now */
        i_band_width = floor( p_effect->i_width / (i_nb_bands/i_sections)) ;

        if( i_amp * height[i] > peaks[i])
        {
            peaks[i] = i_amp * height[i];
        }
        else if (peaks[i] > 0 )
        {
            peaks[i] -= PEAK_SPEED;
            if( peaks[i] < i_amp * height[i] )
            {
                peaks[i] = i_amp * height[i];
            }
            if( peaks[i] < 0 )
            {
                peaks[i] = 0;
            }
        }

        if( i_original != 0 )
        {
        if( peaks[i] > 0 && i_peak )
        {
            if( peaks[i] >= p_effect->i_height )
                peaks[i] = p_effect->i_height - 2;
            i_line = peaks[i];

            for( j = 0 ; j< i_band_width - i_separ; j++)
            {
               for( k = 0 ; k< 3 ; k ++)
               {
                   //* Draw the peak
                     *(p_picture->p[0].p_pixels +
                    (p_effect->i_height - i_line -1 -k ) *
                     p_picture->p[0].i_pitch + (i_band_width*i +j) )
                                    = 0xff;

                    *(p_picture->p[1].p_pixels +
                     ( ( p_effect->i_height - i_line ) / 2 -1 -k/2 ) *
                     p_picture->p[1].i_pitch +
                    ( ( i_band_width * i + j ) /2  ) )
                                    = 0x00;

                   if( 0x04 * (i_line + k ) - 0x0f > 0 )
                   {
                       if ( 0x04 * (i_line + k ) -0x0f < 0xff)
                           *(p_picture->p[2].p_pixels  +
                            ( ( p_effect->i_height - i_line ) / 2 - 1 -k/2 ) *
                             p_picture->p[2].i_pitch +
                             ( ( i_band_width * i + j ) /2  ) )
                                    = ( 0x04 * ( i_line + k ) ) -0x0f ;
                       else
                           *(p_picture->p[2].p_pixels  +
                            ( ( p_effect->i_height - i_line ) / 2 - 1 -k/2 ) *
                             p_picture->p[2].i_pitch +
                             ( ( i_band_width * i + j ) /2  ) )
                                    = 0xff;
                   }
                   else
                   {
                        *(p_picture->p[2].p_pixels  +
                         ( ( p_effect->i_height - i_line ) / 2 - 1 -k/2 ) *
                         p_picture->p[2].i_pitch +
                         ( ( i_band_width * i + j ) /2  ) )
                               = 0x10 ;
                   }
               }
            }
        }
        if(height[i] * i_amp > p_effect->i_height)
            height[i] = floor(p_effect->i_height / i_amp );

        for(i_line = 0 ; i_line < i_amp * height[i]; i_line ++ )
        {
            for( j = 0 ; j< i_band_width - i_separ ; j++)
            {
               *(p_picture->p[0].p_pixels +
                 (p_effect->i_height - i_line -1) *
                  p_picture->p[0].i_pitch + (i_band_width*i +j) ) = 0xff;

                *(p_picture->p[1].p_pixels +
                 ( ( p_effect->i_height - i_line ) / 2 -1) *
                 p_picture->p[1].i_pitch +
                 ( ( i_band_width * i + j ) /2  ) ) = 0x00;

               if( 0x04 * i_line - 0x0f > 0 )
               {
                    if( 0x04 * i_line - 0x0f < 0xff )
                         *(p_picture->p[2].p_pixels  +
                          ( ( p_effect->i_height - i_line ) / 2 - 1) *
                           p_picture->p[2].i_pitch +
                           ( ( i_band_width * i + j ) /2  ) ) =
                               ( 0x04 * i_line) -0x0f ;
                    else
                         *(p_picture->p[2].p_pixels  +
                          ( ( p_effect->i_height - i_line ) / 2 - 1) *
                           p_picture->p[2].i_pitch +
                           ( ( i_band_width * i + j ) /2  ) ) =
                                       0xff;
               }
               else
               {
                    *(p_picture->p[2].p_pixels  +
                     ( ( p_effect->i_height - i_line ) / 2 - 1) *
                     p_picture->p[2].i_pitch +
                     ( ( i_band_width * i + j ) /2  ) ) =
                            0x10 ;
               }
            }
        }
        }
    }

    band_sep_angle = 360.0 / i_nb_bands;
    section_sep_angle = 360.0 / i_sections;
    if( i_peak_height < 1 )
        i_peak_height = 1;
    max_band_length = p_effect->i_height / 2 - ( i_rad + i_peak_height + 1 );

    i_band_width = floor( 360 / i_nb_bands - i_separ );
    if( i_band_width < 1 )
        i_band_width = 1;

    for( c = 0 ; c < i_sections ; c++ )
    for( i = 0 ; i < (i_nb_bands / i_sections) ; i++ )
    {
        /* DO A PEAK */
        if( peaks[i] > 0 && i_peak )
        {
            if( peaks[i] >= p_effect->i_height )
                peaks[i] = p_effect->i_height - 2;
            i_line = peaks[i];

            /* circular line pattern(so color blend is more visible) */
            for( j = 0 ; j < i_peak_height ; j++ )
            {
                //x = p_picture->p[0].i_pitch / 2;
                x = p_effect->i_width / 2;
                y = p_effect->i_height / 2;
                xx = x;
                yy = y;
                for( k = 0 ; k < (i_band_width + i_extra_width) ; k++ )
                {
                    x = xx;
                    y = yy;
                    a = ( (i+1) * band_sep_angle + section_sep_angle * (c+1) + k )
                        * 3.141592 / 180.0;
                    x += (double)( cos(a) * (double)( i_line + j + i_rad ) );
                    y += (double)( -sin(a) * (double)( i_line + j + i_rad ) );

                    *(p_picture->p[0].p_pixels + x + y * p_picture->p[0].i_pitch
                    ) = 255;/* Y(R,G,B); */

                    x /= 2;
                    y /= 2;

                    *(p_picture->p[1].p_pixels + x + y * p_picture->p[1].i_pitch
                    ) = 0;/* U(R,G,B); */

                    if( 0x04 * (i_line + k ) - 0x0f > 0 )
                    {
                        if ( 0x04 * (i_line + k ) -0x0f < 0xff)
                            *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                            ) = ( 0x04 * ( i_line + k ) ) -(color1-1);/* -V(R,G,B); */
                        else
                            *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                            ) = 255;/* V(R,G,B); */
                    }
                    else
                    {
                        *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                        ) = color1;/* V(R,G,B); */
                    }
                }
            }
        }

        if( (height[i] * i_amp) > p_effect->i_height )
            height[i] = floor( p_effect->i_height / i_amp );

        /* DO BASE OF BAND (mostly makes a circle) */
        if( i_show_base != 0 )
        {
            //x = p_picture->p[0].i_pitch / 2;
            x = p_effect->i_width / 2;
            y = p_effect->i_height / 2;

            a =  ( (i+1) * band_sep_angle + section_sep_angle * (c+1) )
                * 3.141592 / 180.0;
            x += (double)( cos(a) * (double)i_rad );/* newb-forceful casting */
            y += (double)( -sin(a) * (double)i_rad );

            *(p_picture->p[0].p_pixels + x + y * p_picture->p[0].i_pitch
            ) = 255;/* Y(R,G,B); */

            x /= 2;
            y /= 2;

            *(p_picture->p[1].p_pixels + x + y * p_picture->p[1].i_pitch
            ) = 0;/* U(R,G,B); */

            if( 0x04 * i_line - 0x0f > 0 )
            {
                if( 0x04 * i_line -0x0f < 0xff)
                    *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                    ) = ( 0x04 * i_line) -(color1-1);/* -V(R,G,B); */
                else
                    *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                    ) = 255;/* V(R,G,B); */
            }
            else
            {
                *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                ) = color1;/* V(R,G,B); */
            }
        }

        /* DO A BAND */
        if( i_show_bands != 0 )
        for( j = 0 ; j < i_band_width ; j++ )
        {
            x = p_effect->i_width / 2;
            y = p_effect->i_height / 2;
            xx = x;
            yy = y;
            a = ( (i+1) * band_sep_angle + section_sep_angle * (c+1) + j )
                * 3.141592/180.0;

            for( k = (i_rad+1) ; k < max_band_length ; k++ )
            {
                if( (k-i_rad) > height[i] )
                    break;/* uhh.. */

                x = xx;
                y = yy;
                x += (double)( cos(a) * (double)k );/* newbed! */
                y += (double)( -sin(a) * (double)k );

                *(p_picture->p[0].p_pixels + x + y * p_picture->p[0].i_pitch
                ) = 255;

                x /= 2;
                y /= 2;

                *(p_picture->p[1].p_pixels + x + y * p_picture->p[1].i_pitch
                ) = 0;

                if( 0x04 * i_line - 0x0f > 0 )
                {
                    if ( 0x04 * i_line -0x0f < 0xff)
                        *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                        ) = ( 0x04 * i_line) -(color1-1);
                    else
                        *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                        ) = 255;
                }
                else
                {
                    *(p_picture->p[2].p_pixels + x + y * p_picture->p[2].i_pitch
                    ) = color1;
                }
            }
        }
    }

    window_close( &wind_ctx );

    fft_close( p_state );

    free( height );

    return 0;
}

static void spectrometer_Free( void *data )
{
    spectrometer_data *p_data = data;

    if( p_data != NULL )
    {
        free( p_data->peaks );
        free( p_data->p_prev_s16_buff );
        free( p_data );
    }
}


/*****************************************************************************
 * scope_Run: scope effect
 *****************************************************************************/
static int scope_Run(visual_effect_t * p_effect, vlc_object_t *p_aout,
                     const block_t * p_buffer , picture_t * p_picture)
{
    VLC_UNUSED(p_aout);

    int i_index;
    float *p_sample ;
    uint8_t *ppp_area[2][3];

    for( i_index = 0 ; i_index < 2 ; i_index++ )
    {
        for( int j = 0 ; j < 3 ; j++ )
        {
            ppp_area[i_index][j] =
                p_picture->p[j].p_pixels + (i_index * 2 + 1) * p_picture->p[j].i_lines
                / 4 * p_picture->p[j].i_pitch;
        }
    }

    for( i_index = 0, p_sample = (float *)p_buffer->p_buffer;
            i_index < __MIN( p_effect->i_width, (int)p_buffer->i_nb_samples );
            i_index++ )
    {
        int8_t i_value;

        /* Left channel */
        i_value =  p_sample[p_effect->i_idx_left] * 127;
        *(ppp_area[0][0]
                + p_picture->p[0].i_pitch * i_index / p_effect->i_width
                + p_picture->p[0].i_lines * i_value / 512
                * p_picture->p[0].i_pitch) = 0xbf;
        *(ppp_area[0][1]
                + p_picture->p[1].i_pitch * i_index / p_effect->i_width
                + p_picture->p[1].i_lines * i_value / 512
                * p_picture->p[1].i_pitch) = 0xff;


        /* Right channel */
        i_value = p_sample[p_effect->i_idx_right] * 127;
        *(ppp_area[1][0]
                + p_picture->p[0].i_pitch * i_index / p_effect->i_width
                + p_picture->p[0].i_lines * i_value / 512
                * p_picture->p[0].i_pitch) = 0x9f;
        *(ppp_area[1][2]
                + p_picture->p[2].i_pitch * i_index / p_effect->i_width
                + p_picture->p[2].i_lines * i_value / 512
                * p_picture->p[2].i_pitch) = 0xdd;

        p_sample += p_effect->i_nb_chans;
    }
    return 0;
}


/*****************************************************************************
 * vuMeter_Run: vu meter effect
 *****************************************************************************/
static int vuMeter_Run(visual_effect_t * p_effect, vlc_object_t *p_aout,
                       const block_t * p_buffer , picture_t * p_picture)
{
    VLC_UNUSED(p_aout);
    float i_value_l = 0;
    float i_value_r = 0;

    /* Compute the peak values */
    for ( unsigned i = 0 ; i < p_buffer->i_nb_samples; i++ )
    {
        const float *p_sample = (float *)p_buffer->p_buffer;
        float ch;

        ch = p_sample[p_effect->i_idx_left] * 256;
        if (ch > i_value_l)
            i_value_l = ch;

        ch = p_sample[p_effect->i_idx_right] * 256;
        if (ch > i_value_r)
            i_value_r = ch;

        p_sample += p_effect->i_nb_chans;
    }

    i_value_l = fabsf(i_value_l);
    i_value_r = fabsf(i_value_r);

    /* Stay under maximum value admited */
    if ( i_value_l > 200 * M_PI_2 )
        i_value_l = 200 * M_PI_2;
    if ( i_value_r > 200 * M_PI_2 )
        i_value_r = 200 * M_PI_2;

    float *i_value;

    if( !p_effect->p_data )
    {
        /* Allocate memory to save hand positions */
        p_effect->p_data = vlc_alloc( 2, sizeof(float) );
        i_value = p_effect->p_data;
        i_value[0] = i_value_l;
        i_value[1] = i_value_r;
    }
    else
    {
        /* Make the hands go down slowly if the current values are slower
           than the previous */
        i_value = p_effect->p_data;

        if ( i_value_l > i_value[0] - 6 )
            i_value[0] = i_value_l;
        else
            i_value[0] = i_value[0] - 6;

        if ( i_value_r > i_value[1] - 6 )
            i_value[1] = i_value_r;
        else
            i_value[1] = i_value[1] - 6;
    }

    int x, y;
    float teta;
    float teta_grad;

    int start_x = p_effect->i_width / 2 - 120; /* i_width.min = 532 (visual.c) */

    for ( int j = 0; j < 2; j++ )
    {
        /* Draw the two scales */
        int k = 0;
        teta_grad = GRAD_ANGLE_MIN;
        for ( teta = -M_PI_4; teta <= M_PI_4; teta = teta + 0.003 )
        {
            for ( unsigned i = 140; i <= 150; i++ )
            {
                y = i * cos(teta) + 20;
                x = i * sin(teta) + start_x + 240 * j;
                /* Compute the last color for the gradation */
                if (teta >= teta_grad + GRAD_INCR && teta_grad <= GRAD_ANGLE_MAX)
                {
                    teta_grad = teta_grad + GRAD_INCR;
                    k = k + 5;
                }
                *(p_picture->p[0].p_pixels +
                        (p_picture->p[0].i_lines - y - 1 ) * p_picture->p[0].i_pitch
                        + x ) = 0x45;
                *(p_picture->p[1].p_pixels +
                        (p_picture->p[1].i_lines - y / 2 - 1 ) * p_picture->p[1].i_pitch
                        + x / 2 ) = 0x0;
                *(p_picture->p[2].p_pixels +
                        (p_picture->p[2].i_lines - y / 2 - 1 ) * p_picture->p[2].i_pitch
                        + x / 2 ) = 0x4D + k;
            }
        }

        /* Draw the two hands */
        teta = (float)i_value[j] / 200 - M_PI_4;
        for ( int i = 0; i <= 150; i++ )
        {
            y = i * cos(teta) + 20;
            x = i * sin(teta) + start_x + 240 * j;
            *(p_picture->p[0].p_pixels +
                    (p_picture->p[0].i_lines - y - 1 ) * p_picture->p[0].i_pitch
                    + x ) = 0xAD;
            *(p_picture->p[1].p_pixels +
                    (p_picture->p[1].i_lines - y / 2 - 1 ) * p_picture->p[1].i_pitch
                    + x / 2 ) = 0xFC;
            *(p_picture->p[2].p_pixels +
                    (p_picture->p[2].i_lines - y / 2 - 1 ) * p_picture->p[2].i_pitch
                    + x / 2 ) = 0xAC;
        }

        /* Draw the hand bases */
        for ( teta = -M_PI_2; teta <= M_PI_2 + 0.01; teta = teta + 0.003 )
        {
            for ( int i = 0; i < 10; i++ )
            {
                y = i * cos(teta) + 20;
                x = i * sin(teta) + start_x + 240 * j;
                *(p_picture->p[0].p_pixels +
                        (p_picture->p[0].i_lines - y - 1 ) * p_picture->p[0].i_pitch
                        + x ) = 0xFF;
                *(p_picture->p[1].p_pixels +
                        (p_picture->p[1].i_lines - y / 2 - 1 ) * p_picture->p[1].i_pitch
                        + x / 2 ) = 0x80;
                *(p_picture->p[2].p_pixels +
                        (p_picture->p[2].i_lines - y / 2 - 1 ) * p_picture->p[2].i_pitch
                        + x / 2 ) = 0x80;
            }
        }

    }

    return 0;
}

/* Table of effects */
const struct visual_cb_t effectv[] = {
    { "scope",        scope_Run,        dummy_Free        },
    { "vuMeter",      vuMeter_Run,      dummy_Free        },
    { "spectrum",     spectrum_Run,     spectrum_Free     },
    { "spectrometer", spectrometer_Run, spectrometer_Free },
    { "dummy",        dummy_Run,        dummy_Free        },
};
const unsigned effectc = sizeof (effectv) / sizeof (effectv[0]);
