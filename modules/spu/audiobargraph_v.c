/*****************************************************************************
 * audiobargraph_v.c : audiobargraph video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2006 VLC authors and VideoLAN
 *
 * Authors: Clement CHESNIN <clement.chesnin@gmail.com>
 *          Philippe COENT <philippe.coent@tdf.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <string.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_subpicture.h>
#include <vlc_image.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define POSX_TEXT N_("X coordinate")
#define POSX_LONGTEXT N_("X coordinate of the bargraph.")
#define POSY_TEXT N_("Y coordinate")
#define POSY_LONGTEXT N_("Y coordinate of the bargraph.")
#define TRANS_TEXT N_("Transparency of the bargraph")
#define TRANS_LONGTEXT N_("Bargraph transparency value " \
  "(from 0 for full transparency to 255 for full opacity).")
#define POS_TEXT N_("Bargraph position")
#define POS_LONGTEXT N_(\
  "Enforce the bargraph position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, eg 6 = top-right).")
#define BARWIDTH_TEXT N_("Bar width in pixel")
#define BARWIDTH_LONGTEXT N_("Width in pixel of each bar in the BarGraph to be displayed." )
#define BARHEIGHT_TEXT N_("Bar Height in pixel")
#define BARHEIGHT_LONGTEXT N_("Height in pixel of BarGraph to be displayed." )

#define CFG_PREFIX "audiobargraph_v-"

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

static int  OpenSub  (vlc_object_t *);
static int  OpenVideo(vlc_object_t *);
static void Close    (vlc_object_t *);

vlc_module_begin ()

    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_SUBPIC)

    set_capability("sub source", 0)
    set_callbacks(OpenSub, Close)
    set_description(N_("Audio Bar Graph Video sub source"))
    set_shortname(N_("Audio Bar Graph Video"))
    add_shortcut("audiobargraph_v")

    add_obsolete_string(CFG_PREFIX "i_values")
    add_integer(CFG_PREFIX "x", 0, POSX_TEXT, POSX_LONGTEXT, true)
    add_integer(CFG_PREFIX "y", 0, POSY_TEXT, POSY_LONGTEXT, true)
    add_integer_with_range(CFG_PREFIX "transparency", 255, 0, 255,
        TRANS_TEXT, TRANS_LONGTEXT, false)
    add_integer(CFG_PREFIX "position", -1, POS_TEXT, POS_LONGTEXT, false)
        change_integer_list(pi_pos_values, ppsz_pos_descriptions)
    add_obsolete_integer(CFG_PREFIX "alarm")
    add_integer(CFG_PREFIX "barWidth", 10, BARWIDTH_TEXT, BARWIDTH_LONGTEXT, true)
    add_integer(CFG_PREFIX "barHeight", 400, BARHEIGHT_TEXT, BARHEIGHT_LONGTEXT, true)

    /* video output filter submodule */
    add_submodule ()
    set_capability("video filter", 0)
    set_callbacks(OpenVideo, Close)
    set_description(N_("Audio Bar Graph Video sub source"))
    add_shortcut("audiobargraph_v")
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * Structure to hold the Bar Graph properties
 ****************************************************************************/
typedef struct
{
    int i_alpha;       /* -1 means use default alpha */
    int nbChannels;
    int *i_values;
    picture_t *p_pic;
    mtime_t date;
    int scale;
    bool alarm;
    int barWidth;

} BarGraph_t;

/**
 * Private data holder
 */
struct filter_sys_t
{
    filter_t *p_blend;

    vlc_mutex_t lock;

    BarGraph_t p_BarGraph;

    int i_pos;
    int i_pos_x;
    int i_pos_y;
    bool b_absolute;

    /* On the fly control variable */
    bool b_spu_update;
};

static const char *const ppsz_filter_options[] = {
    "x", "y", "transparency", "position", "barWidth", "barHeight", NULL
};

static const char *const ppsz_filter_callbacks[] = {
    "audiobargraph_v-x",
    "audiobargraph_v-y",
    "audiobargraph_v-transparency",
    "audiobargraph_v-position",
    "audiobargraph_v-barWidth",
    "audiobargraph_v-barHeight",
    NULL
};

/*****************************************************************************
 * IEC 268-18  Source: meterbridge
 *****************************************************************************/
