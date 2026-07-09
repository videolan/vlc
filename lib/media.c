/*****************************************************************************
 * media.c: Libvlc API media descriptor management
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont@videolan.org>
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

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h> // For the subitems, here for convenience

#include <vlc_common.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_atomic.h>

#include "../src/libvlc.h"

#include "libvlc_internal.h"
#include "media_internal.h"
#include "media_list_internal.h"
#include "picture_internal.h"

static const vlc_meta_type_t libvlc_to_vlc_meta[] =
{
    [libvlc_meta_Title]        = vlc_meta_Title,
    [libvlc_meta_Artist]       = vlc_meta_Artist,
    [libvlc_meta_Genre]        = vlc_meta_Genre,
    [libvlc_meta_Copyright]    = vlc_meta_Copyright,
    [libvlc_meta_Album]        = vlc_meta_Album,
    [libvlc_meta_TrackNumber]  = vlc_meta_TrackNumber,
    [libvlc_meta_Description]  = vlc_meta_Description,
    [libvlc_meta_Rating]       = vlc_meta_Rating,
    [libvlc_meta_Date]         = vlc_meta_Date,
    [libvlc_meta_Setting]      = vlc_meta_Setting,
    [libvlc_meta_URL]          = vlc_meta_URL,
    [libvlc_meta_Language]     = vlc_meta_Language,
    [libvlc_meta_NowPlaying]   = vlc_meta_NowPlaying,
    [libvlc_meta_Publisher]    = vlc_meta_Publisher,
    [libvlc_meta_EncodedBy]    = vlc_meta_EncodedBy,
    [libvlc_meta_ArtworkURL]   = vlc_meta_ArtworkURL,
    [libvlc_meta_TrackID]      = vlc_meta_TrackID,
    [libvlc_meta_TrackTotal]   = vlc_meta_TrackTotal,
    [libvlc_meta_Director]     = vlc_meta_Director,
    [libvlc_meta_Season]       = vlc_meta_Season,
    [libvlc_meta_Episode]      = vlc_meta_Episode,
    [libvlc_meta_ShowName]     = vlc_meta_ShowName,
    [libvlc_meta_Actors]       = vlc_meta_Actors,
    [libvlc_meta_AlbumArtist]  = vlc_meta_AlbumArtist,
    [libvlc_meta_DiscNumber]   = vlc_meta_DiscNumber,
    [libvlc_meta_DiscTotal]    = vlc_meta_DiscTotal
};

static_assert(
    ORIENT_TOP_LEFT     == (int) libvlc_video_orient_top_left &&
    ORIENT_TOP_RIGHT    == (int) libvlc_video_orient_top_right &&
    ORIENT_BOTTOM_LEFT  == (int) libvlc_video_orient_bottom_left &&
    ORIENT_BOTTOM_RIGHT == (int) libvlc_video_orient_bottom_right &&
    ORIENT_LEFT_TOP     == (int) libvlc_video_orient_left_top &&
    ORIENT_LEFT_BOTTOM  == (int) libvlc_video_orient_left_bottom &&
    ORIENT_RIGHT_TOP    == (int) libvlc_video_orient_right_top &&
    ORIENT_RIGHT_BOTTOM == (int) libvlc_video_orient_right_bottom,
    "Mismatch between libvlc_video_orient_t and video_orientation_t" );

static_assert(
    PROJECTION_MODE_RECTANGULAR             == (int) libvlc_video_projection_rectangular &&
    PROJECTION_MODE_EQUIRECTANGULAR         == (int) libvlc_video_projection_equirectangular &&
    PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD == (int) libvlc_video_projection_cubemap_layout_standard,
    "Mismatch between libvlc_video_projection_t and video_projection_mode_t" );

static_assert(
    MULTIVIEW_2D                    == (int) libvlc_video_multiview_2d &&
    MULTIVIEW_STEREO_SBS            == (int) libvlc_video_multiview_stereo_sbs &&
    MULTIVIEW_STEREO_TB             == (int) libvlc_video_multiview_stereo_tb &&
    MULTIVIEW_STEREO_ROW            == (int) libvlc_video_multiview_stereo_row &&
    MULTIVIEW_STEREO_COL            == (int) libvlc_video_multiview_stereo_col &&
    MULTIVIEW_STEREO_FRAME          == (int) libvlc_video_multiview_stereo_frame &&
    MULTIVIEW_STEREO_CHECKERBOARD   == (int) libvlc_video_multiview_stereo_checkerboard,
    "Mismatch between libvlc_video_multiview_t and video_multiview_mode_t");

static libvlc_media_t *input_item_add_subitem( libvlc_media_t *p_md,
                                               input_item_t *item )
{
    libvlc_media_t * p_md_child;
    libvlc_media_list_t *p_subitems;

    p_md_child = libvlc_media_new_from_input_item( item );

    /* Add this to our media list */
    p_subitems = p_md->p_subitems;
    libvlc_media_list_internal_add_media( p_subitems, p_md_child );

    return p_md_child;
}

