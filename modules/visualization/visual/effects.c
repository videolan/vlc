/*****************************************************************************
 * effects.c : Effects for the visualization system
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include "audio_output.h"
#include "aout_internal.h"

#include "visual.h"
#include <math.h>

#include "fft.h"

#define PEAK_SPEED 1

/*****************************************************************************
 * dummy_Run
 *****************************************************************************/
int dummy_Run( visual_effect_t * p_effect, aout_instance_t *p_aout,
               aout_buffer_t * p_buffer , picture_t * p_picture)
{
    return 0;
}

/*****************************************************************************
 * spectrum_Run: spectrum analyser
 *****************************************************************************/
int spectrum_Run(visual_effect_t * p_effect, aout_instance_t *p_aout,
                 aout_buffer_t * p_buffer , picture_t * p_picture)
{
    float p_output[FFT_BUFFER_SIZE];  /* Raw FFT Result  */
    int *height;                      /* Bar heights */
    int *peaks;                       /* Peaks */
    int i_nb_bands;                   /* number of bands */
    int i_band_width;                 /* width of bands */
    int i_separ;                      /* Should we let blanks ? */
    int i_amp;                        /* Vertical amplification */
    int i_peak;                       /* Should we draw peaks ? */
    char *psz_parse = NULL;           /* Args line */

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

    int i , j , y , k;
    int i_line;
    int16_t p_dest[FFT_BUFFER_SIZE];      /* Adapted FFT result */
    int16_t p_buffer1[FFT_BUFFER_SIZE];   /* Buffer on which we perform
                                             the FFT (first channel) */

    float *p_buffl =                     /* Original buffer */
            (float*)p_buffer->p_buffer;

    int16_t  *p_buffs;                    /* int16_t converted buffer */
    int16_t  *p_s16_buff = NULL;                /* int16_t converted buffer */

    p_s16_buff = (int16_t*)malloc(
              p_buffer->i_nb_samples * p_effect->i_nb_chans * sizeof(int16_t));

    if( !p_s16_buff )
    {
        msg_Err(p_aout,"Out of memory");
        return -1;
    }

    p_buffs = p_s16_buff;
    i_nb_bands = config_GetInt ( p_aout, "visual-nbbands" );
    i_separ    = config_GetInt( p_aout, "visual-separ" );
    i_amp     = config_GetInt ( p_aout, "visual-amp" );
    i_peak     = config_GetInt ( p_aout, "visual-peaks" );

    if( i_nb_bands == 20)
    {
        xscale = xscale1;
    }
    else
    {
        i_nb_bands = 80;
        xscale = xscale2;
    }

    if( !p_effect->p_data )
    {
        p_effect->p_data=(void *)malloc(i_nb_bands * sizeof(int) );
        if( !p_effect->p_data)
        {
            msg_Err(p_aout,"Out of memory");
            return -1;
        }
        peaks = (int *)p_effect->p_data;
        for( i = 0 ; i < i_nb_bands ; i++)
        {
           peaks[i] = 0;
        }

    }
    else
    {
        peaks =(int *)p_effect->p_data;
    }


    height = (int *)malloc( i_nb_bands * sizeof(int) );
    if( !height)
    {
        msg_Err(p_aout,"Out of memory");
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
        msg_Err(p_aout,"Unable to initialize FFT transform");
        return -1;
    }
    p_buffs = p_s16_buff;
    for ( i = 0 ; i < FFT_BUFFER_SIZE ; i++)
    {
        p_output[i]    = 0;
        p_buffer1[i] = *p_buffs;
        p_buffs      = p_buffs + p_effect->i_nb_chans;
    }
    fft_perform( p_buffer1, p_output, p_state);
    for(i= 0; i< FFT_BUFFER_SIZE ; i++ )
        p_dest[i] = ( (int) sqrt( p_output [ i + 1 ] ) ) >> 8;

    for ( i = 0 ; i< i_nb_bands ;i++)
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
            height[i] = (int)log(y)* y_scale;
               if(height[i] > 150)
                  height[i] = 150;
        }
        else
        {
            height[i] = 0 ;
        }

        /* Draw the bar now */
        i_band_width = floor( p_effect->i_width / i_nb_bands) ;

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

        if( peaks[i] > 0 && i_peak )
        {
            if( peaks[i] >= p_effect->i_height )
                peaks[i] = p_effect->i_height - 2;
            i_line = peaks[i];

            for( j = 0 ; j< i_band_width - i_separ; j++)
            {
               for( k = 0 ; k< 3 ; k ++)
               {
                   /* Draw the peak */
                     *(p_picture->p[0].p_pixels +
                    (p_picture->p[0].i_lines - i_line -1 -k ) *
                     p_picture->p[0].i_pitch + (i_band_width*i +j) )
                                    = 0xff;

                    *(p_picture->p[1].p_pixels +
                     (p_picture->p[1].i_lines - i_line /2 -1 -k/2 ) *
                     p_picture->p[1].i_pitch +
                    ( ( i_band_width * i + j ) /2  ) )
                                    = 0x00;

                   if( 0x04 * (i_line + k ) - 0x0f > 0 )
                   {
                       if ( 0x04 * (i_line + k ) -0x0f < 0xff)
                           *(p_picture->p[2].p_pixels  +
                            (p_picture->p[2].i_lines - i_line /2 - 1 -k/2 ) *
                             p_picture->p[2].i_pitch +
                             ( ( i_band_width * i + j ) /2  ) )
                                    = ( 0x04 * ( i_line + k ) ) -0x0f ;
                       else
                           *(p_picture->p[2].p_pixels  +
                            (p_picture->p[2].i_lines - i_line /2 - 1 -k/2 ) *
                             p_picture->p[2].i_pitch +
                             ( ( i_band_width * i + j ) /2  ) )
                                    = 0xff;
                   }
                   else
                   {
                        *(p_picture->p[2].p_pixels  +
                         (p_picture->p[2].i_lines - i_line /2 - 1 -k/2 ) *
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
                 (p_picture->p[0].i_lines - i_line -1) *
                  p_picture->p[0].i_pitch + (i_band_width*i +j) ) = 0xff;

                *(p_picture->p[1].p_pixels +
                 (p_picture->p[1].i_lines - i_line /2 -1) *
                 p_picture->p[1].i_pitch +
                 ( ( i_band_width * i + j ) /2  ) ) = 0x00;

               if( 0x04 * i_line - 0x0f > 0 )
               {
                    if( 0x04 * i_line - 0x0f < 0xff )
                         *(p_picture->p[2].p_pixels  +
                          (p_picture->p[2].i_lines - i_line /2 - 1) *
                           p_picture->p[2].i_pitch +
                           ( ( i_band_width * i + j ) /2  ) ) =
                               ( 0x04 * i_line) -0x0f ;
                    else
                         *(p_picture->p[2].p_pixels  +
                          (p_picture->p[2].i_lines - i_line /2 - 1) *
                           p_picture->p[2].i_pitch +
                           ( ( i_band_width * i + j ) /2  ) ) =
                                       0xff;
               }
               else
               {
                    *(p_picture->p[2].p_pixels  +
                     (p_picture->p[2].i_lines - i_line /2 - 1) *
                     p_picture->p[2].i_pitch +
                     ( ( i_band_width * i + j ) /2  ) ) =
                            0x10 ;
               }
            }
        }
    }

    fft_close( p_state );

    if( p_s16_buff != NULL )
    {
        free( p_s16_buff );
        p_s16_buff = NULL;
    }

    if(height) free(height);

    if(psz_parse) free(psz_parse);

    return 0;
}


