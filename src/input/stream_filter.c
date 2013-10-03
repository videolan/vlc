/*****************************************************************************
 * stream_filter.c
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_modules.h>
#include <libvlc.h>

#include <assert.h>

#include "stream.h"

static void StreamDelete( stream_t * );

stream_t *stream_FilterNew( stream_t *p_source,
                            const char *psz_stream_filter )
{
    stream_t *s;
    assert( p_source != NULL );

    s = stream_CommonNew( VLC_OBJECT( p_source ) );
    if( s == NULL )
        return NULL;

    s->p_input = p_source->p_input;

    /* */
    s->psz_access = strdup( p_source->psz_access );
    s->psz_path = strdup( p_source->psz_path );
    if( !s->psz_path )
    {
        stream_CommonDelete( s );
        return NULL;
    }
    s->p_source = p_source;

    /* */
    s->p_module = module_need( s, "stream_filter", psz_stream_filter, true );

    if( !s->p_module )
    {
        stream_CommonDelete( s );
        return NULL;
    }

    s->pf_destroy = StreamDelete;

    return s;
}

stream_t *stream_FilterChainNew( stream_t *p_source,
                                 const char *psz_chain,
                                 bool b_record )
{
    /* Add auto stream filter */
    for( ;; )
    {
        stream_t *p_filter = stream_FilterNew( p_source, NULL );
        if( !p_filter )
            break;

        msg_Dbg( p_filter, "Inserted a stream filter" );
        p_source = p_filter;
    }

    /* Add user stream filter */
    char *psz_tmp = psz_chain ? strdup( psz_chain ) : NULL;
    char *psz = psz_tmp;
    while( psz && *psz )
    {
        stream_t *p_filter;
        char *psz_end = strchr( psz, ':' );

        if( psz_end )
            *psz_end++ = '\0';

        p_filter = stream_FilterNew( p_source, psz );
        if( p_filter )
            p_source = p_filter;
        else
            msg_Warn( p_source, "failed to insert stream filter %s", psz );

        psz = psz_end;
    }
    free( psz_tmp );

    /* Add record filter if useful */
    if( b_record )
    {
        stream_t *p_filter = stream_FilterNew( p_source, "record" );
        if( p_filter )
            p_source = p_filter;
    }
    return p_source;
}

static void StreamDelete( stream_t *s )
{
    module_unneed( s, s->p_module );

    if( s->p_source )
        stream_Delete( s->p_source );

    stream_CommonDelete( s );
}
