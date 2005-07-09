/*****************************************************************************
 * description.c: description stream output module (gathers ES info)
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
vlc_module_begin();
    set_description( _("Description stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "description" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    input_thread_t *p_input;
    mtime_t i_stream_start;
};

struct sout_stream_id_t
{
    int i_d_u_m_m_y;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_stream->pf_add  = Add;
    p_stream->pf_del  = Del;
    p_stream->pf_send = Send;
    p_sys = p_stream->p_sys = malloc(sizeof(sout_stream_sys_t));

    p_sys->p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_PARENT );
    if( !p_sys->p_input ) return VLC_EGENERIC;

    p_sys->i_stream_start = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t *)p_this;
    vlc_object_release( p_stream->p_sys->p_input );
}

static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id;
    es_format_t *p_fmt_copy = malloc(sizeof(es_format_t));

    id = malloc( sizeof( sout_stream_id_t ) );
    id->i_d_u_m_m_y = 0;

    es_format_Copy( p_fmt_copy, p_fmt );

    vlc_mutex_lock( &p_sys->p_input->input.p_item->lock );
    TAB_APPEND( p_sys->p_input->input.p_item->i_es,
                p_sys->p_input->input.p_item->es, p_fmt_copy );
    vlc_mutex_unlock( &p_sys->p_input->input.p_item->lock );

    if( p_sys->i_stream_start <= 0 ) p_sys->i_stream_start = mdate();

    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    free( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    block_ChainRelease( p_buffer );

    if( p_sys->i_stream_start + 1500000 < mdate() )
    {
        p_sys->p_input->b_eof = VLC_TRUE;
    }

    return VLC_SUCCESS;
}
