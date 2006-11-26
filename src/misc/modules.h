/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id: modules.h 17958 2006-11-22 17:13:24Z courmisch $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/* Number of tries before we unload an unused module */
#define MODULE_HIDE_DELAY 50

/*****************************************************************************
 * module_bank_t: the module bank
 *****************************************************************************
 * This variable is accessed by any function using modules.
 *****************************************************************************/
struct module_bank_t
{
    VLC_COMMON_MEMBERS

    int              i_usage;
#ifndef HAVE_SHARED_LIBVLC
    module_symbols_t symbols;
#endif

    vlc_bool_t       b_main;
    vlc_bool_t       b_builtins;
    vlc_bool_t       b_plugins;

    /* Plugins cache */
    vlc_bool_t     b_cache;
    vlc_bool_t     b_cache_dirty;
    vlc_bool_t     b_cache_delete;

    int            i_cache;
    module_cache_t **pp_cache;

    int            i_loaded_cache;
    module_cache_t **pp_loaded_cache;
};

/*****************************************************************************
 * Module cache description structure
 *****************************************************************************/
struct module_cache_t
{
    /* Mandatory cache entry header */
    char       *psz_file;
    int64_t    i_time;
    int64_t    i_size;
    vlc_bool_t b_junk;

    /* Optional extra data */
    module_t *p_module;
};

#define module_InitBank(a)     __module_InitBank(VLC_OBJECT(a))
void  __module_InitBank        ( vlc_object_t * );
#define module_LoadMain(a)     __module_LoadMain(VLC_OBJECT(a))
void  __module_LoadMain        ( vlc_object_t * );
#define module_LoadBuiltins(a) __module_LoadBuiltins(VLC_OBJECT(a))
void  __module_LoadBuiltins    ( vlc_object_t * );
#define module_LoadPlugins(a)  __module_LoadPlugins(VLC_OBJECT(a))
void  __module_LoadPlugins     ( vlc_object_t * );
#define module_EndBank(a)      __module_EndBank(VLC_OBJECT(a))
void  __module_EndBank         ( vlc_object_t * );
#define module_ResetBank(a)    __module_ResetBank(VLC_OBJECT(a))
void  __module_ResetBank       ( vlc_object_t * );
