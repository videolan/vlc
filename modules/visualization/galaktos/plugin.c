/*****************************************************************************
 * plugin.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
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
#include "plugin.h"
#include "glx.h"

#include <vlc/input.h>
#include <vlc/vout.h>
#include "aout_internal.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

vlc_module_begin();
    set_description( _("GaLaktos visualization plugin") );
    set_capability( "audio filter", 0 );
    set_callbacks( Open, Close );
    add_shortcut( "galaktos" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct aout_filter_sys_t
{
    galaktos_thread_t *p_thread;

} aout_filter_sys_t;

static void DoWork   ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                       aout_buffer_t * );

static void Thread   ( vlc_object_t * );

static char *TitleGet( vlc_object_t * );

/*****************************************************************************
 * Open: open a scope effect plugin
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys;
    galaktos_thread_t     *p_thread;
    vlc_value_t       width, height;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2' )
         || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        msg_Warn( p_filter, "Bad input or output format" );
        return VLC_EGENERIC;
    }
    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        msg_Warn( p_filter, "input and output formats are not similar" );
        return VLC_EGENERIC;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 1;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( aout_filter_sys_t ) );

    /* Create galaktos thread */
    p_sys->p_thread = p_thread =
        vlc_object_create( p_filter, sizeof( galaktos_thread_t ) );
    vlc_object_attach( p_thread, p_this );

    galaktos_glx_init( p_thread, 512, 512 );
/*
    var_Create( p_thread, "galaktos-width", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_thread, "galaktos-width", &width );
    var_Create( p_thread, "galaktos-height", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_thread, "galaktos-height", &height );
*/
    vlc_mutex_init( p_filter, &p_thread->lock );
    vlc_cond_init( p_filter, &p_thread->wait );

    p_thread->i_blocks = 0;
    aout_DateInit( &p_thread->date, p_filter->output.i_rate );
    aout_DateSet( &p_thread->date, 0 );
    p_thread->i_channels = aout_FormatNbChannels( &p_filter->input );

    p_thread->psz_title = TitleGet( VLC_OBJECT( p_filter ) );

    if( vlc_thread_create( p_thread, "galaktos update thread", Thread,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_filter, "cannot lauch galaktos thread" );
        vout_Destroy( p_thread->p_vout );
        vlc_mutex_destroy( &p_thread->lock );
        vlc_cond_destroy( &p_thread->wait );
        if( p_thread->psz_title ) free( p_thread->psz_title );
        vlc_object_detach( p_thread );
        vlc_object_destroy( p_thread );
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: process samples buffer
 *****************************************************************************
 * This function queues the audio buffer to be processed by the galaktos thread
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    block_t *p_block;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;

    /* Queue sample */
    vlc_mutex_lock( &p_sys->p_thread->lock );
    if( p_sys->p_thread->i_blocks == MAX_BLOCKS )
    {
        vlc_mutex_unlock( &p_sys->p_thread->lock );
        return;
    }

    p_block = block_New( p_sys->p_thread, p_in_buf->i_nb_bytes );
    if( !p_block ) return;
    memcpy( p_block->p_buffer, p_in_buf->p_buffer, p_in_buf->i_nb_bytes );
    p_block->i_pts = p_in_buf->start_date;

    p_sys->p_thread->pp_blocks[p_sys->p_thread->i_blocks++] = p_block;

    vlc_cond_signal( &p_sys->p_thread->wait );
    vlc_mutex_unlock( &p_sys->p_thread->lock );
}

/*****************************************************************************
 * float to s16 conversion
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

/*****************************************************************************
 * Fill buffer
 *****************************************************************************/
static int FillBuffer( int16_t *p_data, int *pi_data,
                       audio_date_t *pi_date, audio_date_t *pi_date_end,
                       galaktos_thread_t *p_this )
{
    int i_samples = 0;
    block_t *p_block;

    while( *pi_data < 512 )
    {
        if( !p_this->i_blocks ) return VLC_EGENERIC;

        p_block = p_this->pp_blocks[0];
        i_samples = __MIN( 512 - *pi_data, p_block->i_buffer /
                           sizeof(float) / p_this->i_channels );

        /* Date management */
        if( p_block->i_pts > 0 &&
            p_block->i_pts != aout_DateGet( pi_date_end ) )
        {
           aout_DateSet( pi_date_end, p_block->i_pts );
        }
        p_block->i_pts = 0;

        aout_DateIncrement( pi_date_end, i_samples );

        while( i_samples > 0 )
        {
            float *p_float = (float *)p_block->p_buffer;

            p_data[*pi_data] = FloatToInt16( p_float[0] );
            if( p_this->i_channels > 1 )
                p_data[512 + *pi_data] = FloatToInt16( p_float[1] );

            (*pi_data)++;
            p_block->p_buffer += (sizeof(float) * p_this->i_channels);
            p_block->i_buffer -= (sizeof(float) * p_this->i_channels);
            i_samples--;
        }

        if( !p_block->i_buffer )
        {
            block_Release( p_block );
            p_this->i_blocks--;
            if( p_this->i_blocks )
                memmove( p_this->pp_blocks, p_this->pp_blocks + 1,
                         p_this->i_blocks * sizeof(block_t *) );
        }
    }

    *pi_date = *pi_date_end;
    *pi_data = 0;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Thread:
 *****************************************************************************/
static void Thread( vlc_object_t *p_this )
{
    galaktos_thread_t *p_thread = (galaktos_thread_t*)p_this;
    vlc_value_t width, height, speed;
    audio_date_t i_pts;
    int16_t p_data[2][512];
    int i_data = 0, i_count = 0;

    while( !p_thread->b_die )
    {
        msleep( VOUT_OUTMEM_SLEEP );
    }
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    /* Stop galaktos Thread */
    p_sys->p_thread->b_die = VLC_TRUE;

    vlc_mutex_lock( &p_sys->p_thread->lock );
    vlc_cond_signal( &p_sys->p_thread->wait );
    vlc_mutex_unlock( &p_sys->p_thread->lock );

    vlc_thread_join( p_sys->p_thread );

    /* Free data */
    vout_Request( p_filter, p_sys->p_thread->p_vout, 0, 0, 0, 0 );
    vlc_mutex_destroy( &p_sys->p_thread->lock );
    vlc_cond_destroy( &p_sys->p_thread->wait );
    vlc_object_detach( p_sys->p_thread );

    while( p_sys->p_thread->i_blocks-- )
    {
        block_Release( p_sys->p_thread->pp_blocks[p_sys->p_thread->i_blocks] );
    }

    vlc_object_destroy( p_sys->p_thread );

    free( p_sys );
}

static char *TitleGet( vlc_object_t *p_this )
{
    char *psz_title = NULL;
    input_thread_t *p_input =
        vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    if( p_input )
    {
        char *psz = strrchr( p_input->input.p_item->psz_uri, '/' );

        if( psz )
        {
            psz++;
        }
        else
        {
            psz = p_input->input.p_item->psz_uri;
        }
        if( psz && *psz )
        {
            psz_title = strdup( psz );
        }
        vlc_object_release( p_input );
    }

    return psz_title;
}
