/*****************************************************************************
 * Copyright (C) 2021-2026 Alexandre Janniaux <ajanni@videolabs.io>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_modules_manifest.h>

typedef int (*vlc_set_cb)(void*, void*, int, ...);
typedef int (*vlc_plugin_cb)(vlc_set_cb, void*);

#define DECLARE_PLUGIN(NAME) int vlc_entry__ ## NAME(vlc_set_cb, void*);
#define LIST_PLUGIN(NAME) vlc_entry__ ## NAME,

VLC_MODULE_LIST(DECLARE_PLUGIN)

__attribute__((visibility("default")))
vlc_plugin_cb vlc_static_modules[] = {
    VLC_MODULE_LIST(LIST_PLUGIN)
    NULL,
};
