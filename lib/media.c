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
#include <vlc/libvlc_events.h>

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
    libvlc_event_t event;

    p_md_child = libvlc_media_new_from_input_item( item );

    /* Add this to our media list */
    p_subitems = p_md->p_subitems;
    libvlc_media_list_lock( p_subitems );
    libvlc_media_list_internal_add_media( p_subitems, p_md_child );
    libvlc_media_list_unlock( p_subitems );

    /* Construct the event */
    event.type = libvlc_MediaSubItemAdded;
    event.u.media_subitem_added.new_child = p_md_child;

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );
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
}

/**
 * \internal
 * input_item_subitemtree_added (Private) (vlc event Callback)
 */
static void input_item_subtree_added(vlc_preparser_req *req,
                                     input_item_node_t *node,
                                     void *user_data)
{
    VLC_UNUSED(req);
    libvlc_media_t * p_md = user_data;
    libvlc_media_add_subtree(p_md, node);
    input_item_node_Delete(node);
}

void libvlc_media_add_subtree(libvlc_media_t *p_md, const input_item_node_t *node)
{
    input_item_add_subnode( p_md, node );

    /* Construct the event */
    libvlc_event_t event;
    event.type = libvlc_MediaSubItemTreeAdded;
    event.u.media_subitemtree_added.item = p_md;

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );
}

static void input_item_attachments_added( vlc_preparser_req *req,
                                          input_attachment_t *const *array,
                                          size_t count, void *user_data )
{
    VLC_UNUSED(req);
    libvlc_media_t * p_md = user_data;
    libvlc_event_t event;

    libvlc_picture_list_t* list =
        libvlc_picture_list_from_attachments(array, count);
    if( !list )
        return;
    if( !libvlc_picture_list_count(list) )
    {
        libvlc_picture_list_destroy( list );
        return;
    }

    /* Construct the event */
    event.type = libvlc_MediaAttachedThumbnailsFound;
    event.u.media_attached_thumbnails_found.thumbnails = list;

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );

    libvlc_picture_list_destroy( list );
}

static void send_parsed_changed( libvlc_media_t *p_md,
                                 libvlc_media_parsed_status_t new_status )
{
    libvlc_event_t event;

    if (atomic_exchange(&p_md->parsed_status, new_status) == new_status)
        return;

    /* Duration event */
    event.type = libvlc_MediaDurationChanged;
    event.u.media_duration_changed.new_duration =
        libvlc_time_from_vlc_tick(input_item_GetDuration( p_md->p_input_item ));
    libvlc_event_send( &p_md->event_manager, &event );

    /* Meta event */
    event.type = libvlc_MediaMetaChanged;
    event.u.media_meta_changed.meta_type = 0;
    libvlc_event_send( &p_md->event_manager, &event );

    /* Parsed event */
    event.type = libvlc_MediaParsedChanged;
    event.u.media_parsed_changed.new_status = new_status;
    libvlc_event_send( &p_md->event_manager, &event );

    libvlc_media_list_t *p_subitems = p_md->p_subitems;
    /* notify the media list */
    libvlc_media_list_lock( p_subitems );
    libvlc_media_list_internal_end_reached( p_subitems );
    libvlc_media_list_unlock( p_subitems );
}

/**
 * \internal
 * input_item_preparse_ended (Private) (vlc event Callback)
 */
