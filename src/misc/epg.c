/*****************************************************************************
 * epg.c: Electronic Program Guide
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_epg.h>

void vlc_epg_Init( vlc_epg_t *p_epg, const char *psz_name )
{
    p_epg->psz_name = psz_name ? strdup( psz_name ) : NULL;
    p_epg->p_current = NULL;
    TAB_INIT( p_epg->i_event, p_epg->pp_event );
}

void vlc_epg_Clean( vlc_epg_t *p_epg )
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

void vlc_epg_AddEvent( vlc_epg_t *p_epg, int64_t i_start, int i_duration,
                       const char *psz_name, const char *psz_short_description,
                       const char *psz_description, uint8_t i_rating )
{
    vlc_epg_event_t *p_evt = malloc( sizeof(*p_evt) );
    if( !p_evt )
        return;
    p_evt->i_start = i_start;
    p_evt->i_duration = i_duration;
    p_evt->psz_name = psz_name ? strdup( psz_name ) : NULL;
    p_evt->psz_short_description = psz_short_description ? strdup( psz_short_description ) : NULL;
    p_evt->psz_description = psz_description ? strdup( psz_description ) : NULL;
    p_evt->i_rating = i_rating;
    TAB_APPEND( p_epg->i_event, p_epg->pp_event, p_evt );
}

vlc_epg_t *vlc_epg_New( const char *psz_name )
{
    vlc_epg_t *p_epg = malloc( sizeof(*p_epg) );
    if( p_epg )
        vlc_epg_Init( p_epg, psz_name );
    return p_epg;
}

void vlc_epg_Delete( vlc_epg_t *p_epg )
{
    vlc_epg_Clean( p_epg );
    free( p_epg );
}

void vlc_epg_SetCurrent( vlc_epg_t *p_epg, int64_t i_start )
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

void vlc_epg_Merge( vlc_epg_t *p_dst, const vlc_epg_t *p_src )
{
    int i;

    /* Add new event */
    for( i = 0; i < p_src->i_event; i++ )
    {
        vlc_epg_event_t *p_evt = p_src->pp_event[i];
        bool b_add = true;
        int j;

        for( j = 0; j < p_dst->i_event; j++ )
        {
            if( p_dst->pp_event[j]->i_start == p_evt->i_start && p_dst->pp_event[j]->i_duration == p_evt->i_duration )
            {
                b_add = false;
                break;
            }
            if( p_dst->pp_event[j]->i_start > p_evt->i_start )
                break;
        }
        if( b_add )
        {
            vlc_epg_event_t *p_copy = calloc( 1, sizeof(vlc_epg_event_t) );
            if( !p_copy )
                break;
            p_copy->i_start = p_evt->i_start;
            p_copy->i_duration = p_evt->i_duration;
            p_copy->psz_name = p_evt->psz_name ? strdup( p_evt->psz_name ) : NULL;
            p_copy->psz_short_description = p_evt->psz_short_description ? strdup( p_evt->psz_short_description ) : NULL;
            p_copy->psz_description = p_evt->psz_description ? strdup( p_evt->psz_description ) : NULL;
            p_copy->i_rating = p_evt->i_rating;
            TAB_INSERT( p_dst->i_event, p_dst->pp_event, p_copy, j );
        }
    }
    /* Update current */
    if( p_src->p_current )
        vlc_epg_SetCurrent( p_dst, p_src->p_current->i_start );

    /* Keep only 1 old event  */
    if( p_dst->p_current )
    {
        while( p_dst->i_event > 1 && p_dst->pp_event[0] != p_dst->p_current && p_dst->pp_event[1] != p_dst->p_current )
            TAB_REMOVE( p_dst->i_event, p_dst->pp_event, p_dst->pp_event[0] );
    }
}

