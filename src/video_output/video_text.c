/*****************************************************************************
 * video_text.c : OSD text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2010 VLC authors and VideoLAN
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
#include <vlc_threads.h>
#include <vlc_vout.h>
#include <vlc_vout_osd.h>
#include <vlc_subpicture.h>

typedef struct {
    int  position;
    char *text;
} osd_spu_updater_sys_t;

static void OSDTextUpdate(subpicture_t *subpic,
                          const video_format_t *prev_src, const video_format_t *fmt_src,
                          const video_format_t *prev_dst, const video_format_t *fmt_dst,
                          vlc_tick_t ts)
{
    osd_spu_updater_sys_t *sys = subpic->updater.sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(ts);
    VLC_UNUSED(prev_src);

    if (video_format_IsSimilar(prev_dst, fmt_dst))
        return;

    vlc_spu_regions_Clear( &subpic->regions );

    assert(fmt_dst->i_sar_den && fmt_dst->i_sar_num);

    subpic->i_original_picture_width  = fmt_dst->i_visible_width * fmt_dst->i_sar_num / fmt_dst->i_sar_den;
    subpic->i_original_picture_height = fmt_dst->i_visible_height;

    subpicture_region_t *r = subpicture_region_NewText();
    if (!r)
        return;
    vlc_spu_regions_push(&subpic->regions, r);

    r->fmt.i_sar_num = 1;
    r->fmt.i_sar_den = 1;
    r->p_text = text_segment_New( sys->text );

    const float margin_ratio = 0.04f;
    const int   margin_h     = margin_ratio * fmt_dst->i_visible_width;
    const int   margin_v     = margin_ratio * fmt_dst->i_visible_height;

    r->text_flags |= sys->position;
    r->b_absolute = false;
    r->i_align = sys->position;
    if (r->i_align & SUBPICTURE_ALIGN_LEFT)
        r->i_x = margin_h + fmt_dst->i_x_offset;
    else if (r->i_align & SUBPICTURE_ALIGN_RIGHT)
        r->i_x = margin_h - fmt_dst->i_x_offset;
    else
        r->i_x = 0;

    if (r->i_align & SUBPICTURE_ALIGN_TOP )
        r->i_y = margin_v + fmt_dst->i_y_offset;
    else if (r->i_align & SUBPICTURE_ALIGN_BOTTOM )
        r->i_y = margin_v - fmt_dst->i_y_offset;
    else
        r->i_y = 0;
}

static void OSDTextDestroy(subpicture_t *subpic)
{
    osd_spu_updater_sys_t *sys = subpic->updater.sys;

    free(sys->text);
    free(sys);
}

void vout_OSDText(vout_thread_t *vout, int channel,
                   int position, vlc_tick_t duration, const char *text)
{
    assert( (position & ~SUBPICTURE_ALIGN_MASK) == 0);
    if (!var_InheritBool(vout, "osd") || duration <= 0)
        return;

    osd_spu_updater_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return;
    sys->position = position;
    sys->text     = strdup(text);

    static const struct vlc_spu_updater_ops spu_ops =
    {
        .update   = OSDTextUpdate,
        .destroy  = OSDTextDestroy,
    };

    subpicture_updater_t updater = {
        .sys = sys,
        .ops = &spu_ops,
    };
    subpicture_t *subpic = subpicture_New(&updater);
    if (!subpic) {
        free(sys->text);
        free(sys);
        return;
    }

    subpic->i_channel  = channel;
    subpic->i_start    = vlc_tick_now();
    subpic->i_stop     = subpic->i_start + duration;
    subpic->b_ephemer  = true;
    subpic->b_fade     = true;

    vout_PutSubpicture(vout, subpic);
}

void vout_OSDMessageVa(vout_thread_t *vout, int channel,
                       const char *format, va_list args)
{
    char *string;
    if (vasprintf(&string, format, args) != -1) {
        vout_OSDText(vout, channel,
                     SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_RIGHT, VLC_TICK_FROM_SEC(1),
                     string);
        free(string);
    }
}
