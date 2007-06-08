/*****************************************************************************
 * libvlc.h: Internal libvlc generic/misc declaration
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
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

#ifndef LIBVLC_LIBVLC_H
# define LIBVLC_LIBVLC_H 1

extern const char vlc_usage[];

extern const module_config_t libvlc_config[];
extern const size_t libvlc_config_count;

extern const struct hotkey libvlc_hotkeys[];
extern const size_t libvlc_hotkeys_size;

extern vlc_object_t *
vlc_custom_create (vlc_object_t *p_this, size_t i_size, int i_type,
                   const char *psz_type);

/*****************************************************************************
 * libvlc_global_data_t (global variable)
 *****************************************************************************
 * This structure has an unique instance, statically allocated in main and
 * never accessed from the outside. It stores once-initialized data such as
 * the CPU capabilities or the global lock.
 *****************************************************************************/
struct libvlc_global_data_t
{
    VLC_COMMON_MEMBERS

    vlc_bool_t             b_ready;     ///< Initialization boolean

   /* Object structure data */
    int                    i_counter;   ///< object counter
    int                    i_objects;   ///< Attached objects count
    vlc_object_t **        pp_objects;  ///< Array of all objects

    module_bank_t *        p_module_bank; ///< The module bank
    intf_thread_t         *p_probe;       ///< Devices prober

    /* Arch-specific variables */
#if !defined( WIN32 )
    vlc_bool_t             b_daemon;
#endif
#if defined( SYS_BEOS )
    vlc_object_t *         p_appthread;
    char *                 psz_vlcpath;
#elif defined( __APPLE__ )
    char *                 psz_vlcpath;
    vlc_iconv_t            iconv_macosx; /* for HFS+ file names */
    vlc_mutex_t            iconv_lock;
#elif defined( WIN32 )
    char *                 psz_vlcpath;
#endif
};

#define vlc_global( a ) __vlc_global( VLC_OBJECT( a ) )
static inline libvlc_global_data_t *__vlc_global( vlc_object_t *p_this )
{
    return (libvlc_global_data_t *)p_this->p_libvlc_global;
}

extern uint32_t cpu_flags;
uint32_t CPUCapabilities( void );

#endif
