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
    int64_t i_start;    /* Interpreted as a value return by time() */
    int     i_duration;    /* Duration of the event in second */

    char    *psz_name;
    char    *psz_short_description;
    char    *psz_description;

    uint8_t i_rating;   /* Parental control, set to 0 when undefined */
} vlc_epg_event_t;

typedef struct
{
    char            *psz_name;
    vlc_epg_event_t *p_current; /* Can be null or should be the same than one of pp_event entry */

    int             i_event;
    vlc_epg_event_t **pp_event;
} vlc_epg_t;

/**
 * It initializes a vlc_epg_t.
 *
 * You must call vlc_epg_Clean to release the associated resource.
 */
VLC_API void vlc_epg_Init(vlc_epg_t *p_epg, const char *psz_name);

/**
 * It releases all resources associated to a vlc_epg_t
 */
VLC_API void vlc_epg_Clean(vlc_epg_t *p_epg);

/**
 * It creates and appends a new vlc_epg_event_t to a vlc_epg_t.
 *
 * \see vlc_epg_t for the definitions of the parameters.
 */
VLC_API void vlc_epg_AddEvent(vlc_epg_t *p_epg, int64_t i_start, int i_duration, const char *psz_name, const char *psz_short_description, const char *psz_description, uint8_t i_rating );

/**
 * It creates a new vlc_epg_t*
 *
 * You must call vlc_epg_Delete to release the associated resource.
 */
VLC_API vlc_epg_t * vlc_epg_New(const char *psz_name) VLC_USED;

/**
 * It releases a vlc_epg_t*.
 */
VLC_API void vlc_epg_Delete(vlc_epg_t *p_epg);

/**
 * It set the current event of a vlc_epg_t given a start time
 */
VLC_API void vlc_epg_SetCurrent(vlc_epg_t *p_epg, int64_t i_start);

/**
 * It merges all the event of \p p_src and \p p_dst into \p p_dst.
 *
 * \p p_src is not modified.
 */
VLC_API void vlc_epg_Merge(vlc_epg_t *p_dst, const vlc_epg_t *p_src);

#endif

