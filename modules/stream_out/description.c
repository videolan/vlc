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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
vlc_module_begin ()
    set_description( N_("Description stream output") )
    set_capability( "sout stream", 50 )
    add_shortcut( "description" )
    set_callbacks( Open, Close )
vlc_module_end ()

struct sout_stream_sys_t
{
    sout_description_data_t *data;
    mtime_t i_stream_start;
};

struct sout_stream_id_t
{
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

    p_sys->data = var_InheritAddress(p_stream, "sout-description-data");
    if (p_sys->data == NULL)
    {
        msg_Err(p_stream, "Missing data: the description stream output is "
                "not meant to be used without special setup from the core");
        free(p_sys);
        return VLC_EGENERIC;
    }
    p_sys->i_stream_start = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    msg_Dbg( p_this, "Closing" );

    free( p_sys );
}

static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id;
    es_format_t *p_fmt_copy;

    msg_Dbg( p_stream, "Adding a stream" );
 
    p_fmt_copy = malloc(sizeof(es_format_t));
    es_format_Copy( p_fmt_copy, p_fmt );

    TAB_APPEND( p_sys->data->i_es, p_sys->data->es, p_fmt_copy );

    if( p_sys->i_stream_start <= 0 )
        p_sys->i_stream_start = mdate();

    id = malloc( sizeof( sout_stream_id_t ) );
    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    msg_Dbg( p_stream, "Removing a stream" );

    free( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    VLC_UNUSED(id);
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    block_ChainRelease( p_buffer );

    if( p_sys->i_stream_start + 1500000 < mdate() )
        vlc_sem_post(p_sys->data->sem);

    return VLC_SUCCESS;
}
