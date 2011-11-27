/*****************************************************************************
 * video_epg.c : EPG manipulation functions
 *****************************************************************************
 * Copyright (C) 2010 Adrien Maglo
 *
 * Author: Adrien Maglo <magsoft@videolan.org>
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

#include <time.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_vout_osd.h>
#include <vlc_events.h>
#include <vlc_input_item.h>
#include <vlc_epg.h>

/* Layout percentage defines */
#define EPG_TOP 0.7
#define EPG_LEFT 0.1
#define EPG_NAME_SIZE 0.05
#define EPG_PROGRAM_SIZE 0.03

static subpicture_region_t * vout_OSDEpgSlider(int x, int y,
                                               int width, int height,
                                               float ratio)
{
    /* Create a new subpicture region */
    video_palette_t palette = {
        .i_entries = 4,
        .palette = {
            [0] = { 0xff, 0x80, 0x80, 0x00 },
            [1] = { 0x00, 0x80, 0x80, 0x00 },
            [2] = { 0xff, 0x80, 0x80, 0xff },
            [3] = { 0x00, 0x80, 0x80, 0xff },
        },
    };

    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_YUVP);
    fmt.i_width  = fmt.i_visible_width  = width;
    fmt.i_height = fmt.i_visible_height = height;
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;
    fmt.p_palette = &palette;

    subpicture_region_t *region = subpicture_region_New(&fmt);
    if (!region)
        return NULL;

    region->i_x = x;
    region->i_y = y;

    picture_t *picture = region->p_picture;

    ratio = VLC_CLIP(ratio, 0, 1);
    int filled_part_width = ratio * width;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            /* Slider border. */
            bool is_outline = j == 0 || j == height - 1 ||
                              i == 0 || i == width  - 1;
            /* We can see the video through the part of the slider
               which corresponds to the leaving time. */
            bool is_border = j < 3 || j > height - 4 ||
                             i < 3 || i > width  - 4 ||
                             i < filled_part_width;

            picture->p->p_pixels[picture->p->i_pitch * j + i] = 2 * is_border + is_outline;
        }
    }

    return region;
}


static subpicture_region_t * vout_OSDEpgText(const char *text,
                                             int x, int y,
                                             int size, uint32_t color)
{
    video_format_t fmt;
    subpicture_region_t *region;

    if (!text)
        return NULL;

    /* Create a new subpicture region */
    video_format_Init(&fmt, VLC_CODEC_TEXT);
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    region = subpicture_region_New(&fmt);
    if (!region)
        return NULL;

    /* Set subpicture parameters */
    region->psz_text = strdup(text);
    region->i_align  = 0;
    region->i_x      = x;
    region->i_y      = y;

    /* Set text style */
    region->p_style = text_style_New();
    if (region->p_style) {
        region->p_style->i_font_size  = size;
        region->p_style->i_font_color = color;
        region->p_style->i_font_alpha = 0;
    }

    return region;
}


static subpicture_region_t * vout_BuildOSDEpg(vlc_epg_t *epg,
                                              int x, int y,
                                              int visible_width,
                                              int visible_height)
{
    subpicture_region_t *head;
    subpicture_region_t **last_ptr = &head;

    time_t current_time = time(NULL);

    /* Display the name of the channel. */
    *last_ptr = vout_OSDEpgText(epg->psz_name,
                                x + visible_width  * EPG_LEFT,
                                y + visible_height * EPG_TOP,
                                visible_height * EPG_NAME_SIZE,
                                0x00ffffff);

    if (!*last_ptr)
        return head;

    /* Display the name of the current program. */
    last_ptr = &(*last_ptr)->p_next;
    *last_ptr = vout_OSDEpgText(epg->p_current->psz_name,
                                x + visible_width  * (EPG_LEFT + 0.025),
                                y + visible_height * (EPG_TOP + 0.05),
                                visible_height * EPG_PROGRAM_SIZE,
                                0x00ffffff);

    if (!*last_ptr)
        return head;

    /* Display the current program time slider. */
    last_ptr = &(*last_ptr)->p_next;
    *last_ptr = vout_OSDEpgSlider(x + visible_width  * EPG_LEFT,
                                  y + visible_height * (EPG_TOP + 0.1),
                                  visible_width  * (1 - 2 * EPG_LEFT),
                                  visible_height * 0.05,
                                  (current_time - epg->p_current->i_start)
                                  / (float)epg->p_current->i_duration);

    if (!*last_ptr)
        return head;

    /* Format the hours of the beginning and the end of the current program. */
    struct tm tm_start, tm_end;
    time_t t_start = epg->p_current->i_start;
    time_t t_end = epg->p_current->i_start + epg->p_current->i_duration;
    localtime_r(&t_start, &tm_start);
    localtime_r(&t_end, &tm_end);
    char text_start[128];
    char text_end[128];
    snprintf(text_start, sizeof(text_start), "%2.2d:%2.2d",
             tm_start.tm_hour, tm_start.tm_min);
    snprintf(text_end, sizeof(text_end), "%2.2d:%2.2d",
             tm_end.tm_hour, tm_end.tm_min);

    /* Display those hours. */
    last_ptr = &(*last_ptr)->p_next;
    *last_ptr = vout_OSDEpgText(text_start,
                                x + visible_width  * (EPG_LEFT + 0.02),
                                y + visible_height * (EPG_TOP + 0.15),
                                visible_height * EPG_PROGRAM_SIZE,
                                0x00ffffff);

    if (!*last_ptr)
        return head;

    last_ptr = &(*last_ptr)->p_next;
    *last_ptr = vout_OSDEpgText(text_end,
                                x + visible_width  * (1 - EPG_LEFT - 0.085),
                                y + visible_height * (EPG_TOP + 0.15),
                                visible_height * EPG_PROGRAM_SIZE,
                                0x00ffffff);

    return head;
}

