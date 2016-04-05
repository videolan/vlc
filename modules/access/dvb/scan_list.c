/*****************************************************************************
 * scan_list.c : Scanning parameters and transponders list
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_fs.h>

#include "scan_list.h"

static scan_list_entry_t * scan_list_entry_New()
{
    scan_list_entry_t *p_entry = calloc(1, sizeof(scan_list_entry_t));
    if( likely(p_entry) )
    {
        p_entry->i_service = -1;
        p_entry->i_stream_id = -1;
    }
    return p_entry;
}

static void scan_list_entry_Delete( scan_list_entry_t *p_entry )
{
    free( p_entry->psz_channel );
    free( p_entry );
}

static bool scan_list_entry_validate( const scan_list_entry_t *p_entry )
{
    switch( p_entry->delivery )
    {
        case DELIVERY_DVBS:
        case DELIVERY_DVBS2:
            return p_entry->i_freq && p_entry->i_rate && p_entry->i_fec;

        case DELIVERY_DVBT:
        case DELIVERY_DVBT2:
        case DELIVERY_ISDBT:
            return p_entry->i_freq && p_entry->i_bw;

        case DELIVERY_DVBC:
            return p_entry->i_freq && p_entry->i_rate;

        case DELIVERY_UNKNOWN:
        default:
            break;
    }
    return false;
}

static bool scan_list_entry_add( scan_list_entry_t ***ppp_last, scan_list_entry_t *p_entry )
{
    if( scan_list_entry_validate( p_entry ) )
    {
         **ppp_last = p_entry;
         *ppp_last = &p_entry->p_next;
        return true;
    }

    scan_list_entry_Delete( p_entry );
    return false;
}

void scan_list_entries_release( scan_list_entry_t *p_list )
{
    while( p_list )
    {
        scan_list_entry_t *p_next = p_list->p_next;
        scan_list_entry_Delete( p_list );
        p_list = p_next;
    }
}