static float iec_scale(float dB)
{
    if (dB < -70.0f)
        return 0.0f;
    if (dB < -60.0f)
        return (dB + 70.0f) * 0.0025f;
    if (dB < -50.0f)
        return (dB + 60.0f) * 0.005f + 0.025f;
    if (dB < -40.0f)
        return (dB + 50.0f) * 0.0075f + 0.075f;
    if (dB < -30.0f)
        return (dB + 40.0f) * 0.015f + 0.15f;
    if (dB < -20.0f)
        return (dB + 30.0f) * 0.02f + 0.3f;
    if (dB < -0.001f || dB > 0.001f)  /* if (dB < 0.0f) */
        return (dB + 20.0f) * 0.025f + 0.5f;
    return 1.0f;
}

/*****************************************************************************
 * parse_i_values : parse i_values parameter and store the corresponding values
 *****************************************************************************/
static void parse_i_values(BarGraph_t *p_BarGraph, char *i_values)
{
    char delim[] = ":";
    char* tok;

    p_BarGraph->nbChannels = 0;
    free(p_BarGraph->i_values);
    p_BarGraph->i_values = NULL;
    char *res = strtok_r(i_values, delim, &tok);
    while (res != NULL) {
        p_BarGraph->nbChannels++;
        p_BarGraph->i_values = xrealloc(p_BarGraph->i_values,
                                          p_BarGraph->nbChannels*sizeof(int));
        float db = log10(atof(res)) * 20;
        p_BarGraph->i_values[p_BarGraph->nbChannels-1] = VLC_CLIP(iec_scale(db)*p_BarGraph->scale, 0, p_BarGraph->scale);
        res = strtok_r(NULL, delim, &tok);
    }
}

/* Drawing */

static const uint8_t bright_red[4]   = { 76, 85, 0xff, 0xff };
static const uint8_t black[4] = { 0x00, 0x80, 0x80, 0xff };
static const uint8_t white[4] = { 0xff, 0x80, 0x80, 0xff };
static const uint8_t bright_green[4] = { 150, 44, 21, 0xff };
static const uint8_t bright_yellow[4] = { 226, 1, 148, 0xff };
static const uint8_t green[4] = { 74, 85, 74, 0xff };
static const uint8_t yellow[4] = { 112, 64, 138, 0xff };
static const uint8_t red[4] = { 37, 106, 191, 0xff };

static inline void DrawHLine(plane_t *p, int line, int col, const uint8_t color[4], int w)
{
    for (int j = 0; j < 4; j++)
        memset(&p[j].p_pixels[line * p[j].i_pitch + col], color[j], w);
}

static void Draw2VLines(plane_t *p, int scale, int col, const uint8_t color[4])
{
    for (int i = 10; i < scale + 10; i++)
        DrawHLine(p, i, col, color, 2);
}

static void DrawHLines(plane_t *p, int line, int col, const uint8_t color[4], int h, int w)
{
    for (int i = line; i < line + h; i++)
        DrawHLine(p, i, col, color, w);
}

static void DrawNumber(plane_t *p, int h, const uint8_t data[5], int l)
{
    for (int i = 0; i < 5; i++) {
        uint8_t x = data[i];
        for (int j = 0; j < 7; j++) {
            x <<= 1;
            if (x & 0x80)
                DrawHLine(p, h - l + 2 - 1 - i, 12 + j, black, 1);
        }
    }
}
/*****************************************************************************
 * Draw: creates and returns the bar graph image
 *****************************************************************************/
