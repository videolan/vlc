/*****************************************************************************
 * transform.c : transform image module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * Copyright (C) 2010 Laurent Aimar
 * Copyright (C) 2012 Rémi Denis-Courmont
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#   include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#define CFG_PREFIX "transform-"

typedef struct {
    char      name[16];
    video_transform_t operation;
} transform_description_t;

static const transform_description_t descriptions[] = {
    { "90",            TRANSFORM_R90 },
    { "180",           TRANSFORM_R180 },
    { "270",           TRANSFORM_R270 },
    { "hflip",         TRANSFORM_HFLIP },
    { "vflip",         TRANSFORM_VFLIP },
    { "transpose",     TRANSFORM_TRANSPOSE },
    { "antitranspose", TRANSFORM_ANTI_TRANSPOSE },
};

static picture_t *Filter(filter_t *filter, picture_t *src)
{
    /* Work is not too difficult here. */
    (void) filter;
    return src;
}

static int Open(filter_t *filter)
{
    const video_format_t *src = &filter->fmt_in.video;
    video_format_t       *dst = &filter->fmt_out.video;

    if (!filter->b_allow_fmt_out_change) {
        msg_Err(filter, "Format change is not allowed");
        return VLC_EINVAL;
    }

    static const char *const ppsz_filter_options[] = {
        "type", NULL
    };

    config_ChainParse(filter, CFG_PREFIX, ppsz_filter_options,
                      filter->p_cfg);
    char *type_name = var_InheritString(filter, CFG_PREFIX"type");
    const transform_description_t *dsc = NULL;

    for (size_t i = 0; i < ARRAY_SIZE(descriptions); i++)
        if (type_name && !strcmp(descriptions[i].name, type_name)) {
            dsc = &descriptions[i];
            break;
        }
    if (dsc == NULL) {
        dsc = &descriptions[0];
        msg_Warn(filter, "No valid transform mode provided, using '%s'",
                 dsc->name);
    }

    free(type_name);

    /* Change the output orientation. DO NOT change the dimensions. */
    *dst = *src;
    dst->orientation = (video_orientation_t)
        video_format_GetTransform(src->orientation,
                                  (video_orientation_t)
                       transform_Inverse((video_transform_t)dsc->operation));

    static const struct vlc_filter_operations filter_ops =
    {
        .filter_video = Filter,
    };
    filter->ops = &filter_ops;
    return VLC_SUCCESS;
}

#define TYPE_TEXT N_("Transform type")
static const char * const type_list[] = { "90", "180", "270",
    "hflip", "vflip", "transpose", "antitranspose" };
static const char * const type_list_text[] = { N_("Rotate by 90 degrees"),
    N_("Rotate by 180 degrees"), N_("Rotate by 270 degrees"),
    N_("Flip horizontally"), N_("Flip vertically"),
    N_("Transpose"), N_("Anti-transpose") };

vlc_module_begin()
    set_description(N_("Video transformation filter"))
    set_shortname(N_("Transformation"))
    set_help(N_("Rotate or flip the video"))
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_string(CFG_PREFIX "type", "90", TYPE_TEXT, NULL)
        change_string_list(type_list, type_list_text)
        change_safe()

    add_shortcut("transform")
    set_callback_video_filter(Open)
vlc_module_end()
