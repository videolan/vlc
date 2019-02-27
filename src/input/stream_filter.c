/*****************************************************************************
 * stream_filter.c
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
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

struct vlc_stream_filter_private
{
    module_t *module;
};

static void StreamDelete(stream_t *s)
{
    struct vlc_stream_filter_private *priv = vlc_stream_Private(s);

    module_unneed(s, priv->module);
    vlc_stream_Delete(s->s);
    free(s->psz_filepath);
}

stream_t *vlc_stream_FilterNew( stream_t *p_source,
                                const char *psz_stream_filter )
{
    assert(p_source != NULL);

    struct vlc_stream_filter_private *priv;
    stream_t *s = vlc_stream_CustomNew(vlc_object_parent(p_source),
                                       StreamDelete, sizeof (*priv),
                                       "stream filter");
    if( s == NULL )
        return NULL;

    priv = vlc_stream_Private(s);
    s->p_input_item = p_source->p_input_item;

    if( p_source->psz_url != NULL )
    {
        s->psz_url = strdup( p_source->psz_url );
        if( unlikely(s->psz_url == NULL) )
            goto error;

        if( p_source->psz_filepath != NULL )
            s->psz_filepath = strdup( p_source->psz_filepath );
    }
    s->s = p_source;

    /* */
    priv->module = module_need(s, "stream_filter", psz_stream_filter, true);
    if (priv->module == NULL)
        goto error;

    return s;
error:
    free(s->psz_filepath);
    stream_CommonDelete( s );
    return NULL;
}

/* Add automatic stream filter */
stream_t *stream_FilterAutoNew( stream_t *p_source )
{
    /* Limit number of entries to avoid infinite recursion. */
    for( unsigned i = 0; i < 16; i++ )
    {
        stream_t *p_filter = vlc_stream_FilterNew( p_source, NULL );
        if( p_filter == NULL )
            break;

        msg_Dbg( p_filter, "stream filter added to %p", (void *)p_source );
        p_source = p_filter;
    }
    return p_source;
}

/* Add specified stream filter(s) */
stream_t *stream_FilterChainNew( stream_t *p_source, const char *psz_chain )
{
    /* Add user stream filter */
    char *chain = strdup( psz_chain );
    if( unlikely(chain == NULL) )
        return p_source;

    char *buf;
    for( const char *name = strtok_r( chain, ":", &buf );
         name != NULL;
         name = strtok_r( NULL, ":", &buf ) )
    {
        stream_t *p_filter = vlc_stream_FilterNew( p_source, name );
        if( p_filter != NULL )
            p_source = p_filter;
        else
            msg_Warn( p_source, "cannot insert stream filter %s", name );
    }
    free( chain );

    return p_source;
}
