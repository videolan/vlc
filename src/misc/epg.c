/*****************************************************************************
 * epg.c: Electronic Program Guide
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
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

static void vlc_epg_event_Clean(vlc_epg_event_t *p_event)
{
    for(int i=0; i<p_event->i_description_items; i++)
    {
        free(p_event->description_items[i].psz_key);
        free(p_event->description_items[i].psz_value);
    }
    free(p_event->description_items);
    free(p_event->psz_description);
    free(p_event->psz_short_description);
    free(p_event->psz_name);
}

void vlc_epg_event_Delete(vlc_epg_event_t *p_event)
{
    vlc_epg_event_Clean(p_event);
    free(p_event);
}

static void vlc_epg_event_Init(vlc_epg_event_t *p_event, uint16_t i_id,
                               int64_t i_start, uint32_t i_duration)
{
    memset(p_event, 0, sizeof(*p_event));
    p_event->i_start = i_start;
    p_event->i_id = i_id;
    p_event->i_duration = i_duration;
    p_event->i_description_items = 0;
    p_event->description_items = NULL;
}

vlc_epg_event_t * vlc_epg_event_New(uint16_t i_id,
                                    int64_t i_start, uint32_t i_duration)
{
    vlc_epg_event_t *p_event = (vlc_epg_event_t *) malloc(sizeof(*p_event));
    if(p_event)
        vlc_epg_event_Init(p_event, i_id, i_start, i_duration);

    return p_event;
}

vlc_epg_event_t * vlc_epg_event_Duplicate( const vlc_epg_event_t *p_src )
{
    vlc_epg_event_t *p_evt = vlc_epg_event_New( p_src->i_id, p_src->i_start,
                                                p_src->i_duration );
    if( likely(p_evt) )
    {
        if( p_src->psz_description )
            p_evt->psz_description = strdup( p_src->psz_description );
        if( p_src->psz_name )
            p_evt->psz_name = strdup( p_src->psz_name );
        if( p_src->psz_short_description )
            p_evt->psz_short_description = strdup( p_src->psz_short_description );
        if( p_src->i_description_items )
        {
            p_evt->description_items = malloc( sizeof(*p_evt->description_items) *
                                               p_src->i_description_items );
            if( p_evt->description_items )
            {
                for( int i=0; i<p_src->i_description_items; i++ )
                {
                    p_evt->description_items[i].psz_key =
                            strdup( p_src->description_items[i].psz_key );
                    p_evt->description_items[i].psz_value =
                            strdup( p_src->description_items[i].psz_value );
                    if(!p_evt->description_items[i].psz_value ||
                       !p_evt->description_items[i].psz_key)
                    {
                        free(p_evt->description_items[i].psz_key);
                        free(p_evt->description_items[i].psz_value);
                        break;
                    }
                    p_evt->i_description_items++;
                }
            }
        }
        p_evt->i_rating = p_src->i_rating;
    }
    return p_evt;
}

static void vlc_epg_Init( vlc_epg_t *p_epg, uint32_t i_id, uint16_t i_source_id )
{
    p_epg->i_id = i_id;
    p_epg->i_source_id = i_source_id;
    p_epg->psz_name = NULL;
    p_epg->p_current = NULL;
    p_epg->b_present = false;
    TAB_INIT( p_epg->i_event, p_epg->pp_event );
}

static void vlc_epg_Clean( vlc_epg_t *p_epg )
{
    size_t i;
    for( i = 0; i < p_epg->i_event; i++ )
        vlc_epg_event_Delete( p_epg->pp_event[i] );
    TAB_CLEAN( p_epg->i_event, p_epg->pp_event );
    free( p_epg->psz_name );
}

bool vlc_epg_AddEvent( vlc_epg_t *p_epg, vlc_epg_event_t *p_evt )
{
    ssize_t i_pos = -1;

    /* Insertions are supposed in sequential order first */
    if( p_epg->i_event )
    {
        if( p_epg->pp_event[0]->i_start > p_evt->i_start )
        {
            i_pos = 0;
        }
        else if ( p_epg->pp_event[p_epg->i_event - 1]->i_start >= p_evt->i_start )
        {
            /* Do bisect search lower start time entry */
            size_t i_lower = 0;
            size_t i_upper = p_epg->i_event - 1;

            while( i_lower < i_upper )
            {
                size_t i_split = ( i_lower + i_upper ) / 2;
                vlc_epg_event_t *p_cur = p_epg->pp_event[i_split];

                if( p_cur->i_start < p_evt->i_start )
                {
                    i_lower = i_split + 1;
                }
                else if ( p_cur->i_start >= p_evt->i_start )
                {
                    i_upper = i_split;
                }
            }
            i_pos = i_lower;
        }
    }

    if( i_pos != -1 )
    {
        /* There can be only one event at same time */
        if( p_epg->pp_event[i_pos]->i_start == p_evt->i_start )
        {
            vlc_epg_event_Delete( p_epg->pp_event[i_pos] );
            if( p_epg->p_current == p_epg->pp_event[i_pos] )
                p_epg->p_current = p_evt;
            p_epg->pp_event[i_pos] = p_evt;
            return true;
        }
        else
        {
            TAB_INSERT( p_epg->i_event, p_epg->pp_event, p_evt, i_pos );
        }
    }
    else
        TAB_APPEND( p_epg->i_event, p_epg->pp_event, p_evt );

    return true;
}

vlc_epg_t *vlc_epg_New( uint32_t i_id, uint16_t i_source_id )
{
    vlc_epg_t *p_epg = malloc( sizeof(*p_epg) );
    if( p_epg )
        vlc_epg_Init( p_epg, i_id, i_source_id );
    return p_epg;
}

void vlc_epg_Delete( vlc_epg_t *p_epg )
{
    vlc_epg_Clean( p_epg );
    free( p_epg );
}

void vlc_epg_SetCurrent( vlc_epg_t *p_epg, int64_t i_start )
{
    size_t i;
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

vlc_epg_t * vlc_epg_Duplicate( const vlc_epg_t *p_src )
{
    vlc_epg_t *p_epg = vlc_epg_New( p_src->i_id, p_src->i_source_id );
    if( p_epg )
    {
        p_epg->psz_name = ( p_src->psz_name ) ? strdup( p_src->psz_name ) : NULL;
        p_epg->b_present = p_src->b_present;
        for( size_t i=0; i<p_src->i_event; i++ )
        {
            vlc_epg_event_t *p_dup = vlc_epg_event_Duplicate( p_src->pp_event[i] );
            if( p_dup )
            {
                if( p_src->p_current == p_src->pp_event[i] )
                    p_epg->p_current = p_dup;
                TAB_APPEND( p_epg->i_event, p_epg->pp_event, p_dup );
            }
        }
    }
    return p_epg;
}
