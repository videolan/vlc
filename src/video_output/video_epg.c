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
#define EPG_TIME_SIZE 0.03

#define RGB2YUV( R, G, B ) \
    ((0.257 * R) + (0.504 * G) + (0.098 * B) + 16), \
    (-(0.148 * R) - (0.291 * G) + (0.439 * B) + 128),\
    ((0.439 * R) - (0.368 * G) - (0.071 * B) + 128)

#define HEX2YUV( rgb ) \
    RGB2YUV( (rgb >> 16), ((rgb & 0xFF00) >> 8), (rgb & 0xFF) )

static subpicture_region_t * vout_OSDEpgSlider(int x, int y,
                                               int width, int height,
                                               float ratio)
{
    /* Create a new subpicture region */
    video_palette_t palette = {
        .i_entries = 4,
        .palette = {
            [0] = { HEX2YUV(0xffffff), 0x00 }, /* Bar fill remain/background */
            [1] = { HEX2YUV(0x000000), 0x00 },
            [2] = { HEX2YUV(0xffffff), 0xff }, /* Bar fill */
            [3] = { HEX2YUV(0x000000), 0xff }, /* Bar outline */
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

static text_segment_t * vout_OSDSegment(const char *psz_text, int size, uint32_t color)
{
    text_segment_t *p_segment = text_segment_New(psz_text);
    if(unlikely(!p_segment))
        return NULL;

    /* Set text style */
    p_segment->style = text_style_Create(STYLE_NO_DEFAULTS);
    if (unlikely(!p_segment->style))
    {
        text_segment_Delete(p_segment);
        return NULL;
    }

    p_segment->style->i_font_size  = __MAX(size ,1 );
    p_segment->style->i_font_color = color;
    p_segment->style->i_font_alpha = STYLE_ALPHA_OPAQUE;
    p_segment->style->i_features |= STYLE_HAS_FONT_ALPHA | STYLE_HAS_FONT_COLOR;

    return p_segment;
}

static subpicture_region_t * vout_OSDTextRegion(text_segment_t *p_segment,
                                                int x, int y )
{
    video_format_t fmt;
    subpicture_region_t *region;

    if (!p_segment)
        return NULL;

    /* Create a new subpicture region */
    video_format_Init(&fmt, VLC_CODEC_TEXT);
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    region = subpicture_region_New(&fmt);
    if (!region)
        return NULL;

    region->p_text   = p_segment;
    region->i_align  = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;
    region->i_x      = x;
    region->i_y      = y;

    return region;
}

static subpicture_region_t * vout_OSDEpgText(const char *text,
                                             int x, int y,
                                             int size, uint32_t color)
{
    return vout_OSDTextRegion(vout_OSDSegment(text, size, color), x, y);
}

static char * vout_OSDPrintTime(time_t t)
{
    char *psz;
    struct tm tms;
    localtime_r(&t, &tms);
    if(asprintf(&psz, "%2.2d:%2.2d", tms.tm_hour, tms.tm_min) < 0)
       psz = NULL;
    return psz;
}

static subpicture_region_t * vout_BuildOSDEpg(vlc_epg_t *epg,
                                              int64_t epgtime,
                                              int x, int y,
                                              int visible_width,
                                              int visible_height)
{
    subpicture_region_t *head;
    subpicture_region_t **last_ptr = &head;

    float f_progress = 0;

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
    *last_ptr = vout_OSDEpgText(epg->p_current ? epg->p_current->psz_name : NULL,
                                x + visible_width  * (EPG_LEFT + 0.025),
                                y + visible_height * (EPG_TOP + 0.05),
                                visible_height * EPG_PROGRAM_SIZE,
                                0x00ffffff);

    if (!*last_ptr || !epg->p_current)
        return head;

    if(epgtime)
    {
        f_progress = (epgtime - epg->p_current->i_start) /
                     (float)epg->p_current->i_duration;
    }

    /* Display the current program time slider. */
    last_ptr = &(*last_ptr)->p_next;
    *last_ptr = vout_OSDEpgSlider(x + visible_width  * EPG_LEFT,
                                  y + visible_height * (EPG_TOP + 0.1),
                                  visible_width  * (1 - 2 * EPG_LEFT),
                                  visible_height * 0.05,
                                  f_progress);

    if (!*last_ptr)
        return head;

    /* Format the hours of the beginning and the end of the current program. */
    char *psz_start = vout_OSDPrintTime(epg->p_current->i_start);
    char *psz_end = vout_OSDPrintTime(epg->p_current->i_start +
                                      epg->p_current->i_duration);

    /* Display those hours. */
    last_ptr = &(*last_ptr)->p_next;
    *last_ptr = vout_OSDEpgText(psz_start,
                                x + visible_width  * (EPG_LEFT + 0.02),
                                y + visible_height * (EPG_TOP + 0.15),
                                visible_height * EPG_TIME_SIZE,
                                0x00ffffff);

    if (!*last_ptr)
        goto end;

    last_ptr = &(*last_ptr)->p_next;
    *last_ptr = vout_OSDEpgText(psz_end,
                                x + visible_width  * (1 - EPG_LEFT - 0.085),
                                y + visible_height * (EPG_TOP + 0.15),
                                visible_height * EPG_TIME_SIZE,
                                0x00ffffff);

end:
    free(psz_start);
    free(psz_end);

    return head;
}

struct subpicture_updater_sys_t
{
    vlc_epg_t *epg;
    int64_t    time;
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

    subpic->i_original_picture_width  = fmt.i_visible_width;
    subpic->i_original_picture_height = fmt.i_visible_height;

    subpic->p_region = vout_BuildOSDEpg(sys->epg,
                                        sys->time,
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
    vlc_epg_t *epg = NULL;
    int64_t epg_time;

    /* Look for the current program EPG event */

    vlc_mutex_lock(&input->lock);

    const vlc_epg_t *tmp = input->p_epg_table;
    if ( tmp )
    {
        /* Pick table designated event, or first/next one */
        const vlc_epg_event_t *p_current_event = tmp->p_current;
        epg = vlc_epg_New(tmp->i_id, tmp->i_source_id);
        if(epg)
        {
            if( p_current_event )
            {
                vlc_epg_event_t *p_event = vlc_epg_event_Duplicate(p_current_event);
                if(p_event)
                {
                    if(!vlc_epg_AddEvent(epg, p_event))
                        vlc_epg_event_Delete(p_event);
                    else
                        vlc_epg_SetCurrent(epg, p_event->i_start);
                }
            }

            /* Add next event if any */
            vlc_epg_event_t *p_next = NULL;
            for(size_t i=0; i<tmp->i_event; i++)
            {
                vlc_epg_event_t *p_evt = tmp->pp_event[i];
                if((!p_next || p_next->i_start > p_evt->i_start) &&
                   (!p_current_event || (p_evt->i_id != p_current_event->i_id &&
                                         p_evt->i_start >= p_current_event->i_start +
                                                           p_current_event->i_duration )))
                {
                    p_next = tmp->pp_event[i];
                }
            }
            if( p_next )
            {
                vlc_epg_event_t *p_event = vlc_epg_event_Duplicate(p_next);
                if(!vlc_epg_AddEvent(epg, p_event))
                    vlc_epg_event_Delete(p_event);
            }

            if(epg->i_event > 0)
            {
                epg->psz_name = strdup(tmp->psz_name);
            }
            else
            {
                vlc_epg_Delete(epg);
                epg = NULL;
            }
        }
    }
    epg_time = input->i_epg_time;
    vlc_mutex_unlock(&input->lock);

    /* If no EPG event has been found. */
    if (epg == NULL)
        return VLC_EGENERIC;

    if(epg->psz_name == NULL) /* Fallback (title == channel name) */
        epg->psz_name = input_item_GetMeta( input, vlc_meta_Title );

    subpicture_updater_sys_t *sys = malloc(sizeof(*sys));
    if (!sys) {
        vlc_epg_Delete(epg);
        return VLC_EGENERIC;
    }
    sys->epg = epg;
    sys->time = epg_time;
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

    subpic->i_channel  = VOUT_SPU_CHANNEL_OSD;
    subpic->i_start    = now;
    subpic->i_stop     = now + 3000 * INT64_C(1000);
    subpic->b_ephemer  = true;
    subpic->b_absolute = true;
    subpic->b_fade     = true;

    vout_PutSubpicture(vout, subpic);

    return VLC_SUCCESS;
}
