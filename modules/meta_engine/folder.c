/*****************************************************************************
 * folder.c
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

#ifndef MAX_PATH
#   define MAX_PATH 250
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
    set_shortname( N_( "Folder" ) );
    set_description( _("Folder meta data") );

    set_capability( "art finder", 10 );
    set_callbacks( FindMeta, NULL );
vlc_module_end();

/*****************************************************************************
 *****************************************************************************/
static int FindMeta( vlc_object_t *p_this )
{
    meta_engine_t *p_me = (meta_engine_t *)p_this;
    input_item_t *p_item = p_me->p_item;
    vlc_bool_t b_have_art = VLC_FALSE;

    if( !p_item->p_meta ) return VLC_EGENERIC;


    if( p_me->i_mandatory & VLC_META_ENGINE_ART_URL
        || p_me->i_optional & VLC_META_ENGINE_ART_URL )
    {
        int i = 0;
        struct stat a;
        char psz_filename[MAX_PATH];
        char *psz_dir = strdup( p_item->psz_uri );
        char *psz_buf = strrchr( psz_dir, '/' );

        if( psz_buf )
        {
            psz_buf++;
            *psz_buf = '\0';
        }
        else
        {
            *psz_dir = '\0';
        }

        for( i = 0; b_have_art == VLC_FALSE && i < 3; i++ )
        {
            switch( i )
            {
                case 0:
                /* Windows Folder.jpg */
                snprintf( psz_filename, MAX_PATH,
                          "file://%sFolder.jpg", psz_dir );
                break;

                case 1:
                /* Windows AlbumArtSmall.jpg == small version of Folder.jpg */
                snprintf( psz_filename, MAX_PATH,
                      "file://%sAlbumArtSmall.jpg", psz_dir );
                break;

                case 2:
                /* KDE (?) .folder.png */
                snprintf( psz_filename, MAX_PATH,
                      "file://%s.folder.png", psz_dir );
                break;
            }

            if( utf8_stat( psz_filename+7, &a ) != -1 )
            {
                vlc_meta_SetArtURL( p_item->p_meta, psz_filename );
                b_have_art = VLC_TRUE;
            }
        }

        free( psz_dir );
    }

    return VLC_SUCCESS;
}
