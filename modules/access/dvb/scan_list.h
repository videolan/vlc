/*****************************************************************************
 * scan_list.h : Scanning parameters and transponders list
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
#ifndef VLC_SCAN_LIST_H
#define VLC_SCAN_LIST_H

typedef struct scan_list_entry_t scan_list_entry_t;

typedef struct scan_list_entry_t
{
    char *psz_channel;
    int   i_service;
    int   i_stream_id;
    unsigned i_freq;
    unsigned i_bw;

    unsigned i_rate;
    uint8_t  i_fec;
    enum
    {
        POLARIZATION_VERTICAL = 0,
        POLARIZATION_HORIZONTAL
    } polarization;

    enum
    {
        DELIVERY_UNKNOWN = 0,
        DELIVERY_DVBT,
        DELIVERY_DVBT2,
        DELIVERY_DVBS,
        DELIVERY_DVBS2,
        DELIVERY_DVBC,
        DELIVERY_ISDBT,
    } delivery;

    scan_list_entry_t *p_next;

} scan_list_entry_t;

scan_list_entry_t * scan_list_dvbv5_load( vlc_object_t *, const char *, size_t * );
scan_list_entry_t * scan_list_dvbv3_load( vlc_object_t *, const char *, size_t * );

void scan_list_entries_release( scan_list_entry_t * );

#endif
