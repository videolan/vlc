/*****************************************************************************
 * goom.c: based on libgoom (see http://ios.free.fr/?page=projet&quoi=1)
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: goom.c,v 1.3 2003/12/22 23:46:23 hartman Exp $
 *
 * Authors: Laurent Aimar
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
#include <vlc/input.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
#include "aout_internal.h"

#include "goom_core.h"

#define GOOM_WIDTH 160
#define GOOM_HEIGHT 120
#define GOOM_ASPECT (VOUT_ASPECT_FACTOR*GOOM_WIDTH/GOOM_HEIGHT)

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

vlc_module_begin();
    set_description( _("goom effect") );
    set_capability( "audio filter", 0 );
    set_callbacks( Open, Close );
    add_shortcut( "goom" );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    VLC_COMMON_MEMBERS
    vout_thread_t *p_vout;

    char          *psz_title;

    vlc_mutex_t   lock;
    vlc_cond_t    wait;

    mtime_t       i_pts;
    int           i_samples;
    int16_t       samples[2][512];
} goom_thread_t;

typedef struct aout_filter_sys_t
{
    goom_thread_t *p_thread;

} aout_filter_sys_t;

static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

static void Thread    ( vlc_object_t * );

static char *TitleGet( vlc_object_t * );

/*****************************************************************************
 * Open: open a scope effect plugin
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2' )
         || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
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
    p_sys = p_filter->p_sys = malloc( sizeof( aout_filter_sys_t ) );

    /* Create goom thread */
    p_sys->p_thread = vlc_object_create( p_filter,
                                         sizeof( goom_thread_t ) );
    p_sys->p_thread->p_vout = vout_Request( p_filter, NULL,
                                           GOOM_WIDTH, GOOM_HEIGHT,
                                           VLC_FOURCC('R','V','3','2'),
                                           GOOM_ASPECT );
    if( p_sys->p_thread->p_vout == NULL )
    {
        msg_Err( p_filter, "no suitable vout module" );
        vlc_object_destroy( p_sys->p_thread );
        free( p_filter->p_sys );
        return VLC_EGENERIC;
    }
    vlc_mutex_init( p_filter, &p_sys->p_thread->lock );
    vlc_cond_init( p_filter, &p_sys->p_thread->wait );

    p_sys->p_thread->i_samples = 0;
    memset( &p_sys->p_thread->samples, 0, 512*2*2 );
    p_sys->p_thread->psz_title = TitleGet( VLC_OBJECT( p_filter ) );

    if( vlc_thread_create( p_sys->p_thread, "Goom Update Thread", Thread,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_filter, "cannot lauch goom thread" );
        vout_Destroy( p_sys->p_thread->p_vout );
        vlc_mutex_destroy( &p_sys->p_thread->lock );
        vlc_cond_destroy( &p_sys->p_thread->wait );
        if( p_sys->p_thread->psz_title )
        {
            free( p_sys->p_thread->psz_title );
        }
        vlc_object_destroy( p_sys->p_thread );
        free( p_filter->p_sys );
        return VLC_EGENERIC;
    }

    return( 0 );
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static inline int16_t FloatToInt16( float f )
{
    if( f >= 1.0 )
        return 32767;
    else if( f < -1.0 )
        return -32768;
    else
        return (int16_t)( f * 32768.0 );
}

static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    int16_t *p_isample;
    float   *p_fsample = (float*)p_in_buf->p_buffer;
    int     i_samples = 0;
    int     i_channels = aout_FormatNbChannels( &p_filter->input );


    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;

    /* copy samples */
    vlc_mutex_lock( &p_sys->p_thread->lock );
    p_sys->p_thread->i_pts = p_in_buf->start_date;
    if( p_sys->p_thread->i_samples >= 512 )
    {
        p_sys->p_thread->i_samples = 0;
    }
    p_isample = &p_sys->p_thread->samples[0][p_sys->p_thread->i_samples];

    i_samples = __MIN( 512 - p_sys->p_thread->i_samples,
                       (int)p_out_buf->i_nb_samples );
    p_sys->p_thread->i_samples += i_samples;

    while( i_samples > 0 )
    {
        p_isample[0] = FloatToInt16( p_fsample[0] );
        if( i_channels > 1 )
        {
            p_isample[512] = FloatToInt16( p_fsample[1] );
        }

        p_isample++;
        p_fsample += i_channels;

        i_samples--;
    }
    if( p_sys->p_thread->i_samples == 512 )
    {
        vlc_cond_signal( &p_sys->p_thread->wait );
    }
    vlc_mutex_unlock( &p_sys->p_thread->lock );

}
/*****************************************************************************
 * Thread:
 *****************************************************************************/
static void Thread( vlc_object_t *p_this )
{
    goom_thread_t *p_thread = (goom_thread_t*)p_this;

    goom_init( GOOM_WIDTH, GOOM_HEIGHT, 0 );
    goom_set_font( NULL, NULL, NULL );

    while( !p_thread->b_die )
    {
        mtime_t   i_pts;
        int16_t   data[2][512];
        uint32_t  *plane;
        picture_t *p_pic;

        /* goom_update is damn slow, so just copy data and release the lock */
        vlc_mutex_lock( &p_thread->lock );
        vlc_cond_wait( &p_thread->wait, &p_thread->lock );
        i_pts = p_thread->i_pts;
        memcpy( data, p_thread->samples, 512 * 2 * 2 );
        vlc_mutex_unlock( &p_thread->lock );

        plane = goom_update( data, 0, 0.0, p_thread->psz_title, NULL );

        if( p_thread->psz_title )
        {
            free( p_thread->psz_title );
            p_thread->psz_title = NULL;
        }
        while( ( p_pic = vout_CreatePicture( p_thread->p_vout, 0, 0, 0 ) ) == NULL &&
               !p_thread->b_die )
        {
            msleep( VOUT_OUTMEM_SLEEP );
        }

        if( p_pic == NULL )
        {
            break;
        }

        memcpy( p_pic->p[0].p_pixels, plane, GOOM_WIDTH * GOOM_HEIGHT * 4 );
        vout_DatePicture( p_thread->p_vout, p_pic, i_pts );
        vout_DisplayPicture( p_thread->p_vout, p_pic );

    }

    goom_close();
}


/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    /* Stop Goom Thread */
    p_sys->p_thread->b_die = VLC_TRUE;

    vlc_mutex_lock( &p_sys->p_thread->lock );
    vlc_cond_signal( &p_sys->p_thread->wait );
    vlc_mutex_unlock( &p_sys->p_thread->lock );

    vlc_thread_join( p_sys->p_thread );

    /* Free data */
    vout_Request( p_filter, p_sys->p_thread->p_vout, 0, 0, 0, 0 );
    vlc_mutex_destroy( &p_sys->p_thread->lock );
    vlc_cond_destroy( &p_sys->p_thread->wait );
    vlc_object_destroy( p_sys->p_thread );

    free( p_sys );
}


static char *TitleGet( vlc_object_t *p_this )
{
    input_thread_t *p_input = vlc_object_find( p_this,
                                               VLC_OBJECT_INPUT, FIND_ANYWHERE);
    char           *psz_title = NULL;

    if( p_input )
    {
        char *psz = strrchr( p_input->psz_source, '/' );

        if( psz )
        {
            psz++;
        }
        else
        {
            psz = p_input->psz_source;
        }
        if( psz && *psz )
        {
            psz_title = strdup( psz );
        }
        vlc_object_release( p_input );
    }

    return psz_title;
}

