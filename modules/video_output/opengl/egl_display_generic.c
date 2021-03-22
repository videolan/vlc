/*****************************************************************************
 * egl_display_generic.c
 *****************************************************************************
 * Copyright (C) 2021 Videolabs
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "egl_display.h"

static vlc_egl_display_open_fn Open;
static int
Open(struct vlc_egl_display *display)
{
    display->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display->display == EGL_NO_DISPLAY)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description("EGL generic display")
    set_capability("egl display", 1)
    set_callback(Open)
    add_shortcut("egl_display_generic")
vlc_module_end()
