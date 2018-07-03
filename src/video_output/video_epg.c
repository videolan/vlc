/*****************************************************************************
 * video_epg.c : EPG manipulation functions
 *****************************************************************************
 * Copyright (C) 2010 Adrien Maglo
 *               2017 VLC authors, VideoLAN and VideoLabs
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
#include <vlc_url.h>
#include "vout_spuregion_helper.h"

/* Layout percentage defines */
#define OSDEPG_MARGIN   0.025
#define OSDEPG_MARGINS  (OSDEPG_MARGIN * 2)
#define OSDEPG_PADDING  0.05 /* inner margins */
#define OSDEPG_WIDTH    (1.0 - OSDEPG_MARGINS)
#define OSDEPG_HEIGHT   0.25
#define OSDEPG_LEFT     OSDEPG_MARGIN
#define OSDEPG_TOP      (1.0 - OSDEPG_MARGINS - OSDEPG_HEIGHT + OSDEPG_MARGIN)

/* layout */
#define OSDEPG_ROWS_COUNT  10
#define OSDEPG_ROW_HEIGHT  (1.0 / OSDEPG_ROWS_COUNT)
#define OSDEPG_LOGO_SIZE (OSDEPG_HEIGHT)

/* shortcuts */
#define OSDEPG_RIGHT    (1.0 - OSDEPG_MARGIN)

#define OSDEPG_ROWS(x)  (OSDEPG_ROW_HEIGHT * x)
#define OSDEPG_ROW(x)   (OSDEPG_ROWS(x))

#define EPGOSD_TEXTSIZE_NAME    (OSDEPG_ROWS(2))
#define EPGOSD_TEXTSIZE_PROG    (OSDEPG_ROWS(2))
#define EPGOSD_TEXTSIZE_NTWK    (OSDEPG_ROWS(2))

//#define RGB_COLOR1   0xf48b00
//#define ARGB_BGCOLOR 0xC0333333

#define RGB_COLOR1   0x2badde
#define ARGB_BGCOLOR 0xc003182d

typedef struct
{
    vlc_epg_t *epg;
    int64_t    time;
    char      *art;
    vlc_object_t *obj;
} epg_spu_updater_sys_t;

static char * GetDefaultArtUri( void )
{
    char *psz_uri = NULL;
    char *psz_path = config_GetSysPath(VLC_SYSDATA_DIR, "icons/hicolor/"
                                       "128x128/"PACKAGE_NAME".png");
    if( psz_path != NULL )
    {
        psz_uri = vlc_path2uri( psz_path, NULL );
        free( psz_path );
    }
    return psz_uri;
}

#define GRADIENT_COLORS 40

static subpicture_region_t * vout_OSDBackground(int x, int y,
                                                int width, int height,
                                                uint32_t i_argb)
{
    /* Create a new subpicture region */
    video_palette_t palette;
    spuregion_CreateVGradientPalette( &palette, GRADIENT_COLORS, i_argb, 0xFF000000 );

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

    region->i_align = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;
    region->i_x = x;
    region->i_y = y;

    spuregion_CreateVGradientFill( region->p_picture->p, palette.i_entries );

    return region;
}

static subpicture_region_t * vout_OSDEpgSlider(int x, int y,
                                               int width, int height,
                                               float ratio)
{
    /* Create a new subpicture region */
    video_palette_t palette = {
        .i_entries = 4,
        .palette = {
            [0] = { HEX2YUV(RGB_COLOR1), 0x20 }, /* Bar fill remain/background */
            [1] = { HEX2YUV(0x00ff00), 0xff },
            [2] = { HEX2YUV(RGB_COLOR1), 0xC0 }, /* Bar fill */
            [3] = { HEX2YUV(0xffffff), 0xff }, /* Bar outline */
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

    region->i_align = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;
    region->i_x = x;
    region->i_y = y;

    picture_t *picture = region->p_picture;

    ratio = VLC_CLIP(ratio, 0, 1);
    int filled_part_width = ratio * width;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; ) {
            /* Slider border. */
            bool is_outline = j == 0 || j == height - 1 ||
                              i == 0 || i == width  - 1;
            /* We can see the video through the part of the slider
               which corresponds to the leaving time. */
            bool is_border = j < 3 || j > height - 4 ||
                             i < 3 || i > width  - 4 ||
                             i < filled_part_width;

            uint8_t color = 2 * is_border + is_outline;
            if(i >= 3 && i < width - 4)
            {
                if(filled_part_width > 4)
                    memset(&picture->p->p_pixels[picture->p->i_pitch * j + i],
                           color, filled_part_width - 4);
                if(width > filled_part_width + 4)
                    memset(&picture->p->p_pixels[picture->p->i_pitch * j + filled_part_width],
                           color, width - filled_part_width - 4);
                i = __MAX(i+1, filled_part_width - 1);
            }
            else
            {
                picture->p->p_pixels[picture->p->i_pitch * j + i] = color;
                i++;
            }
        }
    }

    return region;
}