/*****************************************************************************
 * scope_Run: scope effect
 *****************************************************************************/
int scope_Run(visual_effect_t * p_effect, aout_instance_t *p_aout,
              aout_buffer_t * p_buffer , picture_t * p_picture)
{
    int i_index;
    float *p_sample ;
    uint8_t *ppp_area[2][3];


        for( i_index = 0 ; i_index < 2 ; i_index++ )
        {
            int j;
            for( j = 0 ; j < 3 ; j++ )
            {
                ppp_area[i_index][j] =
                    p_picture->p[j].p_pixels + i_index * p_picture->p[j].i_lines
                                / 2 * p_picture->p[j].i_pitch;
            }
        }

        for( i_index = 0, p_sample = (float *)p_buffer->p_buffer;
             i_index < p_effect->i_width;
             i_index++ )
        {
            uint8_t i_value;

            /* Left channel */
            i_value =  (*p_sample++ +1) * 127;
            *(ppp_area[0][0]
               + p_picture->p[0].i_pitch * i_index / p_effect->i_width
               + p_picture->p[0].i_lines * i_value / 512
                   * p_picture->p[0].i_pitch) = 0xbf;
            *(ppp_area[0][1]
                + p_picture->p[1].i_pitch * i_index / p_effect->i_width
                + p_picture->p[1].i_lines * i_value / 512
                   * p_picture->p[1].i_pitch) = 0xff;


           /* Right channel */
           i_value = ( *p_sample++ +1 ) * 127;
           *(ppp_area[1][0]
              + p_picture->p[0].i_pitch * i_index / p_effect->i_width
              + p_picture->p[0].i_lines * i_value / 512
                 * p_picture->p[0].i_pitch) = 0x9f;
           *(ppp_area[1][2]
              + p_picture->p[2].i_pitch * i_index / p_effect->i_width
              + p_picture->p[2].i_lines * i_value / 512
                * p_picture->p[2].i_pitch) = 0xdd;
        }
        return 0;
}


