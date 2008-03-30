/*****************************************************************************
 * main.h: access to all program variables
 * Declaration and extern access to global program object.
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 the VideoLAN team
 * $Id$
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

TYPEDEF_ARRAY(input_item_t*, input_item_array_t);

/*****************************************************************************
 * libvlc_internal_instance_t
 *****************************************************************************
 * This structure is a LibVLC instance, for use by libvlc core and plugins
 *****************************************************************************/
struct libvlc_int_t
{
    VLC_COMMON_MEMBERS

    /* Global properties */
    char *                 psz_homedir;      ///< user's home directory
    char *                 psz_configdir;    ///< user's configuration directory
    char *                 psz_datadir;      ///< user's data directory
    char *                 psz_cachedir;     ///< user's cache directory

    char *                 psz_configfile;   ///< location of config file

    playlist_t            *p_playlist;       ///< playlist object

    vlc_object_t          *p_interaction;    ///< interface interaction object

    vlc_object_t          *p_vlm;            ///< vlm if created from libvlc-common.
                                             /// (this is clearly private and
                                             //  shouldn't be used)

    void                 *p_stats_computer;  ///< Input thread computing stats (needs cleanup)
    global_stats_t       *p_stats;           ///< Global statistics

    /* There is no real reason to keep a list of items, but not to break
     * everything, let's keep it */
    input_item_array_t    input_items;       ///< Array of all created input items
    int                   i_last_input_id ;  ///< Last id of input item

    /* Messages */
    msg_bank_t             msg_bank;    ///< The message bank
    int                    i_verbose;   ///< info messages
    vlc_bool_t             b_color;     ///< color messages?

    module_t *             p_memcpy_module;  ///< Fast memcpy plugin used
    void* ( *pf_memcpy ) ( void *, const void *, size_t ); ///< fast memcpy
    void* ( *pf_memset ) ( void *, int, size_t );          ///< fast memset

    vlc_bool_t             b_stats;       ///< Should we collect stats ?
    vlc_mutex_t            timer_lock;    ///< Lock to protect timers
    int                    i_timers;      ///< Number of timers
    counter_t            **pp_timers;     ///< Array of all timers

    vlc_mutex_t            config_lock;    ///< Lock for the config file
#ifdef __APPLE__
    vlc_mutex_t            quicktime_lock; ///< QT is not thread safe on OSX
#endif

    /* Structure storing the action name / key associations */
    struct hotkey
    {
        const char *psz_action;
        int i_action;
        int i_key;
    } *p_hotkeys;
};

