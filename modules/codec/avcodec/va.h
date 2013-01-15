/*****************************************************************************
 * va.h: Video Acceleration API for avcodec
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#ifndef _VLC_VA_H
#define _VLC_VA_H 1

typedef struct vlc_va_t vlc_va_t;
typedef struct vlc_va_sys_t vlc_va_sys_t;

struct vlc_va_t {
    VLC_COMMON_MEMBERS

    vlc_va_sys_t *sys;
    module_t *module;
    char *description;
    int pix_fmt;

    int  (*setup)(vlc_va_t *, void **hw, vlc_fourcc_t *output,
                  int width, int height);
    int  (*get)(vlc_va_t *, AVFrame *frame);
    void (*release)(vlc_va_t *, AVFrame *frame);
    int  (*extract)(vlc_va_t *, picture_t *dst, AVFrame *src);
};

static inline int vlc_va_Setup(vlc_va_t *va, void **hw, vlc_fourcc_t *output,
                                int width, int height)
{
    return va->setup(va, hw, output, width, height);
}
static inline int vlc_va_Get(vlc_va_t *va, AVFrame *frame)
{
    return va->get(va, frame);
}
static inline void vlc_va_Release(vlc_va_t *va, AVFrame *frame)
{
    va->release(va, frame);
}
static inline int vlc_va_Extract(vlc_va_t *va, picture_t *dst, AVFrame *src)
{
    return va->extract(va, dst, src);
}

#endif
