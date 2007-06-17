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
 * Create a new media descriptor object (Private)
 **************************************************************************/
static void preparse_if_needed( libvlc_media_descriptor_t *p_media_desc )
{
    /* XXX: need some locking here */
    if (!p_media_desc->b_preparsed)
    {
        input_Preparse( p_media_desc->p_libvlc_instance->p_libvlc_int,
                        p_media_desc->p_input_item );
        p_media_desc->b_preparsed = VLC_TRUE;
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
    libvlc_media_descriptor_t * p_media_desc;

    p_input_item = input_ItemNew( p_instance->p_libvlc_int, psz_mrl, psz_mrl );

    if (!p_input_item)
        return NULL; /* XXX: throw an exception */

    p_media_desc = malloc( sizeof(libvlc_input_t) );
    p_media_desc->p_libvlc_instance = p_instance;
    p_media_desc->p_input_item      = p_input_item;
    p_media_desc->b_preparsed       = VLC_FALSE;

    return p_media_desc;
}

/**************************************************************************
 * Delete a media descriptor object
 **************************************************************************/
void libvlc_media_descriptor_destroy( libvlc_media_descriptor_t *p_meta_desc )
{
    if (!p_meta_desc)
        return;

    /* XXX: locking */
    input_ItemClean( p_meta_desc->p_input_item );

    free( p_meta_desc );
}

/**************************************************************************
 * Getters for meta information
 **************************************************************************/
static const int meta_conversion[] =
{
    [libvlc_meta_Title]  = 0, /* Offset in the vlc_meta_t structure */
    [libvlc_meta_Artist] = 1
};

char * libvlc_media_descriptor_get_meta( libvlc_media_descriptor_t *p_meta_desc,
                                         libvlc_meta_t e_meta,
                                         libvlc_exception_t *p_e )
{
    char ** ppsz_meta;

    /* XXX: locking */

    preparse_if_needed( p_meta_desc );

    ppsz_meta = (char**)p_meta_desc->p_input_item->p_meta;

    return strdup( ppsz_meta[ meta_conversion[e_meta] ] );
}
