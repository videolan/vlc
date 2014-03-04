/*****************************************************************************
 * va.c: hardware acceleration plugins for avcodec
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2012-2013 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_modules.h>
#include <libavcodec/avcodec.h>
#include "va.h"

static int vlc_va_Start(void *func, va_list ap)
{
    vlc_va_t *va = va_arg(ap, vlc_va_t *);
    AVCodecContext *ctx = va_arg(ap, AVCodecContext *);
    const es_format_t *fmt = va_arg(ap, const es_format_t *);
    int (*open)(vlc_va_t *, AVCodecContext *, const es_format_t *) = func;

    return open(va, ctx, fmt);
}

static void vlc_va_Stop(void *func, va_list ap)
{
    vlc_va_t *va = va_arg(ap, vlc_va_t *);
    void (*close)(vlc_va_t *) = func;

    close(va);
}

vlc_va_t *vlc_va_New(vlc_object_t *obj, AVCodecContext *avctx,
                     const es_format_t *fmt)
{
    vlc_va_t *va = vlc_object_create(obj, sizeof (*va));
    if (unlikely(va == NULL))
        return NULL;

    va->module = vlc_module_load(va, "hw decoder", "$avcodec-hw", true,
                                 vlc_va_Start, va, avctx, fmt);
    if (va->module == NULL)
    {
        vlc_object_release(va);
        va = NULL;
    }
    return va;
}

void vlc_va_Delete(vlc_va_t *va)
{
    vlc_module_unload(va->module, vlc_va_Stop, va);
    vlc_object_release(va);
}
