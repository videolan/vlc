/*****************************************************************************
 * yuv.c : yuv video output
 *****************************************************************************
 * Copyright (C) 2008, M2X BV
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
#include <vlc_fs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define YUV_FILE_TEXT N_("device, fifo or filename")
#define YUV_FILE_LONGTEXT N_("device, fifo or filename to write yuv frames too.")

#define CHROMA_TEXT N_("Chroma used")
#define CHROMA_LONGTEXT N_(\
    "Force use of a specific chroma for output.")

#define YUV4MPEG2_TEXT N_("Add a YUV4MPEG2 header")
#define YUV4MPEG2_LONGTEXT N_("The YUV4MPEG2 header is compatible " \
    "with mplayer yuv video output and requires YV12/I420 fourcc.")

#define CFG_PREFIX "yuv-"

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context);
static void Close(vout_display_t *vd);

vlc_module_begin()
    set_shortname(N_("YUV output"))
    set_description(N_("YUV video output"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_string(CFG_PREFIX "file", "stream.yuv",
                YUV_FILE_TEXT, YUV_FILE_LONGTEXT, false)
    add_string(CFG_PREFIX "chroma", NULL,
                CHROMA_TEXT, CHROMA_LONGTEXT, true)
    add_bool  (CFG_PREFIX "yuv4mpeg2", false,
                YUV4MPEG2_TEXT, YUV4MPEG2_LONGTEXT, true)

    set_callback_display(Open, 0)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* */
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);

/*****************************************************************************
 * vout_display_sys_t: video output descriptor
 *****************************************************************************/
struct vout_display_sys_t {
    FILE *f;
    bool  is_first;
    bool  is_yuv4mpeg2;
};

/* */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;

    /* Allocate instance and initialize some members */
    vd->sys = sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->is_first = false;
    sys->is_yuv4mpeg2 = var_InheritBool(vd, CFG_PREFIX "yuv4mpeg2");

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
    video_format_t fmt;
    video_format_ApplyRotation(&fmt, fmtp);
    fmt.i_chroma = chroma;
    video_format_FixRgb(&fmt);

    /* */
    *fmtp = fmt;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->close = Close;

    (void) cfg; (void) context;
    return VLC_SUCCESS;
}

/* */
static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    fclose(sys->f);
    free(sys);
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    video_format_t fmt = vd->fmt;

    if (ORIENT_IS_SWAP(vd->source.orientation))
    {
        fmt.i_sar_num = vd->source.i_sar_den;
        fmt.i_sar_den = vd->source.i_sar_num;
    }
    else
    {
        fmt.i_sar_num = vd->source.i_sar_num;
        fmt.i_sar_den = vd->source.i_sar_den;
    }

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
        const uint8_t *pixels = plane->p_pixels;

        pixels += (vd->fmt.i_x_offset * plane->i_visible_pitch)
                  / vd->fmt.i_visible_height;

        for( int y = 0; y < plane->i_visible_lines; y++) {
            const size_t written = fwrite(pixels, 1, plane->i_visible_pitch,
                                          sys->f);
            if (written != (size_t)plane->i_visible_pitch)
                msg_Warn(vd, "only %zd of %d bytes written",
                         written, plane->i_visible_pitch);

            pixels += plane->i_pitch;
        }
    }
    fflush(sys->f);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    (void) vd; (void) args;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}