struct subpicture_updater_sys_t
{
    vlc_epg_t *epg;
};

static int OSDEpgValidate(subpicture_t *subpic,
                          bool has_src_changed, const video_format_t *fmt_src,
                          bool has_dst_changed, const video_format_t *fmt_dst,
                          mtime_t ts)
{
    VLC_UNUSED(subpic); VLC_UNUSED(ts);
    VLC_UNUSED(fmt_src); VLC_UNUSED(has_src_changed);
    VLC_UNUSED(fmt_dst);

    if (!has_dst_changed)
        return VLC_SUCCESS;
    return VLC_EGENERIC;
}

static void OSDEpgUpdate(subpicture_t *subpic,
                         const video_format_t *fmt_src,
                         const video_format_t *fmt_dst,
                         mtime_t ts)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(ts);

    video_format_t fmt = *fmt_dst;
    fmt.i_width         = fmt.i_width         * fmt.i_sar_num / fmt.i_sar_den;
    fmt.i_visible_width = fmt.i_visible_width * fmt.i_sar_num / fmt.i_sar_den;
    fmt.i_x_offset      = fmt.i_x_offset      * fmt.i_sar_num / fmt.i_sar_den;

    subpic->i_original_picture_width  = fmt.i_width;
    subpic->i_original_picture_height = fmt.i_height;
    subpic->p_region = vout_BuildOSDEpg(sys->epg,
                                        fmt.i_x_offset,
                                        fmt.i_y_offset,
                                        fmt.i_visible_width,
                                        fmt.i_visible_height);
}

static void OSDEpgDestroy(subpicture_t *subpic)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;

    vlc_epg_Delete(sys->epg);
    free(sys);
}

/**
 * \brief Show EPG information about the current program of an input item
 * \param vout pointer to the vout the information is to be showed on
 * \param p_input pointer to the input item the information is to be showed
 */
int vout_OSDEpg(vout_thread_t *vout, input_item_t *input)
{
    char *now_playing = input_item_GetNowPlaying(input);
    vlc_epg_t *epg = NULL;

    vlc_mutex_lock(&input->lock);

    /* Look for the current program EPG event */
    for (int i = 0; i < input->i_epg; i++) {
        vlc_epg_t *tmp = input->pp_epg[i];

        if (tmp->p_current &&
            tmp->p_current->psz_name && now_playing != NULL &&
            !strcmp(tmp->p_current->psz_name, now_playing)) {
            epg = vlc_epg_New(tmp->psz_name);
            vlc_epg_Merge(epg, tmp);
            break;
        }
    }

    vlc_mutex_unlock(&input->lock);

    /* If no EPG event has been found. */
    if (epg == NULL)
        return VLC_EGENERIC;

    subpicture_updater_sys_t *sys = malloc(sizeof(*sys));
    if (!sys) {
        vlc_epg_Delete(epg);
        return VLC_EGENERIC;
    }
    sys->epg = epg;
    subpicture_updater_t updater = {
        .pf_validate = OSDEpgValidate,
        .pf_update   = OSDEpgUpdate,
        .pf_destroy  = OSDEpgDestroy,
        .p_sys       = sys
    };

    const mtime_t now = mdate();
    subpicture_t *subpic = subpicture_New(&updater);
    if (!subpic) {
        vlc_epg_Delete(sys->epg);
        free(sys);
        return VLC_EGENERIC;
    }

    subpic->i_channel  = SPU_DEFAULT_CHANNEL;
    subpic->i_start    = now;
    subpic->i_stop     = now + 3000 * INT64_C(1000);
    subpic->b_ephemer  = true;
    subpic->b_absolute = true;
    subpic->b_fade     = true;

    vout_PutSubpicture(vout, subpic);

    return VLC_SUCCESS;
}