static void vout_OSDSegmentSetNoWrap(text_segment_t *p_segment)
{
    for( ; p_segment; p_segment = p_segment->p_next )
    {
        p_segment->style->e_wrapinfo = STYLE_WRAP_NONE;
        p_segment->style->i_features |= STYLE_HAS_WRAP_INFO;
    }
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
    p_segment->style->i_outline_alpha = STYLE_ALPHA_TRANSPARENT;
    p_segment->style->i_shadow_alpha = STYLE_ALPHA_TRANSPARENT;
    p_segment->style->i_features |= STYLE_HAS_FONT_ALPHA | STYLE_HAS_FONT_COLOR |
                                    STYLE_HAS_OUTLINE_ALPHA | STYLE_HAS_SHADOW_ALPHA;

    return p_segment;
}

static subpicture_region_t * vout_OSDImage( vlc_object_t *p_obj,
                                            int x, int y, int w, int h,
                                            const char *psz_uri )
{
    video_format_t fmt_out;
    video_format_Init( &fmt_out, VLC_CODEC_YUVA );
    fmt_out.i_width = fmt_out.i_visible_width = w;
    fmt_out.i_height = fmt_out.i_visible_height = h;

    subpicture_region_t *image =
            spuregion_CreateFromPicture( p_obj, &fmt_out, psz_uri );
    if( image )
    {
        image->i_x = x;
        image->i_y = y;
        image->i_align = SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_TOP;
    }
    return image;
}