struct vlc_item_list
{
    struct vlc_list node;
    const input_item_node_t *item;
    libvlc_media_t *media;
};

static struct vlc_item_list *
wrap_item_in_list( libvlc_media_t *media, const input_item_node_t *item )
{
    struct vlc_item_list *node = malloc( sizeof *node );
    if( node == NULL )
        return NULL;
    node->item = item;
    node->media = media;
    return node;
}

static void input_item_add_subnode( libvlc_media_t *md,
                                    const input_item_node_t *root )
{
    struct vlc_list list;
    vlc_list_init( &list );

    /* Retain the media because we don't want the search algorithm to release
     * it when its subitems get parsed. */
    libvlc_media_retain(md);
    libvlc_media_list_lock(md->p_subitems);
    libvlc_media_list_internal_clear(md->p_subitems);

    struct vlc_item_list *node_root = wrap_item_in_list( md, root );
    if( node_root == NULL )
    {
        libvlc_media_release(md);
        goto error;
    }

    /* This is a depth-first search algorithm, so stash the root of the tree
     * first, then stash its children and loop back on the last item in the
     * list until the full subtree is parsed, and eventually the full tree is
     * parsed. */
    vlc_list_append( &node_root->node, &list );

    while( !vlc_list_is_empty( &list ) )
    {
        /* Pop last item in the list. */
        struct vlc_item_list *node =
            vlc_list_last_entry_or_null( &list, struct vlc_item_list, node );
        vlc_list_remove(&node->node);

        for( int i = 0; i < node->item->i_children; i++ )
        {
            input_item_node_t *child = node->item->pp_children[i];

            /* The media will be released when its children will be added to
             * the list. */
            libvlc_media_t *md_child = input_item_add_subitem( node->media, child->p_item );
            if( md_child == NULL )
                goto error;

            /* No need to lock subitems of the submedia since the submedia is
             * not yet exposed */
            struct vlc_item_list *submedia =
                wrap_item_in_list( md_child, child );
            if (submedia == NULL)
            {
                libvlc_media_release( md_child );
                goto error;
            }

            /* Stash a request to parse this subtree. */
            vlc_list_append( &submedia->node, &list );
        }

        libvlc_media_release( node->media );
        free( node );
    }

    libvlc_media_list_unlock(md->p_subitems);
    return;

error:
    libvlc_printerr( "Not enough memory" );

    struct vlc_item_list *node;
    vlc_list_foreach( node, &list, node )
    {
        if( node->media != NULL )
            libvlc_media_release( node->media );
        free( node );
    }

    libvlc_media_list_unlock(md->p_subitems);
}

void libvlc_media_add_subtree(libvlc_media_t *p_md, const input_item_node_t *node)
{
    input_item_add_subnode( p_md, node );
}

static void media_destroy( void *libvlc_owner )
{
    libvlc_media_t *p_md = libvlc_owner;

    if( p_md->p_subitems )
        libvlc_media_list_release( p_md->p_subitems );

    free( p_md );
}

/**
 * \internal
 * Create a new media descriptor object from an input_item (Private)
 *
 * That's the generic constructor
 */
libvlc_media_t * libvlc_media_new_from_input_item(input_item_t *p_input_item )
{
    libvlc_media_t * p_md;

    if (!p_input_item)
    {
        libvlc_printerr( "No input item given" );
        return NULL;
    }

    /* The item is wrapped by a single media for its whole lifetime */
    assert( p_input_item->libvlc_owner == NULL );

    p_md = calloc( 1, sizeof(libvlc_media_t) );
    if( !p_md )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_md->p_subitems = libvlc_media_list_new();
    if( p_md->p_subitems == NULL )
    {
        free( p_md );
        return NULL;
    }
    p_md->p_subitems->b_read_only = true;
    p_md->p_subitems->p_internal_md = p_md;

    p_md->p_input_item      = p_input_item;

    p_md->p_input_item->libvlc_owner = p_md;
    p_md->p_input_item->libvlc_owner_release = media_destroy;
    atomic_init(&p_md->parsed_status, libvlc_media_parsed_status_none);
    p_md->req = NULL;

    input_item_Hold( p_md->p_input_item );

    return p_md;
}

