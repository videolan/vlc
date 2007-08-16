/*****************************************************************************
 * media_descriptor.c: Libvlc API media descriport management
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

#include <vlc/libvlc.h>
#include <vlc_input.h>
#include <vlc_meta.h>

#include "libvlc_internal.h"

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
}

/**************************************************************************
 * Preparse if not already done (Private)
 **************************************************************************/
static void preparse_if_needed( libvlc_media_descriptor_t *p_md )
{
    /* XXX: need some locking here */
    if (!p_md->b_preparsed)
    {
        input_Preparse( p_md->p_libvlc_instance->p_libvlc_int,
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
    p_md->b_preparsed       = VLC_TRUE;
    p_md->i_refcount        = 1;
    p_md->p_event_manager = libvlc_event_manager_new( p_md, p_instance, p_e );

    libvlc_event_manager_register_event_type( p_md->p_event_manager, 
        libvlc_MediaDescriptorMetaChanged, p_e );
    libvlc_event_manager_register_event_type( p_md->p_event_manager, 
        libvlc_MediaDescriptorSubItemAdded, p_e );

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

    p_input_item = input_ItemNew( p_instance->p_libvlc_int, psz_mrl, psz_mrl );

    if (!p_input_item)
    {
        libvlc_exception_raise( p_e, "Can't create md's input_item" );
        return NULL;
    }

    p_md = libvlc_media_descriptor_new_from_input_item( p_instance,
                p_input_item, p_e );

    return p_md;
}

/**************************************************************************
 * Delete a media descriptor object
 **************************************************************************/
void libvlc_media_descriptor_release( libvlc_media_descriptor_t *p_md )
{
    if (!p_md)
        return;

    p_md->i_refcount--;

    if( p_md->i_refcount > 0 )
        return;

    uninstall_input_item_observer( p_md );
    vlc_gc_decref( p_md->p_input_item );

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
    return strdup( p_md->p_input_item->psz_uri );
}

/**************************************************************************
 * Getter for meta information
 **************************************************************************/

char * libvlc_media_descriptor_get_meta( libvlc_media_descriptor_t *p_md,
                                         libvlc_meta_t e_meta,
                                         libvlc_exception_t *p_e )
{
    const char * psz_meta;

    /* XXX: locking */

    preparse_if_needed( p_md );

    psz_meta = input_item_GetMeta( p_md->p_input_item,
                                   libvlc_to_vlc_meta[e_meta] );

    /* Should be integrated in core */
    if( !psz_meta && e_meta == libvlc_meta_Title && p_md->p_input_item->psz_name )
        return strdup( p_md->p_input_item->psz_name );

    if( !psz_meta )
        return NULL;

    return strdup( psz_meta );
}

