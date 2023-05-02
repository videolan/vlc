/*****************************************************************************
 * formatcrop.c
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#include <limits.h>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

/**
 * This filter crops the input pictures by adjusting the format (offsets and
 * size) without any copy (contrary to croppadd).
 */

#define CFG_PREFIX "formatcrop-"
static const char *const filter_options[] = {
    "top", "bottom", "left", "right", NULL,
};

#define CROPTOP_TEXT N_("Pixels to crop from top")
#define CROPTOP_LONGTEXT \
    N_("Number of pixels to crop from the top of the image.")

#define CROPBOTTOM_TEXT N_("Pixels to crop from bottom")
#define CROPBOTTOM_LONGTEXT \
    N_("Number of pixels to crop from the bottom of the image.")

#define CROPLEFT_TEXT N_("Pixels to crop from left")
#define CROPLEFT_LONGTEXT \
    N_("Number of pixels to crop from the left of the image.")

#define CROPRIGHT_TEXT N_("Pixels to crop from right")
#define CROPRIGHT_LONGTEXT \
    N_("Number of pixels to crop from the right of the image.")

#define IDX_TOP 0
#define IDX_LEFT 1
#define IDX_BOTTOM 2
#define IDX_RIGHT 3

struct transform {
    unsigned idx_top;
    unsigned idx_left;
    /* idx_bottom is idx_top XOR 2
       idx_right is idx_left XOR 2 */
};

static const struct transform transforms[8] = {
    [ORIENT_TOP_LEFT]     = { IDX_TOP,    IDX_LEFT },
    [ORIENT_TOP_RIGHT]    = { IDX_TOP,    IDX_RIGHT },
    [ORIENT_BOTTOM_LEFT]  = { IDX_BOTTOM, IDX_LEFT },
    [ORIENT_BOTTOM_RIGHT] = { IDX_BOTTOM, IDX_RIGHT },
    [ORIENT_LEFT_TOP]     = { IDX_LEFT,   IDX_TOP },
    [ORIENT_LEFT_BOTTOM]  = { IDX_LEFT,   IDX_BOTTOM },
    [ORIENT_RIGHT_TOP]    = { IDX_RIGHT,  IDX_TOP },
    [ORIENT_RIGHT_BOTTOM] = { IDX_RIGHT,  IDX_BOTTOM },
};

static picture_t *
Filter(filter_t *filter, picture_t *pic)
{
    if (!pic)
        return NULL;

    picture_t *out = picture_Clone(pic);
    if (!out)
        return NULL;
    picture_CopyProperties(out, pic);

    picture_Release(pic);

    video_format_t *fmt = &out->format;
    fmt->i_x_offset = filter->fmt_out.video.i_x_offset;
    fmt->i_y_offset = filter->fmt_out.video.i_y_offset;
    fmt->i_visible_width = filter->fmt_out.video.i_visible_width;
    fmt->i_visible_height = filter->fmt_out.video.i_visible_height;

    return out;
}

static int
Open(filter_t *filter)
{
    config_ChainParse(filter, CFG_PREFIX, filter_options, filter->p_cfg);
    unsigned top = var_InheritInteger(filter, CFG_PREFIX "top");
    unsigned bottom = var_InheritInteger(filter, CFG_PREFIX "bottom");
    unsigned left = var_InheritInteger(filter, CFG_PREFIX "left");
    unsigned right = var_InheritInteger(filter, CFG_PREFIX "right");

    video_format_t *fmt = &filter->fmt_in.video;
    video_orientation_t orientation = fmt->orientation;

    /* In the same order as IDX_ constants values */
    unsigned crop[] = { top, left, bottom, right };

    /* Transform from picture crop to physical crop (with orientation) */
    const struct transform *tx = &transforms[orientation];
    unsigned crop_top = crop[tx->idx_top];
    unsigned crop_left = crop[tx->idx_left];
    unsigned crop_bottom = crop[tx->idx_top ^ 2];
    unsigned crop_right = crop[tx->idx_left ^ 2];

    if (crop_top + crop_bottom >= fmt->i_visible_height)
    {
        msg_Err(filter, "Vertical crop (top=%u, bottom=%u) "
                        "greater than the picture height (%u)\n",
                        crop_top, crop_bottom, fmt->i_visible_height);
        return VLC_EGENERIC;
    }

    if (crop_left + crop_right >= fmt->i_visible_width)
    {
        msg_Err(filter, "Horizontal crop (left=%u, right=%u) "
                        "greater than the picture width (%u)\n",
                        crop_left, crop_right, fmt->i_visible_width);
        return VLC_EGENERIC;
    }

    filter->fmt_out.video.i_width = fmt->i_width;
    filter->fmt_out.video.i_height = fmt->i_height;
    filter->fmt_out.video.i_x_offset = fmt->i_x_offset + crop_left;
    filter->fmt_out.video.i_y_offset = fmt->i_y_offset + crop_top;
    filter->fmt_out.video.i_visible_width =
        fmt->i_visible_width - crop_left - crop_right;
    filter->fmt_out.video.i_visible_height =
        fmt->i_visible_height - crop_top - crop_bottom;

    static const struct vlc_filter_operations filter_ops =
    {
        .filter_video = Filter,
    };
    filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("formatcrop")
    set_description(N_("Video cropping filter"))
    set_callback_video_filter(Open)

    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_integer_with_range(CFG_PREFIX "top", 0, 0, INT_MAX,
                           CROPTOP_TEXT, CROPTOP_LONGTEXT)
    add_integer_with_range(CFG_PREFIX "bottom", 0, 0, INT_MAX,
                           CROPBOTTOM_TEXT, CROPTOP_LONGTEXT)
    add_integer_with_range(CFG_PREFIX "left", 0, 0, INT_MAX,
                           CROPLEFT_TEXT, CROPTOP_LONGTEXT)
    add_integer_with_range(CFG_PREFIX "right", 0, 0, INT_MAX,
                           CROPRIGHT_TEXT, CROPTOP_LONGTEXT)
vlc_module_end()
