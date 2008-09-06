/*****************************************************************************
 * vlc_epg.h: Electronic Program Guide
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

} vlc_epg_event_t;

typedef struct
{
    char            *psz_name;
    vlc_epg_event_t *p_current; /* Can be null or should be the same than one of pp_event entry */

    int             i_event;
    vlc_epg_event_t **pp_event;
} vlc_epg_t;

static inline void vlc_epg_Init( vlc_epg_t *p_epg, const char *psz_name )
{
    p_epg->psz_name = psz_name ? strdup( psz_name ) : NULL;
    p_epg->p_current = NULL;
    TAB_INIT( p_epg->i_event, p_epg->pp_event );
}

static inline void vlc_epg_Clean( vlc_epg_t *p_epg )
{
    int i;
    for( i = 0; i < p_epg->i_event; i++ )
    {
        vlc_epg_event_t *p_evt = p_epg->pp_event[i];
        free( p_evt->psz_name );
        free( p_evt->psz_short_description );
        free( p_evt->psz_description );
        free( p_evt );
    }
    TAB_CLEAN( p_epg->i_event, p_epg->pp_event );
    free( p_epg->psz_name );
}

static inline void vlc_epg_AddEvent( vlc_epg_t *p_epg, int64_t i_start, int i_duration,
                                const char *psz_name, const char *psz_short_description, const char *psz_description )
{
    vlc_epg_event_t *p_evt = (vlc_epg_event_t*)malloc( sizeof(vlc_epg_event_t) );
    if( !p_evt )
        return;
    p_evt->i_start = i_start;
    p_evt->i_duration = i_duration;
    p_evt->psz_name = psz_name ? strdup( psz_name ) : NULL;
    p_evt->psz_short_description = psz_short_description ? strdup( psz_short_description ) : NULL;
    p_evt->psz_description = psz_description ? strdup( psz_description ) : NULL;
    TAB_APPEND_CPP( vlc_epg_event_t, p_epg->i_event, p_epg->pp_event, p_evt );
}

LIBVLC_USED
static inline vlc_epg_t *vlc_epg_New( const char *psz_name )
{
    vlc_epg_t *p_epg = (vlc_epg_t*)malloc( sizeof(vlc_epg_t) );
    if( p_epg )
        vlc_epg_Init( p_epg, psz_name );
    return p_epg;
}

static inline void vlc_epg_Delete( vlc_epg_t *p_epg )
{
    vlc_epg_Clean( p_epg );
    free( p_epg );
}

static inline void vlc_epg_SetCurrent( vlc_epg_t *p_epg, int64_t i_start )
{
    int i;
    p_epg->p_current = NULL;
    if( i_start < 0 )
        return;

    for( i = 0; i < p_epg->i_event; i++ )
    {
        if( p_epg->pp_event[i]->i_start == i_start )
        {
            p_epg->p_current = p_epg->pp_event[i];
            break;
        }
    }
}

#endif