static void Draw(BarGraph_t *b)
{
    int nbChannels = b->nbChannels;
    int scale      = b->scale;
    int barWidth   = b->barWidth;

    int w = 40;
    if (nbChannels > 0)
        w = 2 * nbChannels * barWidth + 30;
    int h = scale + 30;

    int level[6];
    for (int i = 0; i < 6; i++)
        level[i] = iec_scale(-(i+1) * 10) * scale + 20;

    if (b->p_pic)
        picture_Release(b->p_pic);
    b->p_pic = picture_New(VLC_FOURCC('Y','U','V','A'), w, h, 1, 1);
    if (!b->p_pic)
        return;
    picture_t *p_pic = b->p_pic;
    plane_t *p = p_pic->p;

    for (int i = 0 ; i < p_pic->i_planes ; i++)
        memset(p[i].p_pixels, 0x00, p[i].i_visible_lines * p[i].i_pitch);

    Draw2VLines(p, scale, 20, black);
    Draw2VLines(p, scale, 22, white);

    static const uint8_t pixmap[6][5] = {
        { 0x17, 0x15, 0x15, 0x15, 0x17 },
        { 0x77, 0x45, 0x75, 0x15, 0x77 },
        { 0x77, 0x15, 0x75, 0x15, 0x77 },
        { 0x17, 0x15, 0x75, 0x55, 0x57 },
        { 0x77, 0x15, 0x75, 0x45, 0x77 },
        { 0x77, 0x55, 0x75, 0x45, 0x77 },
    };

    for (int i = 0; i < 6; i++) {
        DrawHLines(p, h - 1 - level[i] - 1, 24, white, 1, 3);
        DrawHLines(p, h - 1 - level[i],     24, black, 2, 3);
        DrawNumber(p, h, pixmap[i], level[i]);
    }

    int minus8  = iec_scale(- 8) * scale + 20;
    int minus18 = iec_scale(-18) * scale + 20;
    int *i_values  = b->i_values;
    const uint8_t *indicator_color = b->alarm ? bright_red : black;

    for (int i = 0; i < nbChannels; i++) {
        int pi = 30 + i * (5 + barWidth);

        DrawHLines(p, h - 20 - 1, pi, indicator_color, 8, barWidth);

        for (int line = 20; line < i_values[i] + 20; line++) {
            if (line < minus18)
                DrawHLines(p, h - line - 1, pi, bright_green, 1, barWidth);
            else if (line < minus8)
                DrawHLines(p, h - line - 1, pi, bright_yellow, 1, barWidth);
            else
                DrawHLines(p, h - line - 1, pi, bright_red, 1, barWidth);
        }

        for (int line = i_values[i] + 20; line < scale + 20; line++) {
            if (line < minus18)
                DrawHLines(p, h - line - 1, pi, green, 1, barWidth);
            else if (line < minus8)
                DrawHLines(p, h - line - 1, pi, yellow, 1, barWidth);
            else
                DrawHLines(p, h - line - 1, pi, red, 1, barWidth);
        }
    }
}

/*****************************************************************************
 * Callback to update params on the fly
 *****************************************************************************/
static int BarGraphCallback(vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = p_data;
    BarGraph_t *p_BarGraph = &p_sys->p_BarGraph;

    vlc_mutex_lock(&p_sys->lock);
    if (!strcmp(psz_var, CFG_PREFIX "x"))
        p_sys->i_pos_x = newval.i_int;
    else if (!strcmp(psz_var, CFG_PREFIX "y"))
        p_sys->i_pos_y = newval.i_int;
    else if (!strcmp(psz_var, CFG_PREFIX "position"))
        p_sys->i_pos = newval.i_int;
    else if (!strcmp(psz_var, CFG_PREFIX "transparency"))
        p_BarGraph->i_alpha = VLC_CLIP(newval.i_int, 0, 255);
    else if (!strcmp(psz_var, CFG_PREFIX "i_values")) {
        if (newval.psz_string)
            parse_i_values(p_BarGraph, newval.psz_string);
        Draw(p_BarGraph);
    } else if (!strcmp(psz_var, CFG_PREFIX "alarm")) {
        p_BarGraph->alarm = newval.b_bool;
        Draw(p_BarGraph);
    } else if (!strcmp(psz_var, CFG_PREFIX "barWidth")) {
        p_BarGraph->barWidth = newval.i_int;
        Draw(p_BarGraph);
    } else if (!strcmp(psz_var, CFG_PREFIX "barHeight")) {
        p_BarGraph->scale = newval.i_int;
        Draw(p_BarGraph);
    }
    p_sys->b_spu_update = true;
    vlc_mutex_unlock(&p_sys->lock);

    return VLC_SUCCESS;
}

/**
 * Sub source
 */