/*****************************************************************************
 * random_Run:  random plots display effect
 *****************************************************************************/
int random_Run(visual_effect_t * p_effect, aout_instance_t *p_aout,
              aout_buffer_t * p_buffer , picture_t * p_picture)
{
    int i_nb_plots;
    char *psz_parse= NULL;
    int i , i_y , i_u , i_v;
    int i_position;
    srand((unsigned int)mdate());

    if( p_effect->psz_args )
    {
        psz_parse = strdup( p_effect->psz_args );
        i_nb_plots = config_GetInt ( p_aout, "visual-stars" );
    }
    else
    {
        i_nb_plots = 200;
    }

    for( i = 0 ; i < i_nb_plots ; i++ )
    {
        i_position = rand() % (p_effect->i_width * p_effect->i_height );
        i_y = rand() % 256;
        i_u = rand() % 256;
        i_v = rand() % 256;
        *(p_picture->p[0].p_pixels + i_position )= i_u;
        *(p_picture->p[1].p_pixels + i_position/4) = i_v;
        *(p_picture->p[2].p_pixels + i_position/4) = i_y;
    }
    return 0;
}

/*****************************************************************************
 * blur_Run:  blur effect
 *****************************************************************************/
#if 0
  /* This code is totally crappy */
int blur_Run(visual_effect_t * p_effect, aout_instance_t *p_aout,
              aout_buffer_t * p_buffer , picture_t * p_picture)
{
    uint8_t * p_pictures;
    int i,j;
    int i_size;   /* Total size of one image */

    i_size = (p_picture->p[0].i_pitch * p_picture->p[0].i_lines +
              p_picture->p[1].i_pitch * p_picture->p[1].i_lines +
              p_picture->p[2].i_pitch * p_picture->p[2].i_lines );

    if( !p_effect->p_data )
    {
        p_effect->p_data=(void *)malloc( 5 * i_size *sizeof(uint8_t));

        if( !p_effect->p_data)
        {
            msg_Err(p_aout,"Out of memory");
            return -1;
        }
        p_pictures = (uint8_t *)p_effect->p_data;
    }
    else
    {
        p_pictures =(uint8_t *)p_effect->p_data;
    }

    for( i = 0 ; i < 5 ; i++)
    {
        for ( j = 0 ; j< p_picture->p[0].i_pitch * p_picture->p[0].i_lines; i++)
            p_picture->p[0].p_pixels[j] =
                    p_pictures[i * i_size + j] * (100 - 20 * i) /100 ;
        for ( j = 0 ; j< p_picture->p[1].i_pitch * p_picture->p[1].i_lines; i++)
            p_picture->p[1].p_pixels[j] =
                    p_pictures[i * i_size +
                    p_picture->p[0].i_pitch * p_picture->p[0].i_lines + j ];
        for ( j = 0 ; j< p_picture->p[2].i_pitch * p_picture->p[2].i_lines; i++)
            p_picture->p[2].p_pixels[j] =
                    p_pictures[i * i_size +
                    p_picture->p[0].i_pitch * p_picture->p[0].i_lines +
                    p_picture->p[1].i_pitch * p_picture->p[1].i_lines
                    + j ];
    }

    memcpy ( &p_pictures[ i_size ] , &p_pictures[0] , 4 * i_size * sizeof(uint8_t) );
}
#endif
