/*****************************************************************************
 * media.c: Libvlc API media descripor management
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h> // For the subitems, here for convenience
#include <vlc/libvlc_events.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_playlist.h> /* For the preparser */

#include "libvlc.h"

#include "libvlc_internal.h"
#include "media_internal.h"

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
    [libvlc_meta_TrackID]      = vlc_meta_TrackID
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
    [vlc_meta_Publisher]    = libvlc_meta_Publisher,
    [vlc_meta_EncodedBy]    = libvlc_meta_EncodedBy,
    [vlc_meta_ArtworkURL]   = libvlc_meta_ArtworkURL,
    [vlc_meta_TrackID]      = libvlc_meta_TrackID
};

/**************************************************************************
 * input_item_subitem_added (Private) (vlc event Callback)
 **************************************************************************/
static void input_item_subitem_added( const vlc_event_t *p_event,
                                       void * user_data )
{
    libvlc_media_t * p_md = user_data;
    libvlc_media_t * p_md_child;
    libvlc_event_t event;

    p_md_child = libvlc_media_new_from_input_item(
                p_md->p_libvlc_instance,
                p_event->u.input_item_subitem_added.p_new_child, NULL );

    /* Add this to our media list */
    if( !p_md->p_subitems )
    {
        p_md->p_subitems = libvlc_media_list_new( p_md->p_libvlc_instance, NULL );
        libvlc_media_list_set_media( p_md->p_subitems, p_md, NULL );
    }
    if( p_md->p_subitems )
    {
        libvlc_media_list_add_media( p_md->p_subitems, p_md_child, NULL );
    }

    /* Construct the event */
    event.type = libvlc_MediaSubItemAdded;
    event.u.media_subitem_added.new_child = p_md_child;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );
    libvlc_media_release( p_md_child );
}

/**************************************************************************
 * input_item_meta_changed (Private) (vlc event Callback)
 **************************************************************************/
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
    libvlc_event_send( p_md->p_event_manager, &event );
}

/**************************************************************************
 * input_item_duration_changed (Private) (vlc event Callback)
 **************************************************************************/
static void input_item_duration_changed( const vlc_event_t *p_event,
                                         void * user_data )
{
    libvlc_media_t * p_md = user_data;
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaDurationChanged;
    event.u.media_duration_changed.new_duration = 
        p_event->u.input_item_duration_changed.new_duration;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );
}

/**************************************************************************
 * input_item_preparsed_changed (Private) (vlc event Callback)
 **************************************************************************/
static void input_item_preparsed_changed( const vlc_event_t *p_event,
                                          void * user_data )
{
    libvlc_media_t * p_md = user_data;
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaPreparsedChanged;
    event.u.media_preparsed_changed.new_status = 
        p_event->u.input_item_preparsed_changed.new_status;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );
}

/**************************************************************************
 * Install event handler (Private)
 **************************************************************************/
static void install_input_item_observer( libvlc_media_t *p_md )
{
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemSubItemAdded,
                      input_item_subitem_added,
                      p_md );
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemMetaChanged,
                      input_item_meta_changed,
                      p_md );
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemDurationChanged,
                      input_item_duration_changed,
                      p_md );
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemPreparsedChanged,
                      input_item_preparsed_changed,
                      p_md );
}

/**************************************************************************
 * Uninstall event handler (Private)
 **************************************************************************/
static void uninstall_input_item_observer( libvlc_media_t *p_md )
{
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemSubItemAdded,
                      input_item_subitem_added,
                      p_md );
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemMetaChanged,
                      input_item_meta_changed,
                      p_md );
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemDurationChanged,
                      input_item_duration_changed,
                      p_md );
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemPreparsedChanged,
                      input_item_preparsed_changed,
                      p_md );
}

/**************************************************************************
 * Preparse if not already done (Private)
 **************************************************************************/
static void preparse_if_needed( libvlc_media_t *p_md )
{
    /* XXX: need some locking here */
    if (!p_md->b_preparsed)
    {
        playlist_PreparseEnqueue(
                libvlc_priv (p_md->p_libvlc_instance->p_libvlc_int)->p_playlist,
                p_md->p_input_item, pl_Unlocked );
        p_md->b_preparsed = true;
    }
}

/**************************************************************************
 * Create a new media descriptor object from an input_item
 * (libvlc internal)
 * That's the generic constructor
 **************************************************************************/
libvlc_media_t * libvlc_media_new_from_input_item(
                                   libvlc_instance_t *p_instance,
                                   input_item_t *p_input_item,
                                   libvlc_exception_t *p_e )
{
    libvlc_media_t * p_md;

    if (!p_input_item)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "No input item given" );
        return NULL;
    }

    p_md = malloc( sizeof(libvlc_media_t) );
    if( !p_md )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_md->p_libvlc_instance = p_instance;
    p_md->p_input_item      = p_input_item;
    p_md->b_preparsed       = false;
    p_md->i_refcount        = 1;
    p_md->p_user_data       = NULL;

    p_md->state = libvlc_NothingSpecial;

    /* A media descriptor can be a playlist. When you open a playlist
     * It can give a bunch of item to read. */
    p_md->p_subitems        = NULL;

    p_md->p_event_manager = libvlc_event_manager_new( p_md, p_instance, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaMetaChanged, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaSubItemAdded, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaFreed, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaDurationChanged, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaStateChanged, p_e );

    vlc_gc_incref( p_md->p_input_item );

    install_input_item_observer( p_md );

    return p_md;
}

