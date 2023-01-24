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
#include <vlc_thumbnailer.h>
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

static const libvlc_meta_t vlc_to_libvlc_meta[] =
{
    [vlc_meta_Title]        = libvlc_meta_Title,
    [vlc_meta_Artist]       = libvlc_meta_Artist,
    [vlc_meta_Genre]        = libvlc_meta_Genre,
    [vlc_meta_Copyright]    = libvlc_meta_Copyright,
    [vlc_meta_Album]        = libvlc_meta_Album,
    [vlc_meta_TrackNumber]  = libvlc_meta_TrackNumber,
    [vlc_meta_Description]  = libvlc_meta_Description,
    [vlc_meta_Rating]       = libvlc_meta_Rating,
    [vlc_meta_Date]         = libvlc_meta_Date,
    [vlc_meta_Setting]      = libvlc_meta_Setting,
    [vlc_meta_URL]          = libvlc_meta_URL,
    [vlc_meta_Language]     = libvlc_meta_Language,
    [vlc_meta_NowPlaying]   = libvlc_meta_NowPlaying,
    [vlc_meta_ESNowPlaying] = libvlc_meta_NowPlaying,
    [vlc_meta_Publisher]    = libvlc_meta_Publisher,
    [vlc_meta_EncodedBy]    = libvlc_meta_EncodedBy,
    [vlc_meta_ArtworkURL]   = libvlc_meta_ArtworkURL,
    [vlc_meta_TrackID]      = libvlc_meta_TrackID,
    [vlc_meta_TrackTotal]   = libvlc_meta_TrackTotal,
    [vlc_meta_Director]     = libvlc_meta_Director,
    [vlc_meta_Season]       = libvlc_meta_Season,
    [vlc_meta_Episode]      = libvlc_meta_Episode,
    [vlc_meta_ShowName]     = libvlc_meta_ShowName,
    [vlc_meta_Actors]       = libvlc_meta_Actors,
    [vlc_meta_AlbumArtist]  = libvlc_meta_AlbumArtist,
    [vlc_meta_DiscNumber]   = libvlc_meta_DiscNumber,
    [vlc_meta_DiscTotal]    = libvlc_meta_DiscTotal
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

static libvlc_media_list_t *media_get_subitems( libvlc_media_t * p_md,
                                                bool b_create )
{
    libvlc_media_list_t *p_subitems = NULL;

    vlc_mutex_lock( &p_md->subitems_lock );
    if( p_md->p_subitems == NULL && b_create )
    {
        p_md->p_subitems = libvlc_media_list_new();
        if( p_md->p_subitems != NULL )
        {
            p_md->p_subitems->b_read_only = true;
            p_md->p_subitems->p_internal_md = p_md;
        }
    }
    p_subitems = p_md->p_subitems;
    vlc_mutex_unlock( &p_md->subitems_lock );
    return p_subitems;
}

static libvlc_media_t *input_item_add_subitem( libvlc_media_t *p_md,
                                               input_item_t *item )
{
    libvlc_media_t * p_md_child;
    libvlc_media_list_t *p_subitems;
    libvlc_event_t event;

    p_md_child = libvlc_media_new_from_input_item( item );

    /* Add this to our media list */
    p_subitems = media_get_subitems( p_md, true );
    if( p_subitems != NULL )
    {
        libvlc_media_list_lock( p_subitems );
        libvlc_media_list_internal_add_media( p_subitems, p_md_child );
        libvlc_media_list_unlock( p_subitems );
    }

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
    input_item_node_t *item;
    libvlc_media_t *media;
};

static struct vlc_item_list *
wrap_item_in_list( libvlc_media_t *media, input_item_node_t *item )
{
    struct vlc_item_list *node = malloc( sizeof *node );
    if( node == NULL )
        return NULL;
    node->item = item;
    node->media = media;
    return node;
}

static void input_item_add_subnode( libvlc_media_t *md,
                                    input_item_node_t *root )
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
static void input_item_subtree_added(input_item_t *item,
                                     input_item_node_t *node,
                                     void *user_data)
{
    VLC_UNUSED(item);
    libvlc_media_t * p_md = user_data;
    libvlc_media_add_subtree(p_md, node);
}

void libvlc_media_add_subtree(libvlc_media_t *p_md, input_item_node_t *node)
{
    input_item_add_subnode( p_md, node );

    /* Construct the event */
    libvlc_event_t event;
    event.type = libvlc_MediaSubItemTreeAdded;
    event.u.media_subitemtree_added.item = p_md;

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );
}

/**
 * \internal
 * input_item_meta_changed (Private) (vlc event Callback)
 */
