/*****************************************************************************
 * dummy.c: dummy stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_block.h>
#include <vlc_sout.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );

static void *Add( sout_stream_t *, const es_format_t * );
static void  Del ( sout_stream_t *, void * );
static int   Send( sout_stream_t *, void *, block_t* );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Dummy stream output") )
    set_capability( "sout output", 50 )
    add_shortcut( "dummy", "drop" )
    set_callback( Open )
vlc_module_end ()

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

static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    VLC_UNUSED(p_stream); VLC_UNUSED(p_fmt);
    return malloc( 1 );
}

static void Del( sout_stream_t *p_stream, void *id )
{
    VLC_UNUSED(p_stream);
    free( id );
}

static int Send( sout_stream_t *p_stream, void *id, block_t *p_buffer )
{
    (void)p_stream; (void)id;
    block_ChainRelease( p_buffer );
    return VLC_SUCCESS;
}

