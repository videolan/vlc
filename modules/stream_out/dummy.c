/*****************************************************************************
 * dummy.c: dummy stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
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
    set_description( _("Dummy stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "dummy" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
#if 0
    sout_stream_t   *p_stream = (sout_stream_t*)p_this;
#endif
}

struct sout_stream_id_t
{
    int i_d_u_m_m_y;
};

static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_id_t *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    id->i_d_u_m_m_y = 0;

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
    block_ChainRelease( p_buffer );
    return VLC_SUCCESS;
}

