/*****************************************************************************
 * scope.c : Scope effect module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: scope.c,v 1.3 2003/08/18 14:57:09 sigmunau Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
#include "aout_internal.h"

#define SCOPE_WIDTH 320
#define SCOPE_HEIGHT 240
#define SCOPE_ASPECT (VOUT_ASPECT_FACTOR*SCOPE_WIDTH/SCOPE_HEIGHT)

/*****************************************************************************
 * aout_sys_t: scope audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some scope specific variables.
 *****************************************************************************/
typedef struct aout_filter_sys_t
{
    aout_fifo_t *p_aout_fifo;
    vout_thread_t *p_vout;
} aout_filter_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );             
static void Close        ( vlc_object_t * );                   

static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("scope effect") ); 
    set_capability( "audio filter", 0 );
    set_callbacks( Open, Close );
    add_shortcut( "scope" );
vlc_module_end();

/*****************************************************************************
 * Open: open a scope effect plugin
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_filter_t *p_aout = (aout_filter_t *)p_this;
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2') //AOUT_FMT_U16_NE
         || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )//AOUT_FMT_U16_NE )
    {
        msg_Warn( p_filter, "Bad input or output format" );
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        msg_Warn( p_filter, "input and output formats are not similar" );
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 1;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_filter_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }

    /* Open video output */
    p_aout->p_sys->p_vout =
        vout_Create( p_aout, SCOPE_WIDTH, SCOPE_HEIGHT,
                     VLC_FOURCC('I','4','2','0'), SCOPE_ASPECT );

    if( p_aout->p_sys->p_vout == NULL )
    {
        msg_Err( p_aout, "no suitable vout module" );
        free( p_aout->p_sys );
        return -1;
    }

    return( 0 );
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    picture_t *p_outpic;
    int i_index, i_image;
    int i_size = p_in_buf->i_size;
    byte_t *p_buffer = p_in_buf->p_buffer;
    uint8_t  *ppp_area[2][3];
    float *p_sample;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;
    for( i_image = 0; (i_image + 1) * SCOPE_WIDTH * 8 < i_size ; i_image++ )
    {
        /* Don't stay here forever */
        if( mdate() >= p_in_buf->start_date - 10000 )
        {
            break;
        }
        /* This is a new frame. Get a structure from the video_output. */
        while( ( p_outpic = vout_CreatePicture( p_filter->p_sys->p_vout, 0, 0, 0 ) )
                  == NULL )
        {
            if( p_aout->b_die )
            {
                return;
            }
            msleep( 1 );/* Not sleeping here makes us use 100% cpu,
                         * sleeping too much absolutly kills audio
                         * quality. 1 seems to be a good value */
        }

        /* Blank the picture */
        for( i_index = 0 ; i_index < p_outpic->i_planes ; i_index++ )
        {
            memset( p_outpic->p[i_index].p_pixels, i_index ? 0x80 : 0x00,
                    p_outpic->p[i_index].i_lines * p_outpic->p[i_index].i_pitch );
        }

        /* We only support 2 channels for now */
        for( i_index = 0 ; i_index < 2 ; i_index++ )
        {
            int j;
            for( j = 0 ; j < 3 ; j++ )
            {
                ppp_area[i_index][j] =
                    p_outpic->p[j].p_pixels + i_index * p_outpic->p[j].i_lines
                                / p_filter->input.i_original_channels * p_outpic->p[j].i_pitch;
            }
        }

        for( i_index = 0, p_sample = (float*)p_buffer;
             i_index < SCOPE_WIDTH;
             i_index++ )
        {
            int i;
            int i_tmp_value;
            uint8_t i_value;

            for( i = 0 ; i < 2 ; i++ )
            {
                /* Left channel */
                if ( *p_sample >= 1.0 ) i_tmp_value = 32767;
                else if ( *p_sample < -1.0 ) i_tmp_value = -32768;
                else i_tmp_value = *p_sample * 32768.0;
                p_sample++;
                i_value = i_tmp_value / 256 + 128;
                *(ppp_area[0][0]
                   + p_outpic->p[0].i_pitch * i_index / SCOPE_WIDTH
                   + p_outpic->p[0].i_lines * i_value / 512
                       * p_outpic->p[0].i_pitch) = 0xbf;
                *(ppp_area[0][1]
                   + p_outpic->p[1].i_pitch * i_index / SCOPE_WIDTH
                   + p_outpic->p[1].i_lines * i_value / 512
                      * p_outpic->p[1].i_pitch) = 0xff;

                /* Right channel */
                if ( *p_sample >= 1.0 ) i_tmp_value = 32767;
                else if ( *p_sample < -1.0 ) i_tmp_value = -32768;
                else i_tmp_value = *p_sample * 32768.0;
                p_sample++;
                i_value = i_tmp_value / 256 + 128;
                *(ppp_area[1][0]
                   + p_outpic->p[0].i_pitch * i_index / SCOPE_WIDTH
                   + p_outpic->p[0].i_lines * i_value / 512
                      * p_outpic->p[0].i_pitch) = 0x9f;
                *(ppp_area[1][2]
                   + p_outpic->p[2].i_pitch * i_index / SCOPE_WIDTH
                   + p_outpic->p[2].i_lines * i_value / 512
                      * p_outpic->p[2].i_pitch) = 0xdd;
            }
        }

        /* Display the picture - FIXME: find a better date :-) */
        vout_DatePicture( p_filter->p_sys->p_vout, p_outpic,
                          p_in_buf->start_date + i_image * 20000 );
        vout_DisplayPicture( p_filter->p_sys->p_vout, p_outpic );

        p_buffer += SCOPE_WIDTH * 8;
    }
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    /* Kill video output */
    vout_Destroy( p_filter->p_sys->p_vout );

    free( p_filter->p_sys );
}

