/*****************************************************************************
 * common.c
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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
# include "config.h"
#endif

#include "../lib/libvlc_internal.h"

#include "common.h"

libvlc_instance_t *libvlc_create(void)
{
#ifdef TOP_BUILDDIR
# ifndef HAVE_STATIC_MODULES
    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
# endif
    setenv("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
#endif

    libvlc_instance_t *vlc = libvlc_new(0, NULL);
    if (vlc == NULL)
        fprintf(stderr, "Error: cannot initialize LibVLC.\n");
    return vlc;
}