// Create a media with a certain given media resource location
libvlc_media_t *libvlc_media_new_location(const char * psz_mrl)
{
    input_item_t * p_input_item;
    libvlc_media_t * p_md;

    p_input_item = input_item_New( psz_mrl, NULL );

    if (!p_input_item)
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_md = libvlc_media_new_from_input_item( p_input_item );

    /* The p_input_item is retained in libvlc_media_new_from_input_item */
    input_item_Release( p_input_item );

    return p_md;
}

// Create a media for a certain file path
libvlc_media_t *libvlc_media_new_path(const char *path)
{
    char *mrl = vlc_path2uri( path, NULL );
    if( unlikely(mrl == NULL) )
    {
        libvlc_printerr( "%s", vlc_strerror_c(errno) );
        return NULL;
    }

    libvlc_media_t *m = libvlc_media_new_location(mrl);
    free( mrl );
    return m;
}

// Create a media for an already open file descriptor
libvlc_media_t *libvlc_media_new_fd(int fd)
{
    char mrl[16];
    snprintf( mrl, sizeof(mrl), "fd://%d", fd );

    return libvlc_media_new_location(mrl);
}

// Create a media with custom callbacks to read the data from
libvlc_media_t *libvlc_media_new_callbacks(const struct libvlc_media_open_cbs *cbs,
                                           void *cbs_opaque)
{
    assert(cbs != NULL && cbs->read != NULL);

    /* No different versions to handle for now */
    assert(cbs->version <= 0);

    libvlc_media_t *m = libvlc_media_new_location("imem://");
    if (unlikely(m == NULL))
        return NULL;

    input_item_AddOpaque(m->p_input_item, "imem-data", cbs_opaque);
    input_item_AddOpaque(m->p_input_item, "imem-cbs", (void *) cbs);

    return m;
}

// Create a media as an empty node with a given name
libvlc_media_t * libvlc_media_new_as_node(const char *psz_name)
{
    input_item_t * p_input_item;
    libvlc_media_t * p_md;

    p_input_item = input_item_New( INPUT_ITEM_URI_NOP, psz_name );

    if (!p_input_item)
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_md = libvlc_media_new_from_input_item( p_input_item );
    input_item_Release( p_input_item );

    return p_md;
}

// Add an option to the media descriptor
void libvlc_media_add_option( libvlc_media_t * p_md,
                              const char * psz_option )
{
    libvlc_media_add_option_flag( p_md, psz_option,
                          VLC_INPUT_OPTION_UNIQUE|VLC_INPUT_OPTION_TRUSTED );
}

// Same as libvlc_media_add_option but with configurable flags
void libvlc_media_add_option_flag( libvlc_media_t * p_md,
                                   const char * ppsz_option,
                                   unsigned i_flags )
{
    input_item_AddOption( p_md->p_input_item, ppsz_option, i_flags );
}

// Delete a media descriptor object
void libvlc_media_release( libvlc_media_t *p_md )
{
    if (!p_md)
        return;

    /* The media shares the reference count of its input item and is
     * destroyed with it, see media_destroy() */
    input_item_Release( p_md->p_input_item );
}

// Retain a media descriptor object
libvlc_media_t *libvlc_media_retain( libvlc_media_t *p_md )
{
    assert (p_md);
    input_item_Hold( p_md->p_input_item );
    return p_md;
}

// Duplicate a media descriptor object
libvlc_media_t *
libvlc_media_duplicate( libvlc_media_t *p_md_orig )
{

    input_item_t *dup = input_item_Copy( p_md_orig->p_input_item );
    if( dup == NULL )
        return NULL;
    libvlc_media_t *p_md = libvlc_media_new_from_input_item( dup );
    input_item_Release( dup );

    return p_md;
}

// Get mrl from a media descriptor object
char *
libvlc_media_get_mrl( libvlc_media_t * p_md )
{
    assert( p_md );
    return input_item_GetURI( p_md->p_input_item );
}

