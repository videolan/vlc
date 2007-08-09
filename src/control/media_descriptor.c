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

    p_md = malloc( sizeof(libvlc_media_descriptor_t) );
    p_md->p_libvlc_instance = p_instance;
    p_md->p_input_item      = p_input_item;
    p_md->b_preparsed       = VLC_FALSE;
    p_md->i_refcount        = 1;
 
    vlc_gc_incref( p_md->p_input_item );

    return p_md;
}

/**************************************************************************
 * Create a new media descriptor object from an input_item
 * (libvlc internal)
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

    vlc_gc_incref( p_md->p_input_item );

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

    /* XXX: locking */
    vlc_gc_decref( p_md->p_input_item );

    if( p_md->i_refcount > 0 )
        return;
    
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

    /* XXX: locking */
    vlc_gc_incref( p_md->p_input_item );
}

/**************************************************************************
 * Duplicate a media descriptor object
 **************************************************************************/
libvlc_media_descriptor_t *
libvlc_media_descriptor_duplicate( libvlc_media_descriptor_t *p_md_orig )
{
    libvlc_media_descriptor_t * p_md;

    p_md = malloc( sizeof(libvlc_media_descriptor_t) );
    memcpy( p_md, p_md_orig, sizeof(libvlc_media_descriptor_t) );

    vlc_gc_incref( p_md->p_input_item );

    return p_md;
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
static const int meta_conversion[] =
{
    [libvlc_meta_Title]  = 0, /* Offset in the vlc_meta_t structure */
    [libvlc_meta_Artist] = 1
};

char * libvlc_media_descriptor_get_meta( libvlc_media_descriptor_t *p_md,
                                         libvlc_meta_t e_meta,
                                         libvlc_exception_t *p_e )
{
    char ** ppsz_meta;
    char *ppz_meta;

    /* XXX: locking */

    preparse_if_needed( p_md );

    ppsz_meta = (char**)p_md->p_input_item->p_meta;
    ppz_meta = ppsz_meta[ meta_conversion[e_meta] ];

    if( !ppz_meta )
        return NULL;

    return strdup( ppz_meta );
}

