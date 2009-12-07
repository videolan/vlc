/*****************************************************************************
 * vlc_vout_opengl.h: vout_opengl_t definitions
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_VOUT_OPENGL_H
#define VLC_VOUT_OPENGL_H 1

/**
 * \file
 * This file defines vout opengl structures and functions in vlc
 */

#include <vlc_common.h>

typedef struct vout_opengl_t vout_opengl_t;
struct vout_opengl_t {
    /* */
    int  (*lock)(vout_opengl_t *);
    void (*swap)(vout_opengl_t *);
    void (*unlock)(vout_opengl_t *);
    /* */
    void *sys;
};

static inline int vout_opengl_Lock(vout_opengl_t *gl)
{
    if (!gl->lock)
        return VLC_SUCCESS;
    return gl->lock(gl);
}
static inline void vout_opengl_Unlock(vout_opengl_t *gl)
{
    if (gl->unlock)
        gl->unlock(gl);
}
static inline void vout_opengl_Swap(vout_opengl_t *gl)
{
    gl->swap(gl);
}

#endif /* VLC_VOUT_OPENGL_H */