// Getter for meta information
char *libvlc_media_get_meta( libvlc_media_t *p_md, libvlc_meta_t e_meta )
{
    char *psz_meta = NULL;

    if( e_meta == libvlc_meta_NowPlaying )
    {
        psz_meta = input_item_GetNowPlayingFb( p_md->p_input_item );
    }
    else
    {
        psz_meta = input_item_GetMeta( p_md->p_input_item,
                                             libvlc_to_vlc_meta[e_meta] );
        /* Should be integrated in core */
        if( psz_meta == NULL && e_meta == libvlc_meta_Title
         && p_md->p_input_item->psz_name != NULL )
            psz_meta = strdup( p_md->p_input_item->psz_name );
    }
    return psz_meta;
}

// Set the meta of the media
void libvlc_media_set_meta( libvlc_media_t *p_md, libvlc_meta_t e_meta, const char *psz_value )
{
    assert( p_md );
    input_item_SetMeta( p_md->p_input_item, libvlc_to_vlc_meta[e_meta], psz_value );
}

// Getter for meta extra information
char *libvlc_media_get_meta_extra( libvlc_media_t *p_md, const char *psz_name )
{
    assert( p_md );
    return input_item_GetMetaExtra( p_md->p_input_item, psz_name );
}

// Set the meta extra of the media
void libvlc_media_set_meta_extra( libvlc_media_t *p_md, const char *psz_name, const char *psz_value)
{
    assert( p_md );
    input_item_SetMetaExtra( p_md->p_input_item, psz_name, psz_value );
}

// Getter for meta extra names
unsigned libvlc_media_get_meta_extra_names( libvlc_media_t *p_md, char ***pppsz_names )
{
    assert( p_md && pppsz_names );
    return input_item_GetMetaExtraNames( p_md->p_input_item, pppsz_names );
}

// Release a media meta extra names
void libvlc_media_meta_extra_names_release( char **ppsz_names, unsigned i_count )
{
    if( i_count > 0 )
    {
        assert( ppsz_names );
        for ( unsigned i = 0; i < i_count; i++ )
            free( ppsz_names[i] );
    }
    free( ppsz_names );
}

// Save the meta previously set
int libvlc_media_save_meta( libvlc_instance_t *inst, libvlc_media_t *p_md )
{
    assert( p_md );
    vlc_object_t *p_obj = VLC_OBJECT(inst->p_libvlc_int);
    return input_item_WriteMeta( p_obj, p_md->p_input_item ) == VLC_SUCCESS;
}

// Get subitems of media descriptor object.
libvlc_media_list_t *
libvlc_media_subitems( libvlc_media_t * p_md )
{
    libvlc_media_list_retain( p_md->p_subitems );
    return p_md->p_subitems;
}

// Getter for statistics information
bool libvlc_media_get_stats(libvlc_media_t *p_md,
                            libvlc_media_stats_t *p_stats)
{
    input_item_t *item = p_md->p_input_item;

    if( !p_md->p_input_item )
        return false;

    vlc_mutex_lock( &item->lock );

    input_stats_t *p_itm_stats = p_md->p_input_item->p_stats;
    if( p_itm_stats == NULL )
    {
        vlc_mutex_unlock( &item->lock );
        return false;
    }

    p_stats->i_read_bytes = p_itm_stats->i_read_bytes;
    p_stats->f_input_bitrate = p_itm_stats->f_input_bitrate;

    p_stats->i_demux_read_bytes = p_itm_stats->i_demux_read_bytes;
    p_stats->f_demux_bitrate = p_itm_stats->f_demux_bitrate;
    p_stats->i_demux_corrupted = p_itm_stats->i_demux_corrupted;
    p_stats->i_demux_discontinuity = p_itm_stats->i_demux_discontinuity;

    p_stats->i_decoded_video = p_itm_stats->i_decoded_video;
    p_stats->i_decoded_audio = p_itm_stats->i_decoded_audio;

    p_stats->i_displayed_pictures = p_itm_stats->i_displayed_pictures;
    p_stats->i_late_pictures = p_itm_stats->i_late_pictures;
    p_stats->i_lost_pictures = p_itm_stats->i_lost_pictures;

    p_stats->i_played_abuffers = p_itm_stats->i_played_abuffers;
    p_stats->i_lost_abuffers = p_itm_stats->i_lost_abuffers;

    vlc_mutex_unlock( &item->lock );
    return true;
}

