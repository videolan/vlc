/*****************************************************************************
 * scope.c : Scope effect module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: scope.c,v 1.2 2002/08/12 09:34:15 sam Exp $
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

#define SCOPE_WIDTH 320
#define SCOPE_HEIGHT 240
#define SCOPE_ASPECT (VOUT_ASPECT_FACTOR*SCOPE_WIDTH/SCOPE_HEIGHT)

/*****************************************************************************
 * aout_sys_t: scope audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some scope specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    aout_fifo_t *p_aout_fifo;

    aout_thread_t *p_aout;
    vout_thread_t *p_vout;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );             
static void Close        ( vlc_object_t * );                   

static int  SetFormat    ( aout_thread_t * );  
static int  GetBufInfo   ( aout_thread_t *, int );
static void Play         ( aout_thread_t *, byte_t *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("scope effect") ); 
    set_capability( "audio output", 0 );
    set_callbacks( Open, Close );
    add_shortcut( "scope" );
vlc_module_end();

/*****************************************************************************
 * Open: open a scope effect plugin
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;
    char *psz_method;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }

    psz_method = config_GetPsz( p_aout, "aout" );
    if( psz_method )
    {
        if( !*psz_method )
        {
            free( psz_method );
            return -1;
        }
    }
    else
    {
        return -1;
    }

    /* Open video output */
    p_aout->p_sys->p_vout =
        vout_CreateThread( p_aout, SCOPE_WIDTH, SCOPE_HEIGHT,
                           VLC_FOURCC('I','4','2','0'), SCOPE_ASPECT );

    if( p_aout->p_sys->p_vout == NULL )
    {
        msg_Err( p_aout, "no suitable vout module" );
        free( p_aout->p_sys );
        return -1;
    }

    /* Open audio output  */
    p_aout->p_sys->p_aout = vlc_object_create( p_aout, VLC_OBJECT_AOUT );

    p_aout->p_sys->p_aout->i_format   = p_aout->i_format;
    p_aout->p_sys->p_aout->i_rate     = p_aout->i_rate;
    p_aout->p_sys->p_aout->i_channels = p_aout->i_channels;

    p_aout->p_sys->p_aout->p_module =
                  module_Need( p_aout->p_sys->p_aout, "audio output", "" );
    if( p_aout->p_sys->p_aout->p_module == NULL )
    {
        msg_Err( p_aout, "no suitable aout module" );
        vlc_object_destroy( p_aout->p_sys->p_aout );
        vout_DestroyThread( p_aout->p_sys->p_vout );
        free( p_aout->p_sys );
        return -1;
    }

    vlc_object_attach( p_aout->p_sys->p_aout, p_aout );

    p_aout->pf_setformat = SetFormat;
    p_aout->pf_getbufinfo = GetBufInfo;
    p_aout->pf_play = Play;

    return( 0 );
}

/*****************************************************************************
 * SetFormat: set the output format
 *****************************************************************************/
static int SetFormat( aout_thread_t *p_aout )
{
    int i_ret;

    /* Force the output method */
    p_aout->p_sys->p_aout->i_format = p_aout->i_format;
    p_aout->p_sys->p_aout->i_channels = p_aout->i_channels;
    p_aout->p_sys->p_aout->i_rate = p_aout->i_rate;

    /*
     * Initialize audio device
     */
    i_ret = p_aout->p_sys->p_aout->pf_setformat( p_aout->p_sys->p_aout );

    if( i_ret )
    {
        return i_ret;
    }

    if( p_aout->p_sys->p_aout->i_format != p_aout->i_format
         || p_aout->p_sys->p_aout->i_channels != p_aout->i_channels )
    {
        msg_Err( p_aout, "plugin is not very cooperative" );
        return 0;
    }

    p_aout->i_channels = p_aout->p_sys->p_aout->i_channels;
    p_aout->i_format = p_aout->p_sys->p_aout->i_format;
    p_aout->i_rate = p_aout->p_sys->p_aout->i_rate;

    return 0;
}

/*****************************************************************************
 * GetBufInfo: buffer status query
 *****************************************************************************/
static int GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    return p_aout->p_sys->p_aout->pf_getbufinfo( p_aout->p_sys->p_aout,
                                                 i_buffer_limit );
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void Play( aout_thread_t *p_aout, byte_t *p_buffer, int i_size )
{
    picture_t *p_outpic;
    int i_index, i_image;
    u8  *ppp_area[2][3];
    u16 *p_sample;

    /* Play the real sound */
    p_aout->p_sys->p_aout->pf_play( p_aout->p_sys->p_aout, p_buffer, i_size );

    for( i_image = 0; (i_image + 1) * SCOPE_WIDTH * 8 < i_size ; i_image++ )
    {
        /* Don't stay here forever */
        if( mdate() >= p_aout->date - 10000 )
        {
            break;
        }

        /* This is a new frame. Get a structure from the video_output. */
        while( ( p_outpic = vout_CreatePicture( p_aout->p_sys->p_vout, 0, 0, 0 ) )
                  == NULL )
        {
            if( p_aout->b_die )
            {
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
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
                                / p_aout->i_channels * p_outpic->p[j].i_pitch;
            }
        }

        for( i_index = 0, p_sample = (u16*)p_buffer;
             i_index < SCOPE_WIDTH;
             i_index++ )
        {
            int i;
            u8 i_value;

            for( i = 0 ; i < 2 ; i++ )
            {
                /* Left channel */
                i_value = *p_sample++ / 256 + 128;
                *(ppp_area[0][0]
                   + p_outpic->p[0].i_pitch * i_index / SCOPE_WIDTH
                   + p_outpic->p[0].i_lines * i_value / 512
                       * p_outpic->p[0].i_pitch) = 0xbf;
                *(ppp_area[0][1]
                   + p_outpic->p[1].i_pitch * i_index / SCOPE_WIDTH
                   + p_outpic->p[1].i_lines * i_value / 512
                      * p_outpic->p[1].i_pitch) = 0xff;

                /* Right channel */
                i_value = *p_sample++ / 256 + 128;
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
        vout_DatePicture( p_aout->p_sys->p_vout, p_outpic,
                          p_aout->date + i_image * 20000 );
        vout_DisplayPicture( p_aout->p_sys->p_vout, p_outpic );

        p_buffer += SCOPE_WIDTH * 4;
    }
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;

    /* Kill audio output */
    module_Unneed( p_aout->p_sys->p_aout, p_aout->p_sys->p_aout->p_module );
    vlc_object_detach( p_aout->p_sys->p_aout );
    vlc_object_destroy( p_aout->p_sys->p_aout );

    /* Kill video output */
    vout_DestroyThread( p_aout->p_sys->p_vout );

    free( p_aout->p_sys );
}