/**************************************************************************
 * Create a new media descriptor object
 **************************************************************************/
libvlc_media_t * libvlc_media_new(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_mrl,
                                   libvlc_exception_t *p_e )
{
    input_item_t * p_input_item;
    libvlc_media_t * p_md;

    p_input_item = input_item_New( p_instance->p_libvlc_int, psz_mrl, NULL );

    if (!p_input_item)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_md = libvlc_media_new_from_input_item( p_instance,
                p_input_item, p_e );

    /* The p_input_item is retained in libvlc_media_new_from_input_item */
    vlc_gc_decref( p_input_item );

    return p_md;
}

/**************************************************************************
 * Create a new media descriptor object
 **************************************************************************/
libvlc_media_t * libvlc_media_new_as_node(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_name,
                                   libvlc_exception_t *p_e )
{
    input_item_t * p_input_item;
    libvlc_media_t * p_md;

    p_input_item = input_item_New( p_instance->p_libvlc_int, "vlc://nop", psz_name );

    if (!p_input_item)
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_md = libvlc_media_new_from_input_item( p_instance,
                p_input_item, p_e );

    p_md->p_subitems = libvlc_media_list_new( p_md->p_libvlc_instance, NULL );

    return p_md;
}

/**************************************************************************
 * Add an option to the media descriptor,
 * that will be used to determine how the media_player will read the
 * media. This allow to use VLC advanced reading/streaming
 * options in a per-media basis
 *
 * The options are detailled in vlc --long-help, for instance "--sout-all"
 **************************************************************************/
void libvlc_media_add_option(
                                   libvlc_media_t * p_md,
                                   const char * psz_option )
{
    input_item_AddOption( p_md->p_input_item, psz_option,
                          VLC_INPUT_OPTION_UNIQUE|VLC_INPUT_OPTION_TRUSTED );
}

/**************************************************************************
 * Same as libvlc_media_add_option but with configurable flags.
 **************************************************************************/
void libvlc_media_add_option_flag(
                                   libvlc_media_t * p_md,
                                   const char * ppsz_option,
                                   libvlc_media_option_t i_flags )
{
    input_item_AddOption( p_md->p_input_item, ppsz_option,
                          i_flags );
}

/**************************************************************************
 * Delete a media descriptor object
 **************************************************************************/
void libvlc_media_release( libvlc_media_t *p_md )
{
    if (!p_md)
        return;

    p_md->i_refcount--;

    if( p_md->i_refcount > 0 )
        return;

    if( p_md->p_subitems )
        libvlc_media_list_release( p_md->p_subitems );

    uninstall_input_item_observer( p_md );
    vlc_gc_decref( p_md->p_input_item );

    /* Construct the event */
    libvlc_event_t event;
    event.type = libvlc_MediaFreed;
    event.u.media_freed.md = p_md;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );

    libvlc_event_manager_release( p_md->p_event_manager );

    free( p_md );
}

/**************************************************************************
 * Retain a media descriptor object
 **************************************************************************/
void libvlc_media_retain( libvlc_media_t *p_md )
{
    assert (p_md);
    p_md->i_refcount++;
}

/**************************************************************************
 * Duplicate a media descriptor object
 **************************************************************************/
libvlc_media_t *
libvlc_media_duplicate( libvlc_media_t *p_md_orig )
{
    return libvlc_media_new_from_input_item(
        p_md_orig->p_libvlc_instance, p_md_orig->p_input_item, NULL );
}

/**************************************************************************
 * Get mrl from a media descriptor object
 **************************************************************************/
char *
libvlc_media_get_mrl( libvlc_media_t * p_md )
{
    assert( p_md );
    return input_item_GetURI( p_md->p_input_item );
}

/**************************************************************************
 * Getter for meta information
 **************************************************************************/

char *libvlc_media_get_meta( libvlc_media_t *p_md, libvlc_meta_t e_meta )
{
    char * psz_meta;

    assert( p_md );
    /* XXX: locking */

    preparse_if_needed( p_md );

    psz_meta = input_item_GetMeta( p_md->p_input_item,
                                   libvlc_to_vlc_meta[e_meta] );

    if( e_meta == libvlc_meta_ArtworkURL && !psz_meta )
    {
        playlist_AskForArtEnqueue(
                libvlc_priv(p_md->p_libvlc_instance->p_libvlc_int)->p_playlist,
                p_md->p_input_item, pl_Unlocked );
    }

    /* Should be integrated in core */
    if( !psz_meta && e_meta == libvlc_meta_Title && p_md->p_input_item->psz_name )
    {
        free( psz_meta );
        return strdup( p_md->p_input_item->psz_name );
    }

    return psz_meta;
}

