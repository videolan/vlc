/*****************************************************************************
 * tables.h
 *****************************************************************************
 * Copyright (C) 2001-2005, 2015 VLC authors and VideoLAN
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
#ifndef _TABLES_H
#define _TABLES_H 1

#define MAX_SDT_DESC 64

typedef struct
{
    ts_stream_t ts;
    int i_netid;
    struct
    {
        char *psz_provider;
        char *psz_service_name;  /* name of program */
    } desc[MAX_SDT_DESC];
} sdt_psi_t;

#endif
