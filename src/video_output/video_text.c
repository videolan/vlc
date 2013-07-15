/*****************************************************************************
 * video_text.c : OSD text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2010 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *         Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_vout_osd.h>

struct subpicture_updater_sys_t {
    int  position;
    char *text;
};

static int OSDTextValidate(subpicture_t *subpic,
                           bool has_src_changed, const video_format_t *fmt_src,
                           bool has_dst_changed, const video_format_t *fmt_dst,
                           mtime_t ts)
{
    VLC_UNUSED(subpic); VLC_UNUSED(ts);
    VLC_UNUSED(fmt_src); VLC_UNUSED(has_src_changed);
    VLC_UNUSED(fmt_dst);

    if( !has_dst_changed )
        return VLC_SUCCESS;
    return VLC_EGENERIC;
}

static void OSDTextUpdate(subpicture_t *subpic,
                          const video_format_t *fmt_src,
                          const video_format_t *fmt_dst,
                          mtime_t ts)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(ts);

    if( fmt_dst->i_sar_num <= 0 || fmt_dst->i_sar_den <= 0 )
        return;

    subpic->b_absolute = false;
    subpic->i_original_picture_width  = fmt_dst->i_visible_width * fmt_dst->i_sar_num / fmt_dst->i_sar_den;
    subpic->i_original_picture_height = fmt_dst->i_visible_height;

    video_format_t fmt;
    video_format_Init( &fmt, VLC_CODEC_TEXT);
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    subpicture_region_t *r = subpic->p_region = subpicture_region_New(&fmt);
    if (!r)
        return;

    r->psz_text = strdup(sys->text);

    const float margin_ratio = 0.04;
    const int   margin_h     = margin_ratio * fmt_dst->i_visible_width;
    const int   margin_v     = margin_ratio * fmt_dst->i_visible_height;

    r->i_align = sys->position;
    r->i_x = 0;
    if (r->i_align & SUBPICTURE_ALIGN_LEFT)
        r->i_x += margin_h + fmt_dst->i_x_offset;
    else if (r->i_align & SUBPICTURE_ALIGN_RIGHT)
        r->i_x += margin_h - fmt_dst->i_x_offset;

    r->i_y = 0;
    if (r->i_align & SUBPICTURE_ALIGN_TOP )
        r->i_y += margin_v + fmt_dst->i_y_offset;
    else if (r->i_align & SUBPICTURE_ALIGN_BOTTOM )
        r->i_y += margin_v - fmt_dst->i_y_offset;
}

static void OSDTextDestroy(subpicture_t *subpic)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;

    free(sys->text);
    free(sys);
}

void vout_OSDText(vout_thread_t *vout, int channel,
                   int position, mtime_t duration, const char *text)
{
    assert( (position & ~SUBPICTURE_ALIGN_MASK) == 0);
    if (!var_InheritBool(vout, "osd") || duration <= 0)
        return;

    subpicture_updater_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return;
    sys->position = position;
    sys->text     = strdup(text);

    subpicture_updater_t updater = {
        .pf_validate = OSDTextValidate,
        .pf_update   = OSDTextUpdate,
        .pf_destroy  = OSDTextDestroy,
        .p_sys       = sys,
    };
    subpicture_t *subpic = subpicture_New(&updater);
    if (!subpic) {
        free(sys->text);
        free(sys);
        return;
    }

    subpic->i_channel  = channel;
    subpic->i_start    = mdate();
    subpic->i_stop     = subpic->i_start + duration;
    subpic->b_ephemer  = true;
    subpic->b_absolute = false;
    subpic->b_fade     = true;

    vout_PutSubpicture(vout, subpic);
}

void vout_OSDMessage(vout_thread_t *vout, int channel, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char *string;
    if (vasprintf(&string, format, args) != -1) {
        vout_OSDText(vout, channel,
                     SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_RIGHT, 1000000,
                     string);
        free(string);
    }
    va_end(args);
}

