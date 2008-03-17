/*****************************************************************************
 * tag_query.c: libvlc new API media tag query functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include "vlc_arrays.h"

/* XXX This API is in construction
 *
 * It's goal is to represent a meta tag query
 * It should be also able to say if a query can be matched in a media
 * descriptor through libvlc_query_match.
 */
 
/*
 * Public libvlc functions
 */

/**************************************************************************
 *       new (Public)
 *
 * Init an object.
 **************************************************************************/
libvlc_tag_query_t *
libvlc_tag_query_new( libvlc_instance_t * p_inst,
                      libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_tag_query_t * p_q;

    p_q = malloc(sizeof(libvlc_tag_query_t));

    if( !p_q )
        return NULL;
    
    p_q->p_libvlc_instance = p_inst;
    p_q->i_refcount = 1;
    p_q->tag = NULL;
    p_q->psz_tag_key = NULL;

    libvlc_retain( p_inst );
    return p_q;
}

/**************************************************************************
 *       release (Public)
 *
 * Release an object.
 **************************************************************************/
void libvlc_tag_query_release( libvlc_tag_query_t * p_q )
{
    p_q->i_refcount--;

    if( p_q->i_refcount > 0 )
        return;

    free( p_q->tag );
    free( p_q->psz_tag_key );
 
    libvlc_release( p_q->p_libvlc_instance );
    free( p_q );
}

/**************************************************************************
 *       retain (Public)
 *
 * Release an object.
 **************************************************************************/
void libvlc_tag_query_retain( libvlc_tag_query_t * p_q )
{
    p_q->i_refcount++;
}

/**************************************************************************
 *       set_match_tag_and_key (Public)
 **************************************************************************/
void libvlc_tag_query_set_match_tag_and_key( libvlc_tag_query_t * p_q,
                                             libvlc_tag_t tag,
                                             char * psz_tag_key,
                                             libvlc_exception_t * p_e )
{
    (void)p_e;

    p_q->tag = strdup( tag );
    p_q->psz_tag_key = strdup( psz_tag_key );
}

/**************************************************************************
 *       match (Public)
 *
 * Return true if the query p_q is matched in p_md
 **************************************************************************/
int
libvlc_tag_query_match( libvlc_tag_query_t * p_q,
                        libvlc_media_descriptor_t * p_md,
                        libvlc_exception_t * p_e )
{
    int i;
    struct libvlc_tags_storage_t * p_ts;
    (void)p_e;
 
    if( !p_q->psz_tag_key )
        return 1;

    p_ts = vlc_dictionary_value_for_key( &p_md->tags, p_q->psz_tag_key );
    if( !p_q->tag )
        return p_ts->i_count > 0;

    for( i = 0; i < p_ts->i_count; i++ )
    {
        if( !strcmp( p_ts->ppsz_tags[i], p_q->tag ) )
            return 1;
    }

    /* In construction... */
    return 0;
}
