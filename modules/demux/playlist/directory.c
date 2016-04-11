/*****************************************************************************
 * directory.c : Use access readdir to output folder content to playlist
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Julien 'Lta' BALLET <contact # lta . io >
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
#include <vlc_demux.h>

#include "playlist.h"

struct demux_sys_t
{
    bool b_dir_sorted;
    bool b_dir_can_loop;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux );


int Import_Dir ( vlc_object_t *p_this)
{
    demux_t  *p_demux = (demux_t *)p_this;
    bool b_dir_sorted, b_dir_can_loop;

    if( stream_Control( p_demux->s, STREAM_IS_DIRECTORY,
                        &b_dir_sorted, &b_dir_can_loop ) )
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "reading directory content" );
    p_demux->p_sys->b_dir_sorted = b_dir_sorted;
    p_demux->p_sys->b_dir_can_loop = b_dir_can_loop;

    return VLC_SUCCESS;
}

void Close_Dir ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys );
}

/**
 * Does the provided URI/path/stuff has one of the extension provided ?
 *
 * \param psz_exts A comma separated list of extension without dot, or only
 * one ext (ex: "avi,mkv,webm")
 * \param psz_uri The uri/path to check (ex: "file:///home/foo/bar.avi"). If
 * providing an URI, it must not contain a query string.
 *
 * \return true if the uri/path has one of the provided extension
 * false otherwise.
 */
static bool has_ext( const char *psz_exts, const char *psz_uri )
{
    if( psz_exts == NULL )
        return false;

    const char *ext = strrchr( psz_uri, '.' );
    if( ext == NULL )
        return false;

    size_t extlen = strlen( ++ext );

    for( const char *type = psz_exts, *end; type[0]; type = end + 1 )
    {
        end = strchr( type, ',' );
        if( end == NULL )
            end = type + strlen( type );

        if( type + extlen == end && !strncasecmp( ext, type, extlen ) )
            return true;

        if( *end == '\0' )
            break;
    }

    return false;
}

static int compar_type( input_item_t *p1, input_item_t *p2 )
{
    if( p1->i_type != p2->i_type )
    {
        if( p1->i_type == ITEM_TYPE_DIRECTORY )
            return -1;
        if( p2->i_type == ITEM_TYPE_DIRECTORY )
            return 1;
    }
    return 0;
}

static int compar_collate( input_item_t *p1, input_item_t *p2 )
{
    int i_ret = compar_type( p1, p2 );

    if( i_ret != 0 )
        return i_ret;

#ifdef HAVE_STRCOLL
    /* The program's LOCAL defines if case is ignored */
    return strcoll( p1->psz_name, p2->psz_name );
#else
    return strcasecmp( p1->psz_name, p2->psz_name );
#endif
}

static int compar_version( input_item_t *p1, input_item_t *p2 )
{
    int i_ret = compar_type( p1, p2 );

    if( i_ret != 0 )
        return i_ret;

    return strverscmp( p1->psz_name, p2->psz_name );
}

static int Demux( demux_t *p_demux )
{
    int i_ret = VLC_SUCCESS;
    input_item_t *p_input;
    input_item_node_t *p_node;
    input_item_t *p_item;
    char *psz_ignored_exts;
    bool b_show_hiddenfiles;

    p_input = GetCurrentItem( p_demux );
    p_node = input_item_node_Create( p_input );
    if( p_node == NULL )
    {
        input_item_Release( p_input );
        return VLC_ENOMEM;
    }
    p_node->b_can_loop = p_demux->p_sys->b_dir_can_loop;
    input_item_Release(p_input);

    b_show_hiddenfiles = var_InheritBool( p_demux, "show-hiddenfiles" );
    psz_ignored_exts = var_InheritString( p_demux, "ignore-filetypes" );

    while( !i_ret && ( p_item = stream_ReadDir( p_demux->s ) ) )
    {
        int i_name_len = p_item->psz_name ? strlen( p_item->psz_name ) : 0;

        /* skip null, "." and ".." and hidden files if option is activated */
        if( !i_name_len || strcmp( p_item->psz_name, "." ) == 0
         || strcmp( p_item->psz_name, ".." ) == 0
         || ( !b_show_hiddenfiles && p_item->psz_name[0] == '.' ) )
            goto skip_item;
        /* skip ignored files */
        if( has_ext( psz_ignored_exts, p_item->psz_name ) )
            goto skip_item;

        input_item_CopyOptions( p_item, p_node->p_item );
        if( !input_item_node_AppendItem( p_node, p_item ) )
            i_ret = VLC_ENOMEM;
skip_item:
        input_item_Release( p_item );
    }
    free( psz_ignored_exts );

    if( i_ret )
    {
        msg_Warn( p_demux, "unable to read directory" );
        input_item_node_Delete( p_node );
        return i_ret;
    }

    if( !p_demux->p_sys->b_dir_sorted )
    {
        input_item_compar_cb compar_cb = NULL;
        char *psz_sort = var_InheritString( p_demux, "directory-sort" );

        if( psz_sort )
        {
            if( !strcasecmp( psz_sort, "version" ) )
                compar_cb = compar_version;
            else if( strcasecmp( psz_sort, "none" ) )
                compar_cb = compar_collate;
        }
        free( psz_sort );
        if( compar_cb )
            input_item_node_Sort( p_node, compar_cb );
    }

    input_item_node_PostAndDelete( p_node );
    return VLC_SUCCESS;
}
