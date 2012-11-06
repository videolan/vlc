/*****************************************************************************
 * yuv.c : yuv video output
 *****************************************************************************
 * Copyright (C) 2008, M2X BV
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman@videolan.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_fs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define YUV_FILE_TEXT N_("device, fifo or filename")
#define YUV_FILE_LONGTEXT N_("device, fifo or filename to write yuv frames too.")

#define CHROMA_TEXT N_("Chroma used")
#define CHROMA_LONGTEXT N_(\
    "Force use of a specific chroma for output. Default is I420.")

#define YUV4MPEG2_TEXT N_("YUV4MPEG2 header (default disabled)")
#define YUV4MPEG2_LONGTEXT N_("The YUV4MPEG2 header is compatible " \
    "with mplayer yuv video output and requires YV12/I420 fourcc. By default "\
    "vlc writes the fourcc of the picture frame into the output destination.")

#define CFG_PREFIX "yuv-"

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("YUV output"))
    set_description(N_("YUV video output"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 0)

    add_string(CFG_PREFIX "file", "stream.yuv",
                YUV_FILE_TEXT, YUV_FILE_LONGTEXT, false)
    add_string(CFG_PREFIX "chroma", NULL,
                CHROMA_TEXT, CHROMA_LONGTEXT, true)
    add_bool  (CFG_PREFIX "yuv4mpeg2", false,
                YUV4MPEG2_TEXT, YUV4MPEG2_LONGTEXT, true)

    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_vout_options[] = {
    "file", "chroma", "yuv4mpeg2", NULL
};

/* */
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *, subpicture_t *subpicture);
static int            Control(vout_display_t *, int, va_list);

/*****************************************************************************
 * vout_display_sys_t: video output descriptor
 *****************************************************************************/
struct vout_display_sys_t {
    FILE *f;
    bool  is_first;
    bool  is_yuv4mpeg2;

    picture_pool_t *pool;
};

/* */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    /* Allocate instance and initialize some members */
    vd->sys = sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->is_first = false;
    sys->is_yuv4mpeg2 = var_InheritBool(vd, CFG_PREFIX "yuv4mpeg2");
    sys->pool = NULL;

    /* */
    char *psz_fcc = var_InheritString(vd, CFG_PREFIX "chroma");
    const vlc_fourcc_t requested_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES,
                                                                        psz_fcc);
    free(psz_fcc);

    const vlc_fourcc_t chroma = requested_chroma ? requested_chroma :
                                                   VLC_CODEC_I420;
    if (sys->is_yuv4mpeg2) {
        switch (chroma) {
        case VLC_CODEC_YV12:
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
            break;
        default:
            msg_Err(vd, "YUV4MPEG2 mode needs chroma YV12 not %4.4s as requested",
                    (char *)&chroma);
            free(sys);
            return VLC_EGENERIC;
        }
    }
    msg_Dbg(vd, "Using chroma %4.4s", (char *)&chroma);

    /* */
    char *name = var_InheritString(vd, CFG_PREFIX "file");
    if (!name) {
        msg_Err(vd, "Empty file name");
        free(sys);
        return VLC_EGENERIC;
    }
    sys->f = vlc_fopen(name, "wb");

    if (!sys->f) {
        msg_Err(vd, "Failed to open %s", name);
        free(name);
        free(sys);
        return VLC_EGENERIC;
    }
    msg_Dbg(vd, "Writing data to %s", name);
    free(name);

    /* */
    video_format_t fmt = vd->fmt;
    fmt.i_chroma = chroma;
    video_format_FixRgb(&fmt);

    /* */
    vout_display_info_t info = vd->info;
    info.has_hide_mouse = true;

    /* */
    vd->fmt     = fmt;
    vd->info    = info;
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = NULL;

    vout_display_SendEventFullscreen(vd, false);
    return VLC_SUCCESS;
}

/* */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);
    fclose(sys->f);
    free(sys);
}

/*****************************************************************************
 *
 *****************************************************************************/
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    video_format_t fmt = vd->fmt;
    fmt.i_sar_num = vd->source.i_sar_num;
    fmt.i_sar_den = vd->source.i_sar_den;

    /* */
    char type;
    if (picture->b_progressive)
        type = 'p';
    else if (picture->b_top_field_first)
        type = 't';
    else
        type = 'b';

    if (type != 'p') {
        msg_Warn(vd, "Received a non progressive frame, "
                     "it will be written as progressive.");
        type = 'p';
    }

    /* */
    if (!sys->is_first) {
        const char *header;
        char buffer[5];
        if (sys->is_yuv4mpeg2) {
            /* MPlayer compatible header, unfortunately it doesn't tell you
             * the exact fourcc used. */
            header = "YUV4MPEG2";
        } else {
            snprintf(buffer, sizeof(buffer), "%4.4s", 
                     (const char*)&fmt.i_chroma);
            header = buffer;
        }

        fprintf(sys->f, "%s W%d H%d F%d:%d I%c A%d:%d\n",
                header,
                fmt.i_visible_width, fmt.i_visible_height,
                fmt.i_frame_rate, fmt.i_frame_rate_base,
                type,
                fmt.i_sar_num, fmt.i_sar_den);
        sys->is_first = true;
    }

    /* */
    fprintf(sys->f, "FRAME\n");
    for (int i = 0; i < picture->i_planes; i++) {
        const plane_t *plane = &picture->p[i];
        for( int y = 0; y < plane->i_visible_lines; y++) {
            const size_t written = fwrite(&plane->p_pixels[y*plane->i_pitch],
                                          1, plane->i_visible_pitch, sys->f);
            if (written != (size_t)plane->i_visible_pitch)
                msg_Warn(vd, "only %zd of %d bytes written",
                         written, plane->i_visible_pitch);
        }
    }
    fflush(sys->f);

    /* */
    picture_Release(picture);
    VLC_UNUSED(subpicture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(vd);
    switch (query) {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN: {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);
        if (cfg->is_fullscreen)
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    default:
        return VLC_EGENERIC;
    }
}