static void input_item_meta_changed( const vlc_event_t *p_event,
                                     void * user_data )
{
    libvlc_media_t * p_md = user_data;
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaMetaChanged;
    event.u.media_meta_changed.meta_type =
        vlc_to_libvlc_meta[p_event->u.input_item_meta_changed.meta_type];

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );
}

/**
 * \internal
 * input_item_duration_changed (Private) (vlc event Callback)
 */
static void input_item_duration_changed( const vlc_event_t *p_event,
                                         void * user_data )
{
    libvlc_media_t * p_md = user_data;
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaDurationChanged;
    event.u.media_duration_changed.new_duration =
        libvlc_time_from_vlc_tick(p_event->u.input_item_duration_changed.new_duration);

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );
}

static void input_item_attachments_found( const vlc_event_t *p_event,
                                                 void * user_data )
{
    libvlc_media_t * p_md = user_data;
    libvlc_event_t event;

    libvlc_picture_list_t* list = libvlc_picture_list_from_attachments(
                p_event->u.input_item_attachments_found.attachments,
                p_event->u.input_item_attachments_found.count );
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

    /* Construct the event */
    event.type = libvlc_MediaParsedChanged;
    event.u.media_parsed_changed.new_status = new_status;

    /* Send the event */
    libvlc_event_send( &p_md->event_manager, &event );

    libvlc_media_list_t *p_subitems = media_get_subitems( p_md, false );
    if( p_subitems != NULL )
    {
        /* notify the media list */
        libvlc_media_list_lock( p_subitems );
        libvlc_media_list_internal_end_reached( p_subitems );
        libvlc_media_list_unlock( p_subitems );
    }
}

/**
 * \internal
 * input_item_preparse_ended (Private) (vlc event Callback)
 */
static void input_item_preparse_ended(input_item_t *item,
                                      enum input_item_preparse_status status,
                                      void *user_data)
{
    VLC_UNUSED(item);
    libvlc_media_t * p_md = user_data;
    libvlc_media_parsed_status_t new_status;

    switch( status )
    {
        case ITEM_PREPARSE_SKIPPED:
            new_status = libvlc_media_parsed_status_skipped;
            break;
        case ITEM_PREPARSE_FAILED:
            new_status = libvlc_media_parsed_status_failed;
            break;
        case ITEM_PREPARSE_TIMEOUT:
            new_status = libvlc_media_parsed_status_timeout;
            break;
        case ITEM_PREPARSE_DONE:
            new_status = libvlc_media_parsed_status_done;
            break;
        default:
            return;
    }
    send_parsed_changed( p_md, new_status );

    if (atomic_fetch_sub_explicit(&p_md->worker_count, 1,
                                  memory_order_release) == 1)
        vlc_atomic_notify_one(&p_md->worker_count);
}

/**
 * \internal
 * Install event handler (Private)
 */
static void install_input_item_observer( libvlc_media_t *p_md )
{
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemMetaChanged,
                      input_item_meta_changed,
                      p_md );
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemDurationChanged,
                      input_item_duration_changed,
                      p_md );
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemAttachmentsFound,
                      input_item_attachments_found,
                      p_md );
}

/**
 * \internal
 * Uninstall event handler (Private)
 */
