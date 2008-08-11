/*****************************************************************************
 * vlc_devices.h : Devices handling
 *****************************************************************************
 * Copyright (C) 1999-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef VLC_DEVICES_H
#define VLC_DEVICES_H 1

/**
 * \file
 * This file implements functions, structures for probing devices (DVD, CD, VCD)
 */

enum
{
    DEVICE_CAN_DVD,
    DEVICE_CAN_CD,
};

enum
{
    MEDIA_TYPE_CDDA,
    MEDIA_TYPE_VCD,
    MEDIA_TYPE_DVD,
};

struct device_t
{
    int             i_capabilities;
    int             i_media_type;
    bool      b_seen;
    char *psz_uri;
    char *psz_name;
};

struct device_probe_t
{
    VLC_COMMON_MEMBERS;
    int         i_devices;
    device_t  **pp_devices;

    probe_sys_t *p_sys;
    void      ( *pf_run )    ( device_probe_t * );  /** Run function */
};

static inline void device_GetDVD(void)
{
}

#endif