static void vout_OSDRegionConstrain(subpicture_region_t *p_region, int w, int h)
{
    if( p_region )
    {
        p_region->i_max_width = w;
        p_region->i_max_height = h;
    }
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
    region->i_text_align = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;
    region->i_x      = x;
    region->i_y      = y;
    region->b_balanced_text = false;

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

static subpicture_region_t * vout_OSDEpgEvent(const vlc_epg_event_t *p_evt,
                                              int x, int y, int size)
{
    text_segment_t *p_segment = NULL;
    char *psz_start = vout_OSDPrintTime(p_evt->i_start);
    char *psz_end = vout_OSDPrintTime(p_evt->i_start + p_evt->i_duration);
    char *psz_text;
    if( -1 < asprintf(&psz_text, "%s-%s ", psz_start, psz_end))
    {
        p_segment = vout_OSDSegment(psz_text, size, RGB_COLOR1);
        if( p_segment )
            p_segment->p_next = vout_OSDSegment(p_evt->psz_name, size, 0xffffff);
        vout_OSDSegmentSetNoWrap( p_segment );
    }
    free( psz_start );
    free( psz_end );
    if(!p_segment)
        return NULL;
    return vout_OSDTextRegion(p_segment, x, y);
}

static void vout_FillRightPanel(epg_spu_updater_sys_t *p_sys,
                                int x, int y,
                                int width, int height,
                                int rx, int ry,
                                subpicture_region_t **last_ptr)
{
    float f_progress = 0;
    VLC_UNUSED(ry);

    /* Display the name of the channel. */
    *last_ptr = vout_OSDEpgText(p_sys->epg->psz_name,
                                x,
                                y,
                                height * EPGOSD_TEXTSIZE_NAME,
                                0x00ffffff);
    if(*last_ptr)
        last_ptr = &(*last_ptr)->p_next;

    const vlc_epg_event_t *p_current = p_sys->epg->p_current;
    vlc_epg_event_t *p_next = NULL;
    if(!p_sys->epg->p_current && p_sys->epg->i_event)
        p_current = p_sys->epg->pp_event[0];

    for(size_t i=0; i<p_sys->epg->i_event; i++)
    {
        if( p_sys->epg->pp_event[i]->i_id != p_current->i_id )
        {
            p_next = p_sys->epg->pp_event[i];
            break;
        }
    }

    /* Display the name of the current program. */
    if(p_current)
    {
        *last_ptr = vout_OSDEpgEvent(p_current,
                                     x,
                                     y + height * OSDEPG_ROW(2),
                                     height * EPGOSD_TEXTSIZE_PROG);
        /* region rendering limits */
        vout_OSDRegionConstrain(*last_ptr, width, 0);
        if(*last_ptr)
            last_ptr = &(*last_ptr)->p_next;
    }

    /* NEXT EVENT */
    if(p_next)
    {
        *last_ptr = vout_OSDEpgEvent(p_next,
                                     x,
                                     y + height * OSDEPG_ROW(5),
                                     height * EPGOSD_TEXTSIZE_PROG);
        /* region rendering limits */
        vout_OSDRegionConstrain(*last_ptr, width, 0);
        if(*last_ptr)
            last_ptr = &(*last_ptr)->p_next;
    }

    if(p_sys->time)
    {
        f_progress = (p_sys->time - p_sys->epg->p_current->i_start) /
                     (float)p_sys->epg->p_current->i_duration;
    }

    /* Display the current program time slider. */
    *last_ptr = vout_OSDEpgSlider(x + width * 0.05,
                                  y + height * OSDEPG_ROW(9),
                                  width  * 0.90,
                                  height * OSDEPG_ROWS(1),
                                  f_progress);
    if (*last_ptr)
        last_ptr = &(*last_ptr)->p_next;

    /* Format the hours */
    if(p_sys->time)
    {
        char *psz_network = vout_OSDPrintTime(p_sys->time);
        if(psz_network)
        {
            *last_ptr = vout_OSDEpgText(psz_network,
                                        rx,
                                        y + height * OSDEPG_ROW(0),
                                        height * EPGOSD_TEXTSIZE_NTWK,
                                        RGB_COLOR1);
            free(psz_network);
            if(*last_ptr)
            {
                (*last_ptr)->i_align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_RIGHT;
                last_ptr = &(*last_ptr)->p_next;
            }
        }
    }
}

static subpicture_region_t * vout_BuildOSDEpg(epg_spu_updater_sys_t *p_sys,
                                              int x, int y,
                                              int visible_width,
                                              int visible_height)
{
    subpicture_region_t *head;
    subpicture_region_t **last_ptr = &head;

    const int i_padding = visible_height * (OSDEPG_HEIGHT * OSDEPG_PADDING);

    *last_ptr = vout_OSDBackground(x + visible_width * OSDEPG_LEFT,
                                   y + visible_height * OSDEPG_TOP,
                                   visible_width  * OSDEPG_WIDTH,
                                   visible_height * OSDEPG_HEIGHT,
                                   ARGB_BGCOLOR);
    if(*last_ptr)
        last_ptr = &(*last_ptr)->p_next;

    struct
    {
        int x;
        int y;
        int w;
        int h;
        int rx;
        int ry;
    } panel = {
        x + visible_width  * OSDEPG_LEFT + i_padding,
        y + visible_height * OSDEPG_TOP + i_padding,
        visible_width  * OSDEPG_WIDTH - 2 * i_padding,
        visible_height * OSDEPG_HEIGHT - 2 * i_padding,
        visible_width * OSDEPG_LEFT + i_padding,
        visible_height * (1.0 - OSDEPG_TOP - OSDEPG_HEIGHT) + i_padding,
    };


    if( p_sys->art )
    {
        struct
        {
            int x;
            int y;
            int w;
            int h;
        } logo = {
            panel.x,
            panel.y,
            panel.h,
            panel.h,
        };

        *last_ptr = vout_OSDBackground(logo.x,
                                       logo.y,
                                       logo.w,
                                       logo.h,
                                       0xFF000000 | RGB_COLOR1);
        if(*last_ptr)
            last_ptr = &(*last_ptr)->p_next;

        int logo_padding = visible_height * (OSDEPG_LOGO_SIZE * OSDEPG_PADDING);
        *last_ptr = vout_OSDImage( p_sys->obj,
                                   logo.x + logo_padding,
                                   logo.y + logo_padding,
                                   logo.w - 2 * logo_padding,
                                   logo.h - 2 * logo_padding,
                                   p_sys->art );
        if(*last_ptr)
            last_ptr = &(*last_ptr)->p_next;

        /* shrink */
        panel.x += logo.w + i_padding;
        panel.w -= logo.w + i_padding;
    }

    vout_FillRightPanel( p_sys,
                         panel.x,
                         panel.y,
                         panel.w,
                         panel.h,
                         panel.rx,
                         panel.ry,
                         last_ptr );

    return head;
}

static int OSDEpgValidate(subpicture_t *subpic,
                          bool has_src_changed, const video_format_t *fmt_src,
                          bool has_dst_changed, const video_format_t *fmt_dst,
                          vlc_tick_t ts)
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
                         vlc_tick_t ts)
{
    epg_spu_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(ts);

    video_format_t fmt = *fmt_dst;
    fmt.i_width         = fmt.i_width         * fmt.i_sar_num / fmt.i_sar_den;
    fmt.i_visible_width = fmt.i_visible_width * fmt.i_sar_num / fmt.i_sar_den;
    fmt.i_x_offset      = fmt.i_x_offset      * fmt.i_sar_num / fmt.i_sar_den;

    subpic->i_original_picture_width  = fmt.i_visible_width;
    subpic->i_original_picture_height = fmt.i_visible_height;

    subpic->p_region = vout_BuildOSDEpg(sys,
                                        fmt.i_x_offset,
                                        fmt.i_y_offset,
                                        fmt.i_visible_width,
                                        fmt.i_visible_height);
}