/**************************************************************************
 * Setter for meta information
 **************************************************************************/

void libvlc_media_set_meta( libvlc_media_t *p_md, libvlc_meta_t e_meta, const char *psz_value )
{
    assert( p_md );
    input_item_SetMeta( p_md->p_input_item, libvlc_to_vlc_meta[e_meta], psz_value );
}

int libvlc_media_save_meta( libvlc_media_t *p_md )
{
    assert( p_md );
    vlc_object_t *p_obj = VLC_OBJECT(libvlc_priv(
                            p_md->p_libvlc_instance->p_libvlc_int)->p_playlist);
    return input_item_WriteMeta( p_obj, p_md->p_input_item ) == VLC_SUCCESS;
}

/**************************************************************************
 * Getter for state information
 * Can be error, playing, buffering, NothingSpecial.
 **************************************************************************/

libvlc_state_t
libvlc_media_get_state( libvlc_media_t *p_md )
{
    assert( p_md );
    return p_md->state;
}

/**************************************************************************
 * Setter for state information (LibVLC Internal)
 **************************************************************************/

void
libvlc_media_set_state( libvlc_media_t *p_md,
                                   libvlc_state_t state )
{
    libvlc_event_t event;

    p_md->state = state;

    /* Construct the event */
    event.type = libvlc_MediaStateChanged;
    event.u.media_state_changed.new_state = state;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );
}

/**************************************************************************
 * subitems
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_subitems( libvlc_media_t * p_md )
{
    if( p_md->p_subitems )
        libvlc_media_list_retain( p_md->p_subitems );
    return p_md->p_subitems;
}

/**************************************************************************
 * Getter for statistics information
 **************************************************************************/
int libvlc_media_get_stats( libvlc_media_t *p_md,
                            libvlc_media_stats_t *p_stats )
{
    if( !p_md->p_input_item )
        return false;

    input_stats_t *p_itm_stats = p_md->p_input_item->p_stats;
    vlc_mutex_lock( &p_itm_stats->lock );
    p_stats->i_read_bytes = p_itm_stats->i_read_bytes;
    p_stats->f_input_bitrate = p_itm_stats->f_input_bitrate;

    p_stats->i_demux_read_bytes = p_itm_stats->i_demux_read_bytes;
    p_stats->f_demux_bitrate = p_itm_stats->f_demux_bitrate;
    p_stats->i_demux_corrupted = p_itm_stats->i_demux_corrupted;
    p_stats->i_demux_discontinuity = p_itm_stats->i_demux_discontinuity;

    p_stats->i_decoded_video = p_itm_stats->i_decoded_video;
    p_stats->i_decoded_audio = p_itm_stats->i_decoded_audio;

    p_stats->i_displayed_pictures = p_itm_stats->i_displayed_pictures;
    p_stats->i_lost_pictures = p_itm_stats->i_lost_pictures;

    p_stats->i_played_abuffers = p_itm_stats->i_played_abuffers;
    p_stats->i_lost_abuffers = p_itm_stats->i_lost_abuffers;

    p_stats->i_sent_packets = p_itm_stats->i_sent_packets;
    p_stats->i_sent_bytes = p_itm_stats->i_sent_bytes;
    p_stats->f_send_bitrate = p_itm_stats->f_send_bitrate;
    vlc_mutex_unlock( &p_itm_stats->lock );
    return true;
}

/**************************************************************************
 * event_manager
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_event_manager( libvlc_media_t * p_md )
{
    assert( p_md );

    return p_md->p_event_manager;
}

/**************************************************************************
 * Get duration of media object (in ms)
 **************************************************************************/
int64_t
libvlc_media_get_duration( libvlc_media_t * p_md, libvlc_exception_t *p_e )
{
    assert( p_md );

    if( !p_md->p_input_item )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "No input item" );
        return -1;
    }

    return input_item_GetDuration( p_md->p_input_item ) / 1000;
}

/**************************************************************************
 * Get preparsed status for media object.
 **************************************************************************/
int
libvlc_media_is_preparsed( libvlc_media_t * p_md )
{
    assert( p_md );

    if( !p_md->p_input_item )
        return false;

    return input_item_IsPreparsed( p_md->p_input_item );
}

/**************************************************************************
 * Sets media descriptor's user_data. user_data is specialized data 
 * accessed by the host application, VLC.framework uses it as a pointer to 
 * an native object that references a libvlc_media_t pointer
 **************************************************************************/
void 
libvlc_media_set_user_data( libvlc_media_t * p_md, void * p_new_user_data )
{
    assert( p_md );
    p_md->p_user_data = p_new_user_data;
}

/**************************************************************************
 * Get media descriptor's user_data. user_data is specialized data 
 * accessed by the host application, VLC.framework uses it as a pointer to 
 * an native object that references a libvlc_media_t pointer
 **************************************************************************/
void *
libvlc_media_get_user_data( libvlc_media_t * p_md )
{
    assert( p_md );
    return p_md->p_user_data;
}
