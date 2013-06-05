/*****************************************************************************
 * chromaprint.c: Chromaprint Fingerprinter Module
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
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
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_block.h>
#include <vlc_sout.h>

#include <assert.h>

#ifdef _WIN32
# define CHROMAPRINT_NODLL
#endif

#include <chromaprint.h> /* chromaprint lib */
#include "chromaprint_data.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DURATION_TEXT N_("Duration of the fingerprinting" )
#define DURATION_LONGTEXT N_("Default: 90sec")

vlc_module_begin ()
    set_description( N_("Chromaprint stream output") )
    set_capability( "sout stream", 0 )
    add_shortcut( "chromaprint" )
    add_integer( "duration", 90, DURATION_TEXT, DURATION_LONGTEXT, true )
    set_callbacks( Open, Close )
vlc_module_end ()

struct sout_stream_sys_t
{
    unsigned int i_duration;
    unsigned int i_total_samples;
    int i_samples;
    bool b_finished;
    bool b_done;
    ChromaprintContext *p_chromaprint_ctx;
    sout_stream_id_t *id;
    chromaprint_fingerprint_t *p_data;
};

struct sout_stream_id_t
{
    int i_samples;
    unsigned int i_channels;
    unsigned int i_samplerate;
};

#define BYTESPERSAMPLE 2

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_stream->p_sys = p_sys = malloc(sizeof(sout_stream_sys_t));
    if ( unlikely( ! p_sys ) ) return VLC_ENOMEM;
    p_sys->id = NULL;
    p_sys->b_finished = false;
    p_sys->b_done = false;
    p_sys->i_total_samples = 0;
    p_sys->i_duration = var_InheritInteger( p_stream, "duration" );
    p_sys->p_data = var_InheritAddress( p_stream, "fingerprint-data" );
    if ( !p_sys->p_data )
    {
        msg_Err( p_stream, "Fingerprint data holder not set" );
        free( p_sys );
        return VLC_ENOVAR;
    }
    msg_Dbg( p_stream, "chromaprint version %s", chromaprint_get_version() );
    p_sys->p_chromaprint_ctx = chromaprint_new( CHROMAPRINT_ALGORITHM_DEFAULT );
    if ( ! p_sys->p_chromaprint_ctx )
    {
        msg_Err( p_stream, "Can't create chromaprint context" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_stream->pf_add  = Add;
    p_stream->pf_del  = Del;
    p_stream->pf_send = Send;
    return VLC_SUCCESS;
}

static void Finish( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_fingerprint = NULL;
    if ( p_sys->b_finished && chromaprint_finish( p_sys->p_chromaprint_ctx ) )
    {
        chromaprint_get_fingerprint( p_sys->p_chromaprint_ctx,
                                     &psz_fingerprint );
        if ( psz_fingerprint )
        {
            p_sys->p_data->i_duration = p_sys->i_total_samples / p_sys->id->i_samplerate;
            p_sys->p_data->psz_fingerprint = strdup( psz_fingerprint );
            chromaprint_dealloc( psz_fingerprint );
            msg_Dbg( p_stream, "DURATION=%u;FINGERPRINT=%s",
                    p_sys->p_data->i_duration,
                    p_sys->p_data->psz_fingerprint );
        }
    } else {
        msg_Dbg( p_stream, "Cannot create %us fingerprint (not enough samples?)",
                 p_sys->i_duration );
    }
    p_sys->b_done = true;
    msg_Dbg( p_stream, "Fingerprinting finished" );
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if ( !p_sys->b_done ) Finish( p_stream );
    chromaprint_free( p_sys->p_chromaprint_ctx );
    free( p_sys );
}

static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id = NULL;

    if ( p_fmt->i_cat == AUDIO_ES && !p_sys->id )
    {
        if( p_fmt->i_codec != VLC_CODEC_S16N || p_fmt->audio.i_channels > 2 )
        {
            msg_Warn( p_stream, "bad input format: need s16l, 1 or 2 channels" );
            goto error;
        }

        id = malloc( sizeof( sout_stream_id_t ) );
        if ( !id ) goto error;

        id->i_channels = p_fmt->audio.i_channels;
        id->i_samplerate = p_fmt->audio.i_rate;
        id->i_samples = p_sys->i_duration * id->i_samplerate;

        if ( !chromaprint_start( p_sys->p_chromaprint_ctx, p_fmt->audio.i_rate, id->i_channels ) )
        {
            msg_Err( p_stream, "Failed starting chromaprint on %uHz %uch samples",
                     p_fmt->audio.i_rate, id->i_channels );
            goto error;
        }
        else
        {
            p_sys->id = id;
            msg_Dbg( p_stream, "Starting chromaprint on %uHz %uch samples",
                     p_fmt->audio.i_rate, id->i_channels );
        }
        return id;
    }

error:
    free( id );
    return NULL;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    Finish( p_stream );
    if ( p_sys->id == id ) /* not assuming only 1 id is in use.. */
        p_sys->id = NULL;
    free( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buf )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if ( p_sys->id != id )
    {
        /* drop the whole buffer at once */
        block_ChainRelease( p_buf );
        return VLC_SUCCESS;
    }

    while( p_buf )
    {
        block_t *p_next;
        int i_samples = p_buf->i_buffer / (BYTESPERSAMPLE * id->i_channels);
        p_sys->i_total_samples += i_samples;
        if ( !p_sys->b_finished && id->i_samples > 0 && p_buf->i_buffer )
        {
            if(! chromaprint_feed( p_sys->p_chromaprint_ctx,
                                   p_buf->p_buffer,
                                   p_buf->i_buffer / BYTESPERSAMPLE ) )
                msg_Warn( p_stream, "feed error" );
            id->i_samples -= i_samples;
            if ( id->i_samples < 1 && !p_sys->b_finished )
            {
                p_sys->b_finished = true;
                msg_Dbg( p_stream, "Fingerprint collection finished" );
            }
        }
        p_next = p_buf->p_next;
        block_Release( p_buf );
        p_buf = p_next;
    }

    return VLC_SUCCESS;
}
