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

static void vlc_epg_Event_Delete( vlc_epg_event_t *p_evt )
{
    free( p_evt->psz_name );
    free( p_evt->psz_short_description );
    free( p_evt->psz_description );
    free( p_evt );
}

static vlc_epg_event_t * vlc_epg_Event_New( int64_t i_start, int i_duration,
                                            const char *psz_name, const char *psz_short_description,
                                            const char *psz_description, uint8_t i_rating )
{
    vlc_epg_event_t *p_evt = malloc( sizeof(*p_evt) );
    if( likely(p_evt) )
    {
        p_evt->i_start = i_start;
        p_evt->i_duration = i_duration;
        p_evt->psz_name = psz_name ? strdup( psz_name ) : NULL;
        p_evt->psz_short_description = psz_short_description ? strdup( psz_short_description ) : NULL;
        p_evt->psz_description = psz_description ? strdup( psz_description ) : NULL;
        p_evt->i_rating = i_rating;
    }
    return p_evt;
}

static inline vlc_epg_event_t * vlc_epg_Event_Duplicate( const vlc_epg_event_t *p_evt )
{
    return vlc_epg_Event_New( p_evt->i_start, p_evt->i_duration,
                              p_evt->psz_name, p_evt->psz_short_description,
                              p_evt->psz_description, p_evt->i_rating );
}

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
        vlc_epg_Event_Delete( p_epg->pp_event[i] );
    TAB_CLEAN( p_epg->i_event, p_epg->pp_event );
    free( p_epg->psz_name );
}

void vlc_epg_AddEvent( vlc_epg_t *p_epg, int64_t i_start, int i_duration,
                       const char *psz_name, const char *psz_short_description,
                       const char *psz_description, uint8_t i_rating )
{
    vlc_epg_event_t *p_evt = vlc_epg_Event_New( i_start, i_duration,
                                                psz_name, psz_short_description,
                                                psz_description, i_rating );
    if( unlikely(!p_evt) )
        return;

    int i_pos = -1;

    /* Insertions are supposed in sequential order first */
    if( p_epg->i_event )
    {
        if( p_epg->pp_event[0]->i_start > i_start )
        {
            i_pos = 0;
        }
        else if ( p_epg->pp_event[p_epg->i_event - 1]->i_start >= i_start )
        {
            /* Do bisect search lower start time entry */
            int i_lower = 0;
            int i_upper = p_epg->i_event - 1;

            while( i_lower < i_upper )
            {
                int i_split = ( (size_t)i_lower + i_upper ) / 2;
                vlc_epg_event_t *p_cur = p_epg->pp_event[i_split];

                if( p_cur->i_start < i_start )
                {
                    i_lower = i_split + 1;
                }
                else if ( p_cur->i_start >= i_start )
                {
                    i_upper = i_split;
                }
            }
            i_pos = i_lower;
        }
    }

    if( i_pos != -1 )
    {
        if( p_epg->pp_event[i_pos]->i_start == i_start )/* There can be only one event at same time */
        {
            vlc_epg_Event_Delete( p_epg->pp_event[i_pos] );
            if( p_epg->p_current == p_epg->pp_event[i_pos] )
                p_epg->p_current = p_evt;
            p_epg->pp_event[i_pos] = p_evt;
            return;
        }
        else
        {
            TAB_INSERT( p_epg->i_event, p_epg->pp_event, p_evt, i_pos );
        }
    }
    else
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

static void vlc_epg_Prune( vlc_epg_t *p_dst )
{
    /* Keep only 1 old event  */
    if( p_dst->p_current )
    {
        while( p_dst->i_event > 1 && p_dst->pp_event[0] != p_dst->p_current && p_dst->pp_event[1] != p_dst->p_current )
        {
            vlc_epg_Event_Delete( p_dst->pp_event[0] );
            TAB_ERASE( p_dst->i_event, p_dst->pp_event, 0 );
        }
    }
}

void vlc_epg_Merge( vlc_epg_t *p_dst_epg, const vlc_epg_t *p_src_epg )
{
    if( p_src_epg->i_event == 0 )
        return;

    int i_dst=0;
    int i_src=0;
    for( ; i_src < p_src_epg->i_event; i_src++ )
    {
        bool b_current = ( p_src_epg->pp_event[i_src] == p_src_epg->p_current );

        vlc_epg_event_t *p_src = vlc_epg_Event_Duplicate( p_src_epg->pp_event[i_src] );
        if( unlikely(!p_src) )
            return;
        const int64_t i_src_end = p_src->i_start + p_src->i_duration;

        while( i_dst < p_dst_epg->i_event )
        {
            vlc_epg_event_t *p_dst = p_dst_epg->pp_event[i_dst];
            const int64_t i_dst_end = p_dst->i_start + p_dst->i_duration;

            /* appended is before current, no overlap */
            if( p_dst->i_start >= i_src_end )
            {
                break;
            }
            /* overlap case: appended would contain current's start (or are identical) */
            else if( ( p_dst->i_start >= p_src->i_start && p_dst->i_start < i_src_end ) ||
            /* overlap case: appended would contain current's end */
                    ( i_dst_end > p_src->i_start && i_dst_end <= i_src_end ) )
            {
                vlc_epg_Event_Delete( p_dst );
                if( p_dst_epg->p_current == p_dst )
                {
                    b_current |= true;
                    p_dst_epg->p_current = NULL;
                }
                TAB_ERASE( p_dst_epg->i_event, p_dst_epg->pp_event, i_dst );
            }
            else
            {
                i_dst++;
            }
        }

        TAB_INSERT( p_dst_epg->i_event, p_dst_epg->pp_event, p_src, i_dst );
        if( b_current )
            p_dst_epg->p_current = p_src;
    }

    /* Remaining/trailing ones */
    for( ; i_src < p_src_epg->i_event; i_src++ )
    {
        vlc_epg_event_t *p_src = vlc_epg_Event_Duplicate( p_src_epg->pp_event[i_src] );
        if( unlikely(!p_src) )
            return;
        TAB_APPEND( p_dst_epg->i_event, p_dst_epg->pp_event, p_src );
        if( p_src_epg->pp_event[i_src] == p_src_epg->p_current )
            p_dst_epg->p_current = p_src;
    }

    vlc_epg_Prune( p_dst_epg );
}
