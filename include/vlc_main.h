/*****************************************************************************
 * main.h: access to all program variables
 * Declaration and extern access to LibVLC instance object.
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002, 2008 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

/**
 * \file
 * This file defines libvlc_int_t internal libvlc instance
 */

TYPEDEF_ARRAY(input_item_t*, input_item_array_t);

/*****************************************************************************
 * libvlc_internal_instance_t
 *****************************************************************************
 * This structure is a LibVLC instance, for use by libvlc core and plugins
 *****************************************************************************/
struct libvlc_int_t
{
    VLC_COMMON_MEMBERS

    /* FIXME: this is only used by the logger module! */
    global_stats_t       *p_stats;           ///< Global statistics

    /* Structure storing the action name / key associations */
    struct hotkey
    {
        const char *psz_action;
        int i_action;
        int i_key;
    } *p_hotkeys;
};

