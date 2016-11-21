/*****************************************************************************
 * stream_extractor.c
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#ifndef STREAM_EXTRACTOR_H
#define STREAM_EXTRACTOR_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_stream_extractor.h>
#include <vlc_modules.h>
#include <vlc_url.h>
#include <vlc_memstream.h>
#include <libvlc.h>
#include <assert.h>

#include "stream.h"
#include "mrl_helpers.h"

/**
 * \defgroup stream_extractor_Private Stream Extractor Private
 * \ingroup stream_extractor
 * \internal
 * @{
 * \file
 **/

struct stream_extractor_private {
    stream_extractor_t public;
    stream_t* stream;
    module_t* module;

    vlc_object_t* owner;
};

/**
 * Release the private data associated with a stream-extractor
 *
 * \param priv pointer to the private section
 */
static void se_Release( struct stream_extractor_private* priv )
{
    free( priv->public.identifier );

    if( priv->module )
    {
        module_unneed( &priv->public, priv->module );
        vlc_stream_Delete( priv->public.source );
    }

    vlc_object_release( &priv->public );
}

/**
 * \defgroup stream_extractor_Callbacks Stream Extractor Callbacks
 * \ingroup stream_extractor
 * @{
 *   \file
 *   These functions simply forwards the relevant stream-request to
 *   the underlying stream-extractor. They are a basic form of
 *   type-erasure in that the outside world sees a stream_t, but the
 *   work is actually done by a stream_extractor_t.
 */

static void
se_StreamDelete( stream_t* stream )
{
    se_Release( stream->p_sys );
}

static ssize_t
se_StreamRead( stream_t* stream, void* buf, size_t len )
{
    struct stream_extractor_private* priv = stream->p_sys;
    stream_extractor_t* extractor = &priv->public;
    return extractor->stream.pf_read( extractor, buf, len );
}

static block_t*
se_StreamBlock( stream_t* stream, bool* eof )
{
    struct stream_extractor_private* priv = stream->p_sys;
    stream_extractor_t* extractor = &priv->public;
    return extractor->stream.pf_block( extractor, eof );
}

static int
se_StreamSeek( stream_t* stream, uint64_t offset )
{
    struct stream_extractor_private* priv = stream->p_sys;
    stream_extractor_t* extractor = &priv->public;
    return extractor->stream.pf_seek( extractor, offset );
}

static int
se_StreamReadDir( stream_t* stream, input_item_node_t* node )
{
    struct stream_extractor_private* priv = stream->p_sys;
    stream_extractor_t* extractor = &priv->public;
    return extractor->directory.pf_readdir( extractor, node );
}

static int
se_StreamControl( stream_t* stream, int req, va_list args )
{
    struct stream_extractor_private* priv = stream->p_sys;
    stream_extractor_t* extractor = &priv->public;

    if( extractor->identifier )
        return extractor->stream.pf_control( extractor, req, args );

    if( req == STREAM_IS_DIRECTORY )
    {
        *va_arg( args, bool* ) = true;
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}
/**
 * @}
 **/

/**
 * Initialize the public stream_t for a stream_extractor_t
 *
 * This function simply initializes the relevant data-members of the
 * public stream_t which is a handle to the internal
 * stream_extractor_t.
 *
 * \param obj the private section of the stream_extractor_t
 * \param source the source stream which the stream_extractor_t should
 *        will read from
 * \return VLC_SUCCESS on success, an error-code on failure.
 **/
static int
se_InitStream( struct stream_extractor_private* priv, stream_t* source )
{
    stream_t* s = vlc_stream_CommonNew( priv->public.obj.parent,
                                        se_StreamDelete );
    if( unlikely( !s ) )
        return VLC_EGENERIC;

    if( priv->public.identifier )
    {
        if( priv->public.stream.pf_read ) s->pf_read = se_StreamRead;
        else                              s->pf_block = se_StreamBlock;

        s->pf_seek = se_StreamSeek;
        s->psz_url = vlc_stream_extractor_CreateMRL( &priv->public,
                                                      priv->public.identifier );
    }
    else
    {
        s->pf_readdir = se_StreamReadDir;
        s->psz_url = source->psz_url ? strdup( source->psz_url ) : NULL;
    }


    if( source->psz_url && unlikely( !s->psz_url ) )
    {
        stream_CommonDelete( s );
        return VLC_EGENERIC;
    }

    priv->stream = s;
    priv->stream->pf_control = se_StreamControl;
    priv->stream->p_input = source->p_input;
    priv->stream->p_sys = priv;

    return VLC_SUCCESS;
}

int
vlc_stream_extractor_Attach( stream_t** source, char const* identifier,
                             char const* module_name )
{
    struct stream_extractor_private* priv = vlc_custom_create(
        (*source)->obj.parent, sizeof( *priv ), "stream_extractor" );

    if( unlikely( !priv ) )
        return VLC_ENOMEM;

    priv->public.identifier = identifier ? strdup( identifier ) : NULL;

    if( unlikely( identifier && !priv->public.identifier ) )
        goto error;

    priv->public.source = *source;
    priv->module = module_need( &priv->public, "stream_extractor",
                                module_name, true );

    if( !priv->module || se_InitStream( priv, *source ) )
        goto error;

    *source = priv->stream;
    return VLC_SUCCESS;

error:
    se_Release( priv );
    return VLC_EGENERIC;
}

char*
vlc_stream_extractor_CreateMRL( stream_extractor_t* extractor,
                                char const* subentry )
{
    struct vlc_memstream buffer;
    char* escaped;

    if( mrl_EscapeFragmentIdentifier( &escaped, subentry ) )
        return NULL;

    if( vlc_memstream_open( &buffer ) )
    {
        free( escaped );
        return NULL;
    }

    vlc_memstream_puts( &buffer, extractor->source->psz_url );

    if( !strstr( extractor->source->psz_url, "#" ) )
        vlc_memstream_putc( &buffer, '#' );

    vlc_memstream_printf( &buffer, "!/%s", escaped );

    free( escaped );
    return vlc_memstream_close( &buffer ) ? NULL : buffer.ptr;
}

/**
 * @}
 **/

#endif /* include-guard */
