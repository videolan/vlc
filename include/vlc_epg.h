/*****************************************************************************
 * vlc_epg.h: Electronic Program Guide
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_EPG_H
#define VLC_EPG_H 1

/**
 * \file
 * This file defines functions and structures for storing dvb epg information
 */

typedef struct
{
    int64_t  i_start;    /* Interpreted as a value return by time() */
    uint32_t i_duration; /* Duration of the event in second */
    uint16_t i_id;       /* Unique event id withing the event set */

    char    *psz_name;
    char    *psz_short_description;
    char    *psz_description;
    struct               /* Description items in tranmission order */
    {
        char *psz_key;
        char *psz_value;
    } * description_items;
    int i_description_items;

    uint8_t i_rating;   /* Parental control, set to 0 when undefined */
} vlc_epg_event_t;

typedef struct
{
    char            *psz_name;
    uint32_t         i_id;       /* Unique identifier for this table / events (partial sets) */
    uint16_t         i_source_id;/* Channel / Program reference id this epg relates to */
    size_t            i_event;
    vlc_epg_event_t **pp_event;
    bool             b_present;  /* Contains present/following or similar, and sets below */
    const vlc_epg_event_t *p_current; /* NULL, or equal to one of the entries in pp_event */
} vlc_epg_t;

/**
 * Creates a new vlc_epg_event_t*
 *
 * You must call vlc_epg_event_Delete to release the associated resources.
 *
 * \p i_id is the event unique id
 * \p i_start start in epoch time
 * \p i_duration event duration in seconds
 */
VLC_API vlc_epg_event_t * vlc_epg_event_New(uint16_t i_id,
                                            int64_t i_start, uint32_t i_duration);

/**
 * Releases a vlc_epg_event_t*.
 */
VLC_API void vlc_epg_event_Delete(vlc_epg_event_t *p_event);

/**
 * Returns a vlc_epg_event_t * duplicated from \p p_src.
 *
 */
VLC_API vlc_epg_event_t * vlc_epg_event_Duplicate(const vlc_epg_event_t *p_src);

/**
 * It creates a new vlc_epg_t*
 *
 * You must call vlc_epg_Delete to release the associated resource.
 *
 * \p i_id is computed unique id depending on standard (table id, eit number)
 * \p i_source_id is the associated program number
 */
VLC_API vlc_epg_t * vlc_epg_New(uint32_t i_id, uint16_t i_source_id);

/**
 * It releases a vlc_epg_t*.
 */
VLC_API void vlc_epg_Delete(vlc_epg_t *p_epg);

/**
 * It appends a new vlc_epg_event_t to a vlc_epg_t.
 * Takes ownership of \p p_evt or returns false
 *
 * \p p_evt a vlc_epg_event_t * created with vlc_epg_event_New.
 */
VLC_API bool vlc_epg_AddEvent(vlc_epg_t *p_epg, vlc_epg_event_t *p_evt);

/**
 * It set the current event of a vlc_epg_t given a start time
 */
VLC_API void vlc_epg_SetCurrent(vlc_epg_t *p_epg, int64_t i_start);

/**
 * Returns a duplicated \p p_src and its associated events.
 *
 */
VLC_API vlc_epg_t * vlc_epg_Duplicate(const vlc_epg_t *p_src);

#endif