static void input_item_preparse_ended(vlc_preparser_req *req,
                                      int status, void *user_data)
{
    libvlc_media_t * p_md = user_data;
    libvlc_media_parsed_status_t new_status;

    switch( status )
    {
        case VLC_EGENERIC:
            new_status = libvlc_media_parsed_status_failed;
            break;
        case VLC_ETIMEOUT:
            new_status = libvlc_media_parsed_status_timeout;
            break;
        case -EINTR:
            new_status = libvlc_media_parsed_status_cancelled;
            break;
        case VLC_SUCCESS:
            new_status = libvlc_media_parsed_status_done;
            break;
        default:
            vlc_assert_unreachable();
    }
    send_parsed_changed( p_md, new_status );
    vlc_preparser_req_Release( req );
    p_md->req = NULL;

    if (atomic_fetch_sub_explicit(&p_md->worker_count, 1,
                                  memory_order_release) == 1)
        vlc_atomic_notify_one(&p_md->worker_count);
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
    vlc_atomic_rc_init(&p_md->rc);

    atomic_init(&p_md->worker_count, 0);

    p_md->p_input_item->libvlc_owner = p_md;
    atomic_init(&p_md->parsed_status, libvlc_media_parsed_status_none);
    p_md->req = NULL;

    libvlc_event_manager_init( &p_md->event_manager, p_md );

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
libvlc_media_t *libvlc_media_new_callbacks(libvlc_media_open_cb open_cb,
                                           libvlc_media_read_cb read_cb,
                                           libvlc_media_seek_cb seek_cb,
                                           libvlc_media_close_cb close_cb,
                                           void *opaque)
{
    libvlc_media_t *m = libvlc_media_new_location("imem://");
    if (unlikely(m == NULL))
        return NULL;

    assert(read_cb != NULL);
    input_item_AddOpaque(m->p_input_item, "imem-data", opaque);
    input_item_AddOpaque(m->p_input_item, "imem-open", open_cb);
    input_item_AddOpaque(m->p_input_item, "imem-read", read_cb);
    input_item_AddOpaque(m->p_input_item, "imem-seek", seek_cb);
    input_item_AddOpaque(m->p_input_item, "imem-close", close_cb);
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
    unsigned int ref;

    if (!p_md)
        return;

    if( !vlc_atomic_rc_dec(&p_md->rc) )
        return;

    /* Wait for all async tasks to stop. */
    while ((ref = atomic_load_explicit(&p_md->worker_count,
                                       memory_order_acquire)) > 0)
        vlc_atomic_wait(&p_md->worker_count, ref);

    if( p_md->p_subitems )
        libvlc_media_list_release( p_md->p_subitems );

    input_item_Release( p_md->p_input_item );

    libvlc_event_manager_destroy( &p_md->event_manager );
    free( p_md );
}

// Retain a media descriptor object
libvlc_media_t *libvlc_media_retain( libvlc_media_t *p_md )
{
    assert (p_md);
    vlc_atomic_rc_inc( &p_md->rc );
    return p_md;
}

// Duplicate a media descriptor object
libvlc_media_t *
libvlc_media_duplicate( libvlc_media_t *p_md_orig )
{

    input_item_t *dup = input_item_Copy( p_md_orig->p_input_item );
    if( dup == NULL )
        return NULL;
    return libvlc_media_new_from_input_item( dup );
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

// Get event manager from a media descriptor object
libvlc_event_manager_t *
libvlc_media_event_manager( libvlc_media_t * p_md )
{
    assert( p_md );

    return &p_md->event_manager;
}

// Get duration of media object (in ms)
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

static const struct vlc_preparser_cbs preparser_callbacks = {
    .on_ended = input_item_preparse_ended,
    .on_subtree_added = input_item_subtree_added,
    .on_attachments_added = input_item_attachments_added,
};

int libvlc_media_parse_request(libvlc_instance_t *inst, libvlc_media_t *media,
                               libvlc_media_parse_flag_t parse_flag,
                               int timeout)
{
    libvlc_media_parsed_status_t expected = libvlc_media_parsed_status_none;

    while (!atomic_compare_exchange_weak(&media->parsed_status, &expected,
                                        libvlc_media_parsed_status_pending))
        if (expected == libvlc_media_parsed_status_pending
         || expected == libvlc_media_parsed_status_done)
            return -1;

    vlc_preparser_t *parser = libvlc_get_preparser(inst);
    if (unlikely(parser == NULL))
        return -1;

    input_item_t *item = media->p_input_item;
    int parse_scope = 0;
    unsigned int ref = atomic_load_explicit(&media->worker_count,
                                            memory_order_relaxed);
    do
    {
        if (unlikely(ref == UINT_MAX))
            return -1;
    }
    while (!atomic_compare_exchange_weak_explicit(&media->worker_count,
                                                  &ref, ref + 1,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed));

    bool input_net;
    enum input_item_type_e input_type = input_item_GetType(item, &input_net);

    bool do_parse, do_fetch;
    if (parse_flag & libvlc_media_parse_forced)
        do_parse = true;
    else
    {
        if (input_net)
            do_parse = parse_flag & libvlc_media_parse_network;
        else if (parse_flag & libvlc_media_parse_local)
        {
            switch (input_type)
            {
                case ITEM_TYPE_NODE:
                case ITEM_TYPE_FILE:
                case ITEM_TYPE_DIRECTORY:
                case ITEM_TYPE_PLAYLIST:
                    do_parse = true;
                    break;
                default:
                    do_parse = false;
                    break;
            }
        }
        else
            do_parse = false;
    }

    if (do_parse)
        parse_scope |= VLC_PREPARSER_TYPE_PARSE;

    do_fetch = false;
    if (parse_flag & libvlc_media_fetch_local)
    {
        parse_scope |= VLC_PREPARSER_TYPE_FETCHMETA_LOCAL;
        do_fetch = true;
    }
    if (parse_flag & libvlc_media_fetch_network)
    {
        parse_scope |= VLC_PREPARSER_TYPE_FETCHMETA_NET;
        do_fetch = true;
    }

    if (!do_parse && !do_fetch)
    {
        send_parsed_changed( media, libvlc_media_parsed_status_skipped );
        atomic_fetch_sub_explicit(&media->worker_count, 1,
                                  memory_order_relaxed);
        return 0;
    }

    if (parse_flag & libvlc_media_do_interact)
        parse_scope |= VLC_PREPARSER_OPTION_INTERACT;
    parse_scope |= VLC_PREPARSER_OPTION_SUBITEMS;

    if (timeout == -1)
        timeout = var_InheritInteger(inst->p_libvlc_int, "preparse-timeout");

    vlc_preparser_SetTimeout(parser, VLC_TICK_FROM_MS(timeout));

    media->req = vlc_preparser_Push(parser, item, parse_scope,
                                    &preparser_callbacks, media);
    if (media->req == NULL)
    {
        atomic_fetch_sub_explicit(&media->worker_count, 1,
                                  memory_order_relaxed);
        return -1;
    }
    return 0;
}

// Stop parsing of the media
void
libvlc_media_parse_stop(libvlc_instance_t *inst, libvlc_media_t *media)
{
    vlc_preparser_t *parser = libvlc_get_preparser(inst);
    assert(parser != NULL);
    if (media->req != NULL)
    {
        vlc_preparser_Cancel(parser, media->req);
        media->req = NULL;
    }
}

// Get Parsed status for media descriptor object
libvlc_media_parsed_status_t
libvlc_media_get_parsed_status(libvlc_media_t *media)
{
    return atomic_load(&media->parsed_status);
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

struct libvlc_media_thumbnail_request_t
{
    libvlc_instance_t *instance;
    libvlc_media_t *md;
    unsigned int width;
    unsigned int height;
    bool crop;
    libvlc_picture_type_t type;
    vlc_preparser_req *preparser_req;
};

static void media_on_thumbnail_ready( vlc_preparser_req *request, int status,
                                      picture_t* thumbnail, void* data )
{
    (void) status;

    libvlc_media_thumbnail_request_t *req = data;
    libvlc_media_t *p_media = req->md;
    libvlc_event_t event;
    event.type = libvlc_MediaThumbnailGenerated;
    libvlc_picture_t* pic = NULL;
    if ( thumbnail != NULL )
        pic = libvlc_picture_new( VLC_OBJECT(req->instance->p_libvlc_int),
                                    thumbnail, req->type, req->width, req->height,
                                    req->crop );
    event.u.media_thumbnail_generated.p_thumbnail = pic;
    libvlc_event_send( &p_media->event_manager, &event );
    if ( pic != NULL )
        libvlc_picture_release( pic );

    vlc_preparser_req_Release( request );
}

// Start an asynchronous thumbnail generation
static libvlc_media_thumbnail_request_t*
libvlc_media_thumbnail_request( libvlc_instance_t *inst,
                                libvlc_media_t *md,
                                const struct vlc_thumbnailer_arg *thumb_arg,
                                unsigned int width, unsigned int height,
                                bool crop, libvlc_picture_type_t picture_type,
                                libvlc_time_t timeout )
{
    assert( md );

    vlc_preparser_t *thumb = libvlc_get_thumbnailer(inst);
    if (unlikely(thumb == NULL))
        return NULL;

    vlc_preparser_SetTimeout( thumb, vlc_tick_from_libvlc_time( timeout ) );

    libvlc_media_thumbnail_request_t *req = malloc( sizeof( *req ) );
    if ( unlikely( req == NULL ) )
        return NULL;

    req->instance = inst;
    req->md = md;
    req->width = width;
    req->height = height;
    req->type = picture_type;
    req->crop = crop;
    libvlc_media_retain( md );
    static const struct vlc_thumbnailer_cbs cbs = {
        .on_ended = media_on_thumbnail_ready,
    };
    req->preparser_req = vlc_preparser_GenerateThumbnail( thumb, md->p_input_item,
                                                          thumb_arg, &cbs, req );
    if ( req->preparser_req == NULL )
    {
        free( req );
        libvlc_media_release( md );
        return NULL;
    }
    libvlc_retain(inst);
    return req;
}

libvlc_media_thumbnail_request_t*
libvlc_media_thumbnail_request_by_time( libvlc_instance_t *inst,
                                        libvlc_media_t *md, libvlc_time_t time,
                                        libvlc_thumbnailer_seek_speed_t speed,
                                        unsigned int width, unsigned int height,
                                        bool crop, libvlc_picture_type_t picture_type,
                                        libvlc_time_t timeout )
{
    const struct vlc_thumbnailer_arg thumb_arg = {
        .seek = {
            .type = VLC_THUMBNAILER_SEEK_TIME,
            .time = vlc_tick_from_libvlc_time( time ),
            .speed = speed == libvlc_media_thumbnail_seek_fast ?
                VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE,
        },
        .hw_dec = false,
    };
    return libvlc_media_thumbnail_request( inst, md, &thumb_arg, width, height,
                                           crop, picture_type, timeout );
}

// Start an asynchronous thumbnail generation
libvlc_media_thumbnail_request_t*
libvlc_media_thumbnail_request_by_pos( libvlc_instance_t *inst,
                                       libvlc_media_t *md, double pos,
                                       libvlc_thumbnailer_seek_speed_t speed,
                                       unsigned int width, unsigned int height,
                                       bool crop, libvlc_picture_type_t picture_type,
                                       libvlc_time_t timeout )
{
    const struct vlc_thumbnailer_arg thumb_arg = {
        .seek = {
            .type = VLC_THUMBNAILER_SEEK_POS,
            .pos = pos,
            .speed = speed == libvlc_media_thumbnail_seek_fast ?
                VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE,
        },
        .hw_dec = false,
    };
    return libvlc_media_thumbnail_request( inst, md, &thumb_arg, width, height,
                                           crop, picture_type, timeout );
}

// Destroy a thumbnail request
void libvlc_media_thumbnail_request_destroy( libvlc_media_thumbnail_request_t *req )
{
    vlc_preparser_t *thumb = libvlc_get_thumbnailer(req->instance);
    assert(thumb != NULL);

    vlc_preparser_Cancel( thumb, req->preparser_req );
    libvlc_media_release( req->md );
    libvlc_release(req->instance);
    free( req );
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
