/*****************************************************************************
 * input_interface.h: Input functions usable outside input code.
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#ifndef LIBVLC_INPUT_INTERFACE_H
#define LIBVLC_INPUT_INTERFACE_H 1

#include <vlc_input.h>
#include <libvlc.h>

/**********************************************************************
 * Item metadata
 **********************************************************************/
void input_item_SetPreparsed( input_item_t *p_i, bool b_preparsed );
void input_item_SetArtNotFound( input_item_t *p_i, bool b_not_found );
void input_item_SetArtFetched( input_item_t *p_i, bool b_art_fetched );
void input_item_SetEpg( input_item_t *p_item, const vlc_epg_t *p_epg, bool );
void input_item_ChangeEPGSource( input_item_t *p_item, int i_source_id );
void input_item_SetEpgEvent( input_item_t *p_item, const vlc_epg_event_t *p_epg_evt );
void input_item_SetEpgTime( input_item_t *, int64_t );
void input_item_SetEpgOffline( input_item_t * );

/**
 * This function deletes the current sout in the resources.
 */
void input_resource_TerminateSout( input_resource_t *p_resource );

#endif