// Get duration of media object (in us)
libvlc_time_t
libvlc_media_get_duration( libvlc_media_t * p_md )
{
    assert( p_md );

    if( !p_md->p_input_item )
    {
        libvlc_printerr( "No input item" );
        return -1;
    }

    if (!input_item_IsPreparsed( p_md->p_input_item ))
        return -1;

    return libvlc_time_from_vlc_tick(input_item_GetDuration( p_md->p_input_item ));
}

int
libvlc_media_get_filestat( libvlc_media_t *p_md, unsigned type, uint64_t *out )
{
    assert( p_md );
    assert( out );

    if( !p_md->p_input_item )
    {
        libvlc_printerr( "No input item" );
        return -1;
    }

    const char *name;
    switch (type)
    {
        case libvlc_media_filestat_mtime:   name = "mtime"; break;
        case libvlc_media_filestat_size:    name = "size"; break;
        default:
            libvlc_printerr( "unknown libvlc_media_stat" );
            return -1;
    };

    char *str = input_item_GetInfo( p_md->p_input_item, ".stat", name );
    if( str == NULL )
        return 0;

    char *end;
    unsigned long long val = strtoull( str, &end, 10 );

    if( *end != '\0' )
    {
        free( str );
        return -1;
    }
    free( str );

    *out = val;
    return 1;
}

bool
libvlc_media_is_parsed(libvlc_media_t *media)
{
    return atomic_load(&media->parsed_status) == libvlc_media_parsed_status_done;
}

// Sets media descriptor's user_data
void
libvlc_media_set_user_data( libvlc_media_t * p_md, void * p_new_user_data )
{
    assert( p_md );
    p_md->p_user_data = p_new_user_data;
}

// Get media descriptor's user_data
void *
libvlc_media_get_user_data( libvlc_media_t * p_md )
{
    assert( p_md );
    return p_md->p_user_data;
}

libvlc_media_tracklist_t *
libvlc_media_get_tracklist( libvlc_media_t *p_md, libvlc_track_type_t type )
{
    assert( p_md );

    input_item_t *p_input_item = p_md->p_input_item;

    vlc_mutex_lock( &p_input_item->lock );
    libvlc_media_tracklist_t *list =
        libvlc_media_tracklist_from_item( p_input_item, type );
    vlc_mutex_unlock( &p_input_item->lock );

    return list;
}

// Get codec description from media elementary stream
const char *
libvlc_media_get_codec_description( libvlc_track_type_t i_type,
                                    uint32_t i_codec )
{
    return vlc_fourcc_GetDescription( libvlc_track_type_to_escat( i_type),
                                      i_codec );
}

// Get the media type of the media descriptor object
libvlc_media_type_t libvlc_media_get_type( libvlc_media_t *p_md )
{
    assert( p_md );

    enum input_item_type_e i_type;
    input_item_t *p_input_item = p_md->p_input_item;

    vlc_mutex_lock( &p_input_item->lock );
    i_type = p_md->p_input_item->i_type;
    vlc_mutex_unlock( &p_input_item->lock );

    switch( i_type )
    {
    case ITEM_TYPE_FILE:
        return libvlc_media_type_file;
    case ITEM_TYPE_NODE:
    case ITEM_TYPE_DIRECTORY:
        return libvlc_media_type_directory;
    case ITEM_TYPE_DISC:
        return libvlc_media_type_disc;
    case ITEM_TYPE_STREAM:
        return libvlc_media_type_stream;
    case ITEM_TYPE_PLAYLIST:
        return libvlc_media_type_playlist;
    default:
        return libvlc_media_type_unknown;
    }
}

// Add a slave to the media descriptor
int libvlc_media_slaves_add( libvlc_media_t *p_md,
                             libvlc_media_slave_type_t i_type,
                             unsigned int i_priority,
                             const char *psz_uri )
{
    assert( p_md && psz_uri );
    input_item_t *p_input_item = p_md->p_input_item;

    enum slave_type i_input_slave_type;
    switch( i_type )
    {
    case libvlc_media_slave_type_subtitle:
        i_input_slave_type = SLAVE_TYPE_SPU;
        break;
    case libvlc_media_slave_type_generic:
        i_input_slave_type = SLAVE_TYPE_GENERIC;
        break;
    default:
        vlc_assert_unreachable();
        return -1;
    }

    enum slave_priority i_input_slave_priority;
    switch( i_priority )
    {
    case 0:
        i_input_slave_priority = SLAVE_PRIORITY_MATCH_NONE;
        break;
    case 1:
        i_input_slave_priority = SLAVE_PRIORITY_MATCH_RIGHT;
        break;
    case 2:
        i_input_slave_priority = SLAVE_PRIORITY_MATCH_LEFT;
        break;
    case 3:
        i_input_slave_priority = SLAVE_PRIORITY_MATCH_ALL;
        break;
    default:
    case 4:
        i_input_slave_priority = SLAVE_PRIORITY_USER;
        break;
    }

    input_item_slave_t *p_slave = input_item_slave_New( psz_uri,
                                                      i_input_slave_type,
                                                      i_input_slave_priority );
    if( p_slave == NULL )
        return -1;
    return input_item_AddSlave( p_input_item, p_slave ) == VLC_SUCCESS ? 0 : -1;
}

