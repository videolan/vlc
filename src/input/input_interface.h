/*****************************************************************************
 * input_interface.h: Input functions usable outside input code.
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * $Id$
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
void input_item_SetEpg( input_item_t *p_item, const vlc_epg_t *p_epg );
void input_item_SetEpgOffline( input_item_t * );

int input_Preparse( vlc_object_t *, input_item_t * );

/* misc/stats.c
 * FIXME it should NOT be defined here or not coded in misc/stats.c */
input_stats_t *stats_NewInputStats( input_thread_t *p_input );

/**
 * This function deletes the current sout in the resources.
 */
void input_resource_TerminateSout( input_resource_t *p_resource );

/**
 * This function return true if there is at least one vout in the resources.
 *
 * It can only be called on detached resources.
 */
bool input_resource_HasVout( input_resource_t *p_resource );

/* input.c */

/* */
typedef enum
{
    INPUT_STATISTIC_DECODED_VIDEO,
    INPUT_STATISTIC_DECODED_AUDIO,
    INPUT_STATISTIC_DECODED_SUBTITLE,

    /* Use them only if you do not goes through a access_out module */
    INPUT_STATISTIC_SENT_PACKET,
    INPUT_STATISTIC_SENT_BYTE,

} input_statistic_t;
/**
 * It will update internal input statistics from external sources.
 * XXX For now, the only one allowed to do it is stream_out and input core.
 */
void input_UpdateStatistic( input_thread_t *, input_statistic_t, int i_delta );

#endif