static void OSDEpgDestroy(subpicture_t *subpic)
{
    epg_spu_updater_sys_t *sys = subpic->updater.p_sys;
    if( sys->epg )
        vlc_epg_Delete(sys->epg);
    free( sys->art );
    free(sys);
}

/**
 * \brief Show EPG information about the current program of an input item
 * \param vout pointer to the vout the information is to be showed on
 * \param p_input pointer to the input item the information is to be showed
 * \param i_action osd_epg_action_e action
 */
int vout_OSDEpg(vout_thread_t *vout, input_item_t *input )
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

    epg_spu_updater_sys_t *sys = malloc(sizeof(*sys));
    if (!sys) {
        vlc_epg_Delete(epg);
        return VLC_EGENERIC;
    }
    sys->epg = epg;
    sys->obj = VLC_OBJECT(vout);
    sys->time = epg_time;
    sys->art = input_item_GetMeta( input, vlc_meta_ArtworkURL );
    if( !sys->art )
        sys->art = GetDefaultArtUri();

    subpicture_updater_t updater = {
        .pf_validate = OSDEpgValidate,
        .pf_update   = OSDEpgUpdate,
        .pf_destroy  = OSDEpgDestroy,
        .p_sys       = sys
    };

    const vlc_tick_t now = vlc_tick_now();
    subpicture_t *subpic = subpicture_New(&updater);
    if (!subpic) {
        vlc_epg_Delete(sys->epg);
        free(sys);
        return VLC_EGENERIC;
    }

    subpic->i_channel  = VOUT_SPU_CHANNEL_OSD;
    subpic->i_start    = now;
    subpic->i_stop     = now + VLC_TICK_FROM_SEC(3);
    subpic->b_ephemer  = true;
    subpic->b_absolute = false;
    subpic->b_fade     = true;
    subpic->b_subtitle = false;

    vout_PutSubpicture(vout, subpic);

    return VLC_SUCCESS;
}