static subpicture_t *FilterSub(filter_t *p_filter, mtime_t date)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    BarGraph_t *p_BarGraph = &(p_sys->p_BarGraph);

    subpicture_t *p_spu;
    subpicture_region_t *p_region;
    video_format_t fmt;
    picture_t *p_pic;

    vlc_mutex_lock(&p_sys->lock);
    /* Basic test:  b_spu_update occurs on a dynamic change */
    if (!p_sys->b_spu_update) {
        vlc_mutex_unlock(&p_sys->lock);
        return NULL;
    }

    p_pic = p_BarGraph->p_pic;

    /* Allocate the subpicture internal data. */
    p_spu = filter_NewSubpicture(p_filter);
    if (!p_spu)
        goto exit;

    p_spu->b_absolute = p_sys->b_absolute;
    p_spu->i_start = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = true;

    /* Send an empty subpicture to clear the display when needed */
    if (!p_pic || !p_BarGraph->i_alpha)
        goto exit;

    /* Create new SPU region */
    memset(&fmt, 0, sizeof(video_format_t));
    fmt.i_chroma = VLC_CODEC_YUVA;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = p_pic->p[Y_PLANE].i_visible_pitch;
    fmt.i_height = fmt.i_visible_height = p_pic->p[Y_PLANE].i_visible_lines;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = subpicture_region_New(&fmt);
    if (!p_region) {
        msg_Err(p_filter, "cannot allocate SPU region");
        subpicture_Delete(p_spu);
        p_spu = NULL;
        goto exit;
    }

    /* */
    picture_Copy(p_region->p_picture, p_pic);

    /*  where to locate the bar graph: */
    if (p_sys->i_pos < 0) {   /*  set to an absolute xy */
        p_region->i_align = SUBPICTURE_ALIGN_RIGHT | SUBPICTURE_ALIGN_TOP;
        p_spu->b_absolute = true;
    } else {   /* set to one of the 9 relative locations */
        p_region->i_align = p_sys->i_pos;
        p_spu->b_absolute = false;
    }

    p_region->i_x = p_sys->i_pos_x;
    p_region->i_y = p_sys->i_pos_y;

    p_spu->p_region = p_region;

    p_spu->i_alpha = p_BarGraph->i_alpha ;

exit:
    vlc_mutex_unlock(&p_sys->lock);

    return p_spu;
}

/**
 * Video filter
 */
static picture_t *FilterVideo(filter_t *p_filter, picture_t *p_src)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    BarGraph_t *p_BarGraph = &(p_sys->p_BarGraph);

    picture_t *p_dst = filter_NewPicture(p_filter);
    if (!p_dst) {
        picture_Release(p_src);
        return NULL;
    }

    picture_Copy(p_dst, p_src);

    /* */
    vlc_mutex_lock(&p_sys->lock);

    /* */
    const picture_t *p_pic = p_BarGraph->p_pic;
    if (!p_pic)
        goto out;

    const video_format_t *p_fmt = &p_pic->format;
    const int i_dst_w = p_filter->fmt_out.video.i_visible_width;
    const int i_dst_h = p_filter->fmt_out.video.i_visible_height;

    if (p_sys->i_pos) {
        if (p_sys->i_pos & SUBPICTURE_ALIGN_BOTTOM)
            p_sys->i_pos_y = i_dst_h - p_fmt->i_visible_height;
        else if (!(p_sys->i_pos & SUBPICTURE_ALIGN_TOP))
            p_sys->i_pos_y = (i_dst_h - p_fmt->i_visible_height) / 2;
        else
            p_sys->i_pos_y = 0;

        if (p_sys->i_pos & SUBPICTURE_ALIGN_RIGHT)
            p_sys->i_pos_x = i_dst_w - p_fmt->i_visible_width;
        else if (!(p_sys->i_pos & SUBPICTURE_ALIGN_LEFT))
            p_sys->i_pos_x = (i_dst_w - p_fmt->i_visible_width) / 2;
        else
            p_sys->i_pos_x = 0;
    }

    /* */
    const int i_alpha = p_BarGraph->i_alpha;
    if (filter_ConfigureBlend(p_sys->p_blend, i_dst_w, i_dst_h, p_fmt) ||
            filter_Blend(p_sys->p_blend, p_dst, p_sys->i_pos_x, p_sys->i_pos_y,
                p_pic, i_alpha))
        msg_Err(p_filter, "failed to blend a picture");

out:
    vlc_mutex_unlock(&p_sys->lock);

    picture_Release(p_src);
    return p_dst;
}

/**
 * Common open function
 */
