/*****************************************************************************
 * display.c: display stream output module
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define AUDIO_TEXT N_("Enable audio")
#define AUDIO_LONGTEXT N_( "Enable/disable audio rendering." )
#define VIDEO_TEXT N_("Enable video")
#define VIDEO_LONGTEXT N_( "Enable/disable video rendering." )
#define DELAY_TEXT N_("Delay")
#define DELAY_LONGTEXT N_( "Introduces a delay in the display of the stream." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-display-"

vlc_module_begin();
    set_shortname( _("Display"));
    set_description( _("Display stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "display" );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_STREAM );
    add_bool( SOUT_CFG_PREFIX "audio", 1, NULL, AUDIO_TEXT,
              AUDIO_LONGTEXT, VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "video", 1, NULL, VIDEO_TEXT,
              VIDEO_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "delay", 100, NULL, DELAY_TEXT,
                 DELAY_LONGTEXT, VLC_TRUE );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "audio", "video", "delay", NULL
};

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

struct sout_stream_sys_t
{
    input_thread_t *p_input;

    vlc_bool_t     b_audio;
    vlc_bool_t     b_video;

    mtime_t        i_delay;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    vlc_value_t val;

    sout_CfgParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    p_sys          = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->p_input = vlc_object_find( p_stream, VLC_OBJECT_INPUT,
                                      FIND_PARENT );
    if( !p_sys->p_input )
    {
        msg_Err( p_stream, "cannot find p_input" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "audio", &val );
    p_sys->b_audio = val.b_bool;

    var_Get( p_stream, SOUT_CFG_PREFIX "video", &val );
    p_sys->b_video = val.b_bool;

    var_Get( p_stream, SOUT_CFG_PREFIX "delay", &val );
    p_sys->i_delay = (int64_t)val.i_int * 1000;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol--;

    vlc_object_release( p_sys->p_input );
    free( p_sys );
}

struct sout_stream_id_t
{
    decoder_t *p_dec;
};

static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id;

    if( ( p_fmt->i_cat == AUDIO_ES && !p_sys->b_audio )||
        ( p_fmt->i_cat == VIDEO_ES && !p_sys->b_video ) )
    {
        return NULL;
    }

    id = malloc( sizeof( sout_stream_id_t ) );

    id->p_dec = input_DecoderNew( p_sys->p_input, p_fmt, VLC_TRUE );
    if( id->p_dec == NULL )
    {
        msg_Err( p_stream, "cannot create decoder for fcc=`%4.4s'",
                 (char*)&p_fmt->i_codec );
        free( id );
        return NULL;
    }

    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    input_DecoderDelete( id->p_dec );
    free( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    while( p_buffer )
    {
        block_t *p_next = p_buffer->p_next;

        p_buffer->p_next = NULL;

        if( id->p_dec && p_buffer->i_buffer > 0 )
        {
            if( p_buffer->i_dts <= 0 )
                p_buffer->i_dts= 0;
            else
                p_buffer->i_dts += p_sys->i_delay;

            if( p_buffer->i_pts <= 0 )
                p_buffer->i_pts= 0;
            else
                p_buffer->i_pts += p_sys->i_delay;

            input_DecoderDecode( id->p_dec, p_buffer );
        }

        p_buffer = p_next;
    }

    return VLC_SUCCESS;
}
