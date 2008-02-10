/*****************************************************************************
 * media_descriptor.c: Libvlc API media descripor management
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

#include "libvlc_internal.h"

#include <vlc/libvlc.h>
#include <vlc_input.h>
#include <vlc_meta.h>

/* For the preparser */
#include <vlc_playlist.h>

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
    libvlc_media_descriptor_t * p_md = user_data;
    libvlc_media_descriptor_t * p_md_child;
    libvlc_event_t event;

    p_md_child = libvlc_media_descriptor_new_from_input_item(
                p_md->p_libvlc_instance,
                p_event->u.input_item_subitem_added.p_new_child, NULL );

    /* Add this to our media list */
    if( !p_md->p_subitems )
    {
        p_md->p_subitems = libvlc_media_list_new( p_md->p_libvlc_instance, NULL );
        libvlc_media_list_set_media_descriptor( p_md->p_subitems, p_md, NULL );
    }
    if( p_md->p_subitems )
    {
        libvlc_media_list_add_media_descriptor( p_md->p_subitems, p_md_child, NULL );
    }

    /* Construct the event */
    event.type = libvlc_MediaDescriptorSubItemAdded;
    event.u.media_descriptor_subitem_added.new_child = p_md_child;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );
    libvlc_media_descriptor_release( p_md_child );
}

/**************************************************************************
 * input_item_meta_changed (Private) (vlc event Callback)
 **************************************************************************/
static void input_item_meta_changed( const vlc_event_t *p_event,
                                     void * user_data )
{
    libvlc_media_descriptor_t * p_md = user_data;
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaDescriptorMetaChanged;
    event.u.media_descriptor_meta_changed.meta_type =
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
    libvlc_media_descriptor_t * p_md = user_data;
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaDescriptorDurationChanged;
    event.u.media_descriptor_duration_changed.new_duration = 
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
    libvlc_media_descriptor_t * p_md = user_data;
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaDescriptorPreparsedChanged;
    event.u.media_descriptor_preparsed_changed.new_status = 
        p_event->u.input_item_preparsed_changed.new_status;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );
}

/**************************************************************************
 * Install event handler (Private)
 **************************************************************************/
static void install_input_item_observer( libvlc_media_descriptor_t *p_md )
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
static void uninstall_input_item_observer( libvlc_media_descriptor_t *p_md )
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
static void preparse_if_needed( libvlc_media_descriptor_t *p_md )
{
    /* XXX: need some locking here */
    if (!p_md->b_preparsed)
    {
        playlist_PreparseEnqueue(
                p_md->p_libvlc_instance->p_libvlc_int->p_playlist,
                p_md->p_input_item );
        p_md->b_preparsed = VLC_TRUE;
    }
}

/**************************************************************************
 * Create a new media descriptor object from an input_item
 * (libvlc internal)
 * That's the generic constructor
 **************************************************************************/
libvlc_media_descriptor_t * libvlc_media_descriptor_new_from_input_item(
                                   libvlc_instance_t *p_instance,
                                   input_item_t *p_input_item,
                                   libvlc_exception_t *p_e )
{
    libvlc_media_descriptor_t * p_md;

    if (!p_input_item)
    {
        libvlc_exception_raise( p_e, "No input item given" );
        return NULL;
    }

    p_md = malloc( sizeof(libvlc_media_descriptor_t) );
    p_md->p_libvlc_instance = p_instance;
    p_md->p_input_item      = p_input_item;
    p_md->b_preparsed       = VLC_FALSE;
    p_md->i_refcount        = 1;
    p_md->p_user_data       = NULL;

    p_md->state = libvlc_NothingSpecial;

    /* A media descriptor can be a playlist. When you open a playlist
     * It can give a bunch of item to read. */
    p_md->p_subitems        = NULL;

    vlc_dictionary_init( &p_md->tags, 1 );

    p_md->p_event_manager = libvlc_event_manager_new( p_md, p_instance, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaDescriptorMetaChanged, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaDescriptorSubItemAdded, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaDescriptorFreed, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaDescriptorDurationChanged, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager,
        libvlc_MediaDescriptorStateChanged, p_e );

    vlc_gc_incref( p_md->p_input_item );

    install_input_item_observer( p_md );

    return p_md;
}

/**************************************************************************
 * Create a new media descriptor object
 **************************************************************************/
libvlc_media_descriptor_t * libvlc_media_descriptor_new(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_mrl,
                                   libvlc_exception_t *p_e )
{
    input_item_t * p_input_item;
    libvlc_media_descriptor_t * p_md;

    p_input_item = input_ItemNew( p_instance->p_libvlc_int, psz_mrl, NULL );

    if (!p_input_item)
    {
        libvlc_exception_raise( p_e, "Can't create md's input_item" );
        return NULL;
    }

    p_md = libvlc_media_descriptor_new_from_input_item( p_instance,
                p_input_item, p_e );

    /* The p_input_item is retained in libvlc_media_descriptor_new_from_input_item */
    vlc_gc_decref( p_input_item );
    
    return p_md;
}