// Clear all slaves of the media descriptor
void libvlc_media_slaves_clear( libvlc_media_t *p_md )
{
    assert( p_md );
    input_item_t *p_input_item = p_md->p_input_item;

    vlc_mutex_lock( &p_input_item->lock );
    for( int i = 0; i < p_input_item->i_slaves; i++ )
        input_item_slave_Delete( p_input_item->pp_slaves[i] );
    TAB_CLEAN( p_input_item->i_slaves, p_input_item->pp_slaves );
    vlc_mutex_unlock( &p_input_item->lock );
}

// Get a media descriptor's slave list
unsigned int libvlc_media_slaves_get( libvlc_media_t *p_md,
                                      libvlc_media_slave_t ***ppp_slaves )
{
    assert( p_md && ppp_slaves );
    input_item_t *p_input_item = p_md->p_input_item;
    *ppp_slaves = NULL;

    vlc_mutex_lock( &p_input_item->lock );

    int i_count = p_input_item->i_slaves;
    if( i_count <= 0 )
        return vlc_mutex_unlock( &p_input_item->lock ), 0;

    libvlc_media_slave_t **pp_slaves = calloc( i_count, sizeof(*pp_slaves) );
    if( pp_slaves == NULL )
        return vlc_mutex_unlock( &p_input_item->lock ), 0;

    for( int i = 0; i < i_count; ++i )
    {
        input_item_slave_t *p_item_slave = p_input_item->pp_slaves[i];
        assert( p_item_slave->i_priority >= SLAVE_PRIORITY_MATCH_NONE );

        /* also allocate psz_uri buffer at the end of the struct */
        libvlc_media_slave_t *p_slave = malloc( sizeof(*p_slave) +
                                                strlen( p_item_slave->psz_uri )
                                                + 1 );
        if( p_slave == NULL )
        {
            libvlc_media_slaves_release(pp_slaves, i);
            return vlc_mutex_unlock( &p_input_item->lock ), 0;
        }
        p_slave->psz_uri = (char *) ((uint8_t *)p_slave) + sizeof(*p_slave);
        strcpy( p_slave->psz_uri, p_item_slave->psz_uri );

        switch( p_item_slave->i_type )
        {
        case SLAVE_TYPE_SPU:
            p_slave->i_type = libvlc_media_slave_type_subtitle;
            break;
        case SLAVE_TYPE_GENERIC:
            p_slave->i_type = libvlc_media_slave_type_generic;
            break;
        default:
            vlc_assert_unreachable();
        }

        switch( p_item_slave->i_priority )
        {
        case SLAVE_PRIORITY_MATCH_NONE:
            p_slave->i_priority = 0;
            break;
        case SLAVE_PRIORITY_MATCH_RIGHT:
            p_slave->i_priority = 1;
            break;
        case SLAVE_PRIORITY_MATCH_LEFT:
            p_slave->i_priority = 2;
            break;
        case SLAVE_PRIORITY_MATCH_ALL:
            p_slave->i_priority = 3;
            break;
        case SLAVE_PRIORITY_USER:
            p_slave->i_priority = 4;
            break;
        default:
            vlc_assert_unreachable();
        }
        pp_slaves[i] = p_slave;
    }
    vlc_mutex_unlock( &p_input_item->lock );

    *ppp_slaves = pp_slaves;
    return i_count;
}

// Release a media descriptor's slave list
void libvlc_media_slaves_release( libvlc_media_slave_t **pp_slaves,
                                  unsigned int i_count )
{
    if( i_count > 0 )
    {
        assert( pp_slaves );
        for( unsigned int i = 0; i < i_count; ++i )
            free( pp_slaves[i] );
    }
    free( pp_slaves );
}