static int OpenCommon(vlc_object_t *p_this, bool b_sub)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* */
    if (!b_sub && !es_format_IsSimilar(&p_filter->fmt_in, &p_filter->fmt_out)) {
        msg_Err(p_filter, "Input and output format does not match");
        return VLC_EGENERIC;
    }


    /* */
    p_filter->p_sys = p_sys = malloc(sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    /* */
    p_sys->p_blend = NULL;
    if (!b_sub) {
        p_sys->p_blend = filter_NewBlend(VLC_OBJECT(p_filter),
                                          &p_filter->fmt_in.video);
        if (!p_sys->p_blend) {
            free(p_sys);
            return VLC_EGENERIC;
        }
    }

    /* */
    config_ChainParse(p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg);

    /* create and initialize variables */
    p_sys->i_pos = var_CreateGetInteger(p_filter, CFG_PREFIX "position");
    p_sys->i_pos_x = var_CreateGetInteger(p_filter, CFG_PREFIX "x");
    p_sys->i_pos_y = var_CreateGetInteger(p_filter, CFG_PREFIX "y");
    BarGraph_t *p_BarGraph = &p_sys->p_BarGraph;
    p_BarGraph->p_pic = NULL;
    p_BarGraph->i_alpha = var_CreateGetInteger(p_filter, CFG_PREFIX "transparency");
    p_BarGraph->i_alpha = VLC_CLIP(p_BarGraph->i_alpha, 0, 255);
    p_BarGraph->i_values = NULL;
    parse_i_values(p_BarGraph, &(char){ 0 });
    p_BarGraph->alarm = false;

    p_BarGraph->barWidth = var_CreateGetInteger(p_filter, CFG_PREFIX "barWidth");
    p_BarGraph->scale = var_CreateGetInteger( p_filter, CFG_PREFIX "barHeight");

    /* Ignore aligment if a position is given for video filter */
    if (!b_sub && p_sys->i_pos_x >= 0 && p_sys->i_pos_y >= 0)
        p_sys->i_pos = 0;

    vlc_mutex_init(&p_sys->lock);
    var_Create(p_filter->obj.libvlc, CFG_PREFIX "alarm", VLC_VAR_BOOL);
    var_Create(p_filter->obj.libvlc, CFG_PREFIX "i_values", VLC_VAR_STRING);

    var_AddCallback(p_filter->obj.libvlc, CFG_PREFIX "alarm",
                    BarGraphCallback, p_sys);
    var_AddCallback(p_filter->obj.libvlc, CFG_PREFIX "i_values",
                    BarGraphCallback, p_sys);

    var_TriggerCallback(p_filter->obj.libvlc, CFG_PREFIX "alarm");
    var_TriggerCallback(p_filter->obj.libvlc, CFG_PREFIX "i_values");

    for (int i = 0; ppsz_filter_callbacks[i]; i++)
        var_AddCallback(p_filter, ppsz_filter_callbacks[i],
                         BarGraphCallback, p_sys);

    if (b_sub)
        p_filter->pf_sub_source = FilterSub;
    else
        p_filter->pf_video_filter = FilterVideo;

    return VLC_SUCCESS;
}

/**
 * Open the sub source
 */
static int OpenSub(vlc_object_t *p_this)
{
    return OpenCommon(p_this, true);
}

/**
 * Open the video filter
 */
static int OpenVideo(vlc_object_t *p_this)
{
    return OpenCommon(p_this, false);
}

/**
 * Common close function
 */
static void Close(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    for (int i = 0; ppsz_filter_callbacks[i]; i++)
        var_DelCallback(p_filter, ppsz_filter_callbacks[i],
                         BarGraphCallback, p_sys);

    var_DelCallback(p_filter->obj.libvlc, CFG_PREFIX "i_values",
                    BarGraphCallback, p_sys);
    var_DelCallback(p_filter->obj.libvlc, CFG_PREFIX "alarm",
                    BarGraphCallback, p_sys);
    var_Destroy(p_filter->obj.libvlc, CFG_PREFIX "i_values");
    var_Destroy(p_filter->obj.libvlc, CFG_PREFIX "alarm");

    if (p_sys->p_blend)
        filter_DeleteBlend(p_sys->p_blend);

    vlc_mutex_destroy(&p_sys->lock);

    if (p_sys->p_BarGraph.p_pic)
        picture_Release(p_sys->p_BarGraph.p_pic);

    free(p_sys->p_BarGraph.i_values);

    free(p_sys);
}