/**************************************************************************
 * Create a new media descriptor object
 **************************************************************************/
libvlc_media_descriptor_t * libvlc_media_descriptor_new_as_node(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_name,
                                   libvlc_exception_t *p_e )
{
    input_item_t * p_input_item;
    libvlc_media_descriptor_t * p_md;

    p_input_item = input_ItemNew( p_instance->p_libvlc_int, "vlc:nop", psz_name );

    if (!p_input_item)
    {
        libvlc_exception_raise( p_e, "Can't create md's input_item" );
        return NULL;
    }

    p_md = libvlc_media_descriptor_new_from_input_item( p_instance,
                p_input_item, p_e );

    p_md->p_subitems = libvlc_media_list_new( p_md->p_libvlc_instance, NULL );

    return p_md;
}

/**************************************************************************
 * Add an option to the media descriptor,
 * that will be used to determine how the media_instance will read the
 * media_descriptor. This allow to use VLC advanced reading/streaming
 * options in a per-media basis
 *
 * The options are detailled in vlc --long-help, for instance "--sout-all"
 **************************************************************************/
void libvlc_media_descriptor_add_option(
                                   libvlc_media_descriptor_t * p_md,
                                   const char * ppsz_option,
                                   libvlc_exception_t *p_e )
{
    (void)p_e;
    input_ItemAddOptionNoDup( p_md->p_input_item, ppsz_option );
}

/**************************************************************************
 * Delete a media descriptor object
 **************************************************************************/
void libvlc_media_descriptor_release( libvlc_media_descriptor_t *p_md )
{
    int i;
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
    event.type = libvlc_MediaDescriptorFreed;
    event.u.media_descriptor_freed.md = p_md;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );

    libvlc_event_manager_release( p_md->p_event_manager );

    char ** all_keys = vlc_dictionary_all_keys( &p_md->tags );
    for( i = 0; all_keys[i]; i++ )
    {
        int j;
        struct libvlc_tags_storage_t * p_ts = vlc_dictionary_value_for_key( &p_md->tags, all_keys[i] );
        for( j = 0; j < p_ts->i_count; j++ )
        {
            free( p_ts->ppsz_tags[j] );
            free( p_ts->ppsz_tags );
        }
        free( p_ts );
    }
    vlc_dictionary_clear( &p_md->tags );
    free( p_md );
}

/**************************************************************************
 * Retain a media descriptor object
 **************************************************************************/
void libvlc_media_descriptor_retain( libvlc_media_descriptor_t *p_md )
{
    if (!p_md)
        return;

    p_md->i_refcount++;
}

/**************************************************************************
 * Duplicate a media descriptor object
 **************************************************************************/
libvlc_media_descriptor_t *
libvlc_media_descriptor_duplicate( libvlc_media_descriptor_t *p_md_orig )
{
    return libvlc_media_descriptor_new_from_input_item(
        p_md_orig->p_libvlc_instance, p_md_orig->p_input_item, NULL );
}

/**************************************************************************
 * Retain a media descriptor object
 **************************************************************************/
char *
libvlc_media_descriptor_get_mrl( libvlc_media_descriptor_t * p_md,
                                 libvlc_exception_t * p_e )
{
    (void)p_e;
    return input_item_GetURI( p_md->p_input_item );
}

/**************************************************************************
 * Getter for meta information
 **************************************************************************/

