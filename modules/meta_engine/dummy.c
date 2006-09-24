/*****************************************************************************
 * dummy.c
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#define _GNU_SOURCE
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_meta.h>
#include <vlc_meta_engine.h>
#include <charset.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int FindMeta( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
/*    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );*/
    set_shortname( N_( "Dummy" ) );
    set_description( _("Dummy meta data") );

    set_capability( "meta engine", 1000 );
    set_callbacks( FindMeta, NULL );
vlc_module_end();

/*****************************************************************************
 *****************************************************************************/
static int FindMeta( vlc_object_t *p_this )
{
    meta_engine_t *p_me = (meta_engine_t *)p_this;
    input_item_t *p_item = p_me->p_item;

    if( !p_item->p_meta ) return VLC_EGENERIC;

    uint32_t i_meta = 0;
#define CHECK( a, b ) \
    if( p_item->p_meta->psz_ ## a && *p_item->p_meta->psz_ ## a ) \
        i_meta |= VLC_META_ENGINE_ ## b;

    CHECK( title, TITLE )
    CHECK( author, AUTHOR )
    CHECK( artist, ARTIST )
    CHECK( genre, GENRE )
    CHECK( copyright, COPYRIGHT )
    CHECK( album, COLLECTION )
    CHECK( tracknum, SEQ_NUM )
    CHECK( description, DESCRIPTION )
    CHECK( rating, RATING )
    CHECK( date, DATE )
    CHECK( url, URL )
    CHECK( language, LANGUAGE )
    CHECK( arturl, ART_URL )

    if( !( i_meta & VLC_META_ENGINE_ART_URL )
        && ( p_me->i_mandatory & VLC_META_ENGINE_ART_URL ) )
    {
        if( i_meta & VLC_META_ENGINE_COLLECTION
            && i_meta & VLC_META_ENGINE_ARTIST )
        {
            char *psz_filename;
            struct stat a;
            asprintf( &psz_filename,
                      "file://%s" DIR_SEP CONFIG_DIR DIR_SEP "art"
                      DIR_SEP "%s" DIR_SEP "%s" DIR_SEP "art.jpg", /* ahem ... we can have other filetype too... */
                      p_me->p_libvlc->psz_homedir,
                      p_item->p_meta->psz_artist,
                      p_item->p_meta->psz_album );
            if( utf8_stat( psz_filename+7, &a ) != -1 )
            {
                vlc_meta_SetArtURL( p_item->p_meta, psz_filename );
                i_meta |= VLC_META_ENGINE_ART_URL;
            }
            free( psz_filename );
        }
    }

    /* Add checks for musicbrainz meta */

    if( ( p_me->i_mandatory & i_meta ) == p_me->i_mandatory )
    {
        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
     }
}
