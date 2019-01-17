/*****************************************************************************
 * media_list_path.h : Some inlined function that allows media_list_path
 * manipulation. This is internal and used only by media_list_player.
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#ifndef _LIBVLC_MEDIA_LIST_PATH_H
#define _LIBVLC_MEDIA_LIST_PATH_H 1

typedef int * libvlc_media_list_path_t; /* (Media List Player Internal) */

/**************************************************************************
 *       path_dump (Media List Player Internal)
 **************************************************************************/
static inline void libvlc_media_list_path_dump( const libvlc_media_list_path_t path )
{
    if(!path)
    {
        printf("NULL path\n");
        return;
    }

    for(int i = 0; path[i] != -1; i++)
        printf("%s%d", i > 0 ? "/" : "", path[i]);
    printf("\n");
}

/**************************************************************************
 *       path_empty (Media List Player Internal)
 **************************************************************************/
static inline libvlc_media_list_path_t libvlc_media_list_path_empty( void )
{
    libvlc_media_list_path_t ret = xmalloc(sizeof(int));
    ret[0] = -1;
    return ret;
}

/**************************************************************************
 *       path_with_root_index (Media List Player Internal)
 **************************************************************************/
static inline libvlc_media_list_path_t libvlc_media_list_path_with_root_index( int index )
{
    libvlc_media_list_path_t ret = xmalloc(sizeof(int)*2);
    ret[0] = index;
    ret[1] = -1;
    return ret;
}

/**************************************************************************
 *       path_depth (Media List Player Internal)
 **************************************************************************/
static inline int libvlc_media_list_path_depth( const libvlc_media_list_path_t path )
{
    int i;
    for( i = 0; path[i] != -1; i++ );
    return i;
}

/**************************************************************************
 *       path_append (Media List Player Internal)
 **************************************************************************/
static inline void libvlc_media_list_path_append( libvlc_media_list_path_t * p_path, int index )
{
    int old_depth = libvlc_media_list_path_depth( *p_path );
    *p_path = xrealloc( *p_path, sizeof(int)*(old_depth+2));
    *p_path[old_depth] = index;
    *p_path[old_depth+1] = -1;
}

/**************************************************************************
 *       path_copy_by_appending (Media List Player Internal)
 **************************************************************************/
static inline libvlc_media_list_path_t libvlc_media_list_path_copy_by_appending( const libvlc_media_list_path_t path, int index )
{
    libvlc_media_list_path_t ret;
    int old_depth = libvlc_media_list_path_depth( path );
    ret = xmalloc( sizeof(int) * (old_depth + 2) );
    memcpy( ret, path, sizeof(int) * old_depth );
    ret[old_depth] = index;
    ret[old_depth+1] = -1;
    return ret;
}

/**************************************************************************
 *       path_copy (Media List Player Internal)
 **************************************************************************/
static inline libvlc_media_list_path_t libvlc_media_list_path_copy( const libvlc_media_list_path_t path )
{
    libvlc_media_list_path_t ret;
    int depth = libvlc_media_list_path_depth( path );
    ret = xmalloc( sizeof(int)*(depth+1) );
    memcpy( ret, path, sizeof(int)*(depth+1) );
    return ret;
}

/**************************************************************************
 *       get_path_rec (Media List Player Internal)
 **************************************************************************/
static libvlc_media_list_path_t
get_path_rec( const libvlc_media_list_path_t path, libvlc_media_list_t * p_current_mlist, libvlc_media_t * p_searched_md )
{
    int count = libvlc_media_list_count( p_current_mlist );

    for( int i = 0; i < count; i++ )
    {
        libvlc_media_t * p_md = libvlc_media_list_item_at_index( p_current_mlist, i );

        if( p_md == p_searched_md )
            return libvlc_media_list_path_copy_by_appending( path, i ); /* Found! */

        libvlc_media_list_t * p_subitems = libvlc_media_subitems( p_md );
        libvlc_media_release( p_md );
        if( p_subitems )
        {
            libvlc_media_list_path_t new_path = libvlc_media_list_path_copy_by_appending( path, i );
            libvlc_media_list_lock( p_subitems );
            libvlc_media_list_path_t ret = get_path_rec( new_path, p_subitems, p_searched_md );
            libvlc_media_list_unlock( p_subitems );
            free( new_path );
            libvlc_media_list_release( p_subitems );
            if( ret )
                return ret; /* Found in sublist! */
        }
    }
    return NULL;
}

/**************************************************************************
 *       path_of_item (Media List Player Internal)
 **************************************************************************/
static inline libvlc_media_list_path_t libvlc_media_list_path_of_item( libvlc_media_list_t * p_mlist, libvlc_media_t * p_md )
{
    libvlc_media_list_path_t path = libvlc_media_list_path_empty();
    libvlc_media_list_path_t ret;
    ret = get_path_rec( path, p_mlist, p_md );
    free( path );
    return ret;
}

/**************************************************************************
 *       item_at_path (Media List Player Internal)
 **************************************************************************/
static libvlc_media_t *
libvlc_media_list_item_at_path( libvlc_media_list_t * p_mlist, const libvlc_media_list_path_t path )
{
    libvlc_media_list_t * p_current_mlist = p_mlist;

    for( int i = 0; path[i] != -1; i++ )
    {
        libvlc_media_t* p_md = libvlc_media_list_item_at_index( p_current_mlist, path[i] );

        if( p_current_mlist != p_mlist )
            libvlc_media_list_release( p_current_mlist );

        if( path[i+1] == -1 )
            return p_md;

        p_current_mlist = libvlc_media_subitems( p_md );
        libvlc_media_release( p_md );

        if( !p_current_mlist )
            return NULL;

        /* Fetch next one */
    }
    /* Not found, shouldn't happen if the p_path is not empty */
    if( p_current_mlist != p_mlist )
        libvlc_media_list_release( p_current_mlist );
    return NULL;
}

/**************************************************************************
 *       parentlist_at_path (Media List Player Internal)
 **************************************************************************/
static libvlc_media_list_t *
libvlc_media_list_parentlist_at_path( libvlc_media_list_t * p_mlist, const libvlc_media_list_path_t path )
{
    libvlc_media_list_t * p_current_mlist = p_mlist;

    for( int i = 0; path[i] != -1; i++ )
    {
        if( p_current_mlist != p_mlist )
            libvlc_media_list_release( p_current_mlist );

        if( path[i+1] == -1 )
        {
            libvlc_media_list_retain(p_current_mlist);
            return p_current_mlist;
        }

        libvlc_media_t* p_md = libvlc_media_list_item_at_index( p_current_mlist, path[i] );

        p_current_mlist = libvlc_media_subitems( p_md );
        libvlc_media_release( p_md );

        if( !p_current_mlist )
            return NULL;

        /* Fetch next one */
    }
    /* Not found, shouldn't happen if the p_path is not empty */
    if( p_current_mlist != p_mlist )
        libvlc_media_list_release( p_current_mlist );
    return NULL;
}

/**************************************************************************
 *       sublist_at_path (Media List Player Internal)
 **************************************************************************/
static libvlc_media_list_t *
libvlc_media_list_sublist_at_path( libvlc_media_list_t * p_mlist, const libvlc_media_list_path_t path )
{
    libvlc_media_list_t * ret;
    libvlc_media_t * p_md = libvlc_media_list_item_at_path( p_mlist, path );
    if( !p_md )
        return NULL;

    ret = libvlc_media_subitems( p_md );
    libvlc_media_release( p_md );

    return ret;
}

#endif