char * libvlc_media_descriptor_get_meta( libvlc_media_descriptor_t *p_md,
                                         libvlc_meta_t e_meta,
                                         libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    char * psz_meta;

    /* XXX: locking */

    preparse_if_needed( p_md );

    psz_meta = input_item_GetMeta( p_md->p_input_item,
                                   libvlc_to_vlc_meta[e_meta] );
    
    if( e_meta == libvlc_meta_ArtworkURL && !psz_meta )
    {
        playlist_AskForArtEnqueue(
                p_md->p_libvlc_instance->p_libvlc_int->p_playlist,
                p_md->p_input_item );
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
 * Getter for state information
 * Can be error, playing, buffering, NothingSpecial.
 **************************************************************************/

libvlc_state_t
libvlc_media_descriptor_get_state( libvlc_media_descriptor_t *p_md,
                                   libvlc_exception_t *p_e )
{
    (void)p_e;
    return p_md->state;
}

/**************************************************************************
 * Setter for state information (LibVLC Internal)
 **************************************************************************/

void
libvlc_media_descriptor_set_state( libvlc_media_descriptor_t *p_md,
                                   libvlc_state_t state,
                                   libvlc_exception_t *p_e )
{
    (void)p_e;
    libvlc_event_t event;

    p_md->state = state;

    /* Construct the event */
    event.type = libvlc_MediaDescriptorStateChanged;
    event.u.media_descriptor_state_changed.new_state = state;

    /* Send the event */
    libvlc_event_send( p_md->p_event_manager, &event );
}

/**************************************************************************
 * Add a tag
 **************************************************************************/
void libvlc_media_descriptor_add_tag( libvlc_media_descriptor_t *p_md,
                                      const char * key,
                                      const libvlc_tag_t tag,
                                      libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    struct libvlc_tags_storage_t * p_ts;

    if( !tag || !key )
        return;
 
    p_ts = vlc_dictionary_value_for_key( &p_md->tags, key );

    if( !p_ts )
    {
        p_ts = malloc(sizeof(struct libvlc_tags_storage_t));
        memset( p_ts, 0, sizeof(struct libvlc_tags_storage_t) );
    }
    p_ts->i_count++;

    if( !p_ts->ppsz_tags )
        p_ts->ppsz_tags = malloc(sizeof(char*)*(p_ts->i_count));
    else
        p_ts->ppsz_tags = realloc(p_ts->ppsz_tags, sizeof(char*)*(p_ts->i_count));
 
    p_ts->ppsz_tags[p_ts->i_count-1] = strdup( tag );
}


/**************************************************************************
 * Remove a tag
 **************************************************************************/
void libvlc_media_descriptor_remove_tag( libvlc_media_descriptor_t *p_md,
                                         const char * key,
                                         const libvlc_tag_t tag,
                                         libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    struct libvlc_tags_storage_t * p_ts;
    int i;

    if( !tag || !key )
        return;
 
    p_ts = vlc_dictionary_value_for_key( &p_md->tags, key );

    if( !p_ts )
        return;

    for( i = 0; i < p_ts->i_count; i++ )
    {
        if( !strcmp( p_ts->ppsz_tags[i], tag ) )
        {
            free( p_ts->ppsz_tags[i] );
            memcpy( p_ts->ppsz_tags + i + 1, p_ts->ppsz_tags + i, (p_ts->i_count - i - 2)*sizeof(char*) );
            /* Don't dealloc, the memory will be regain if we add a new tag */
            p_ts->i_count--;
            return;
        }
    }
}

/**************************************************************************
 * Get tags count
 **************************************************************************/
int libvlc_media_descriptor_tags_count_for_key( libvlc_media_descriptor_t *p_md,
                                                 const char * key,
                                                 libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    struct libvlc_tags_storage_t * p_ts;

    if( !key )
        return 0;
 
    p_ts = vlc_dictionary_value_for_key( &p_md->tags, key );

    if( !p_ts )
        return 0;
    return p_ts->i_count;
}

/**************************************************************************
 * Get a tag
 **************************************************************************/
libvlc_tag_t
libvlc_media_descriptor_tag_at_index_for_key( libvlc_media_descriptor_t *p_md,
                                              int i,
                                              const char * key,
                                              libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    struct libvlc_tags_storage_t * p_ts;

    if( !key )
        return NULL;
 
    p_ts = vlc_dictionary_value_for_key( &p_md->tags, key );

    if( !p_ts )
        return NULL;
 
    return strdup( p_ts->ppsz_tags[i] );
}

/**************************************************************************
 * subitems
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_descriptor_subitems( libvlc_media_descriptor_t * p_md,
                                  libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    if( p_md->p_subitems )
        libvlc_media_list_retain( p_md->p_subitems );
    return p_md->p_subitems;
}

/**************************************************************************
 * event_manager
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_descriptor_event_manager( libvlc_media_descriptor_t * p_md,
                                       libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    return p_md->p_event_manager;
}

/**************************************************************************
 * Get duration of media_descriptor object.
 **************************************************************************/
vlc_int64_t
libvlc_media_descriptor_get_duration( libvlc_media_descriptor_t * p_md,
                                      libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    if( p_md && p_md->p_input_item)
    {
        return input_item_GetDuration( p_md->p_input_item );
    }
    else
    {
        return -1;
    }
}

/**************************************************************************
 * Get preparsed status for media_descriptor object.
 **************************************************************************/
vlc_bool_t
libvlc_media_descriptor_is_preparsed( libvlc_media_descriptor_t * p_md,
                                       libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    if( p_md && p_md->p_input_item)
    {
        return input_item_IsPreparsed( p_md->p_input_item );
    }
    else
    {
        return VLC_FALSE;
    }
}

/**************************************************************************
 * Sets media descriptor's user_data. user_data is specialized data 
 * accessed by the host application, VLC.framework uses it as a pointer to 
 * an native object that references a libvlc_media_descriptor_t pointer
 **************************************************************************/
void 
libvlc_media_descriptor_set_user_data( libvlc_media_descriptor_t * p_md,
                                       void * p_new_user_data,
                                       libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    if( p_md )
    {
        p_md->p_user_data = p_new_user_data;
    }
}

/**************************************************************************
 * Get media descriptor's user_data. user_data is specialized data 
 * accessed by the host application, VLC.framework uses it as a pointer to 
 * an native object that references a libvlc_media_descriptor_t pointer
 **************************************************************************/
void *
libvlc_media_descriptor_get_user_data( libvlc_media_descriptor_t * p_md,
                                       libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    if( p_md )
    {
        return p_md->p_user_data;
    }
    else
    {
        return NULL;
    }
}