static void uninstall_input_item_observer( libvlc_media_t *p_md )
{
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemMetaChanged,
                      input_item_meta_changed,
                      p_md );
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemDurationChanged,
                      input_item_duration_changed,
                      p_md );
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemAttachmentsFound,
                      input_item_attachments_found,
                      p_md );
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

    p_md->p_input_item      = p_input_item;
    vlc_atomic_rc_init(&p_md->rc);

    vlc_mutex_init(&p_md->subitems_lock);
    atomic_init(&p_md->worker_count, 0);

    /* A media descriptor can be a playlist. When you open a playlist
     * It can give a bunch of item to read. */
    p_md->p_subitems        = NULL;
    p_md->p_input_item->libvlc_owner = p_md;
    atomic_init(&p_md->parsed_status, libvlc_media_parsed_status_none);

    libvlc_event_manager_init( &p_md->event_manager, p_md );

    input_item_Hold( p_md->p_input_item );

    install_input_item_observer( p_md );
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
    libvlc_media_list_t * p_subitems;

    p_input_item = input_item_New( INPUT_ITEM_URI_NOP, psz_name );

    if (!p_input_item)
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_md = libvlc_media_new_from_input_item( p_input_item );
    input_item_Release( p_input_item );

    p_subitems = media_get_subitems( p_md, true );
    if( p_subitems == NULL) {
        libvlc_media_release( p_md );
        return NULL;
    }

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

    uninstall_input_item_observer( p_md );

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
void libvlc_media_retain( libvlc_media_t *p_md )
{
    assert (p_md);
    vlc_atomic_rc_inc( &p_md->rc );
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
    libvlc_media_list_t *p_subitems = media_get_subitems( p_md, true );
    if( p_subitems )
        libvlc_media_list_retain( p_subitems );
    return p_subitems;
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

static const input_preparser_callbacks_t input_preparser_callbacks = {
    .on_preparse_ended = input_item_preparse_ended,
    .on_subtree_added = input_item_subtree_added,
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

    libvlc_int_t *libvlc = inst->p_libvlc_int;
    input_item_t *item = media->p_input_item;
    input_item_meta_request_option_t parse_scope = META_REQUEST_OPTION_SCOPE_LOCAL;
    int ret;
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

    if (parse_flag & libvlc_media_parse_network)
        parse_scope |= META_REQUEST_OPTION_SCOPE_NETWORK;
    if (parse_flag & libvlc_media_fetch_local)
        parse_scope |= META_REQUEST_OPTION_FETCH_LOCAL;
    if (parse_flag & libvlc_media_fetch_network)
        parse_scope |= META_REQUEST_OPTION_FETCH_NETWORK;
    if (parse_flag & libvlc_media_do_interact)
        parse_scope |= META_REQUEST_OPTION_DO_INTERACT;
    if (parse_flag & libvlc_media_no_skip)
        parse_scope |= META_REQUEST_OPTION_NO_SKIP;

    ret = libvlc_MetadataRequest(libvlc, item, parse_scope,
                                 &input_preparser_callbacks, media,
                                 timeout, media);
    if (ret != VLC_SUCCESS)
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
    libvlc_MetadataCancel(inst->p_libvlc_int, media);
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
        libvlc_media_tracklist_from_es_array( p_input_item->es,
                                              p_input_item->i_es, type );
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
    vlc_thumbnailer_request_t* req;
};

static void media_on_thumbnail_ready( void* data, picture_t* thumbnail )
{
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
}

// Start an asynchronous thumbnail generation
libvlc_media_thumbnail_request_t*
libvlc_media_thumbnail_request_by_time( libvlc_instance_t *inst,
                                        libvlc_media_t *md, libvlc_time_t time,
                                        libvlc_thumbnailer_seek_speed_t speed,
                                        unsigned int width, unsigned int height,
                                        bool crop, libvlc_picture_type_t picture_type,
                                        libvlc_time_t timeout )
{
    assert( md );

    libvlc_priv_t *p_priv = libvlc_priv(inst->p_libvlc_int);
    if( unlikely( p_priv->p_thumbnailer == NULL ) )
        return NULL;

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
    req->req = vlc_thumbnailer_RequestByTime( p_priv->p_thumbnailer,
        vlc_tick_from_libvlc_time( time ),
        speed == libvlc_media_thumbnail_seek_fast ?
            VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE,
        md->p_input_item,
        timeout > 0 ? vlc_tick_from_libvlc_time( timeout ) : VLC_TICK_INVALID,
        media_on_thumbnail_ready, req );
    if ( req->req == NULL )
    {
        free( req );
        libvlc_media_release( md );
        return NULL;
    }
    libvlc_retain(inst);
    return req;
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
    assert( md );

    libvlc_priv_t *priv = libvlc_priv(inst->p_libvlc_int);
    if( unlikely( priv->p_thumbnailer == NULL ) )
        return NULL;

    libvlc_media_thumbnail_request_t *req = malloc( sizeof( *req ) );
    if ( unlikely( req == NULL ) )
        return NULL;

    req->instance = inst;
    req->md = md;
    req->width = width;
    req->height = height;
    req->crop = crop;
    req->type = picture_type;
    libvlc_media_retain( md );
    req->req = vlc_thumbnailer_RequestByPos( priv->p_thumbnailer, pos,
        speed == libvlc_media_thumbnail_seek_fast ?
            VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE,
        md->p_input_item,
        timeout > 0 ? vlc_tick_from_libvlc_time( timeout ) : VLC_TICK_INVALID,
        media_on_thumbnail_ready, req );
    if ( req->req == NULL )
    {
        free( req );
        libvlc_media_release( md );
        return NULL;
    }
    libvlc_retain(inst);
    return req;
}

// Destroy a thumbnail request
void libvlc_media_thumbnail_request_destroy( libvlc_media_thumbnail_request_t *req )
{
    libvlc_priv_t *p_priv = libvlc_priv(req->instance->p_libvlc_int);
    assert( p_priv->p_thumbnailer != NULL );
    vlc_thumbnailer_DestroyRequest( p_priv->p_thumbnailer, req->req );
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
