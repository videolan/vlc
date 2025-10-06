/*****************************************************************************
 * wingdi.c : Win32 / WinCE GDI video output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <windows.h>

#include "common.h"
#include "../../video_chroma/copy.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vout_display_t *,
                  video_format_t *, vlc_video_context *);
static void Close(vout_display_t *);

vlc_module_begin ()
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("GDI")
    set_description(N_("Windows GDI video output"))
    set_callback_display(Open, 110)
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct vout_display_sys_t
{
    struct event_thread_t    *video_wnd;
    bool                     place_changed;

    /* Our offscreen bitmap and its framebuffer */
    HDC        off_dc;
    HBITMAP    off_bitmap;

    plane_t    pic_buf;

    BITMAPINFO bmiInfo;
} vout_display_sys_t;

static void           Display(vout_display_t *, picture_t *);

static int            Init(vout_display_t *, video_format_t *);
static void           Clean(vout_display_t *);

static bool ChangeSize(vout_display_t *vd, HDC hdc)
{
    // clear the background, even if creating the writable buffer fails
    RECT display = {
        .left   = 0,
        .right  = vd->cfg->display.width,
        .top    = 0,
        .bottom = vd->cfg->display.height,
    };
    return FillRect(hdc, &display, GetStockObject(BLACK_BRUSH)) == 0;
}

static bool ChangePlace(vout_display_t *vd, HDC hdc)
{
    vout_display_sys_t *sys = vd->sys;

    video_format_t fmt_rot;
    video_format_ApplyRotation(&fmt_rot, vd->source);

    BITMAPINFOHEADER *bih = &sys->bmiInfo.bmiHeader;
    if (bih->biWidth  != (LONG)fmt_rot.i_visible_width ||
        bih->biHeight != -(LONG)fmt_rot.i_visible_height)
    {
        if (sys->off_bitmap)
            DeleteObject(sys->off_bitmap);

        bih->biWidth     = fmt_rot.i_visible_width;
        bih->biHeight    = -(LONG)fmt_rot.i_visible_height;
        void *p_pic_buffer;
        sys->off_bitmap = CreateDIBSection(hdc,
                                           &sys->bmiInfo,
                                           DIB_RGB_COLORS,
                                           &p_pic_buffer, NULL, 0);
        if (unlikely(sys->off_bitmap == NULL))
            return true;
        sys->pic_buf.p_pixels = p_pic_buffer;
        sys->pic_buf.i_pixel_pitch = (bih->biBitCount + 7) / 8;
        sys->pic_buf.i_pitch = sys->pic_buf.i_visible_pitch =
            fmt_rot.i_visible_width * sys->pic_buf.i_pixel_pitch;
        sys->pic_buf.i_lines = sys->pic_buf.i_visible_lines =
            fmt_rot.i_visible_height;
    }
    return false;
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    const struct vlc_render_subpicture *subpic,
                    vlc_tick_t date)
{
    VLC_UNUSED(subpic);
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;

    if (sys->place_changed)
    {
        bool err = false;
        HWND hwnd = CommonVideoHWND(sys->video_wnd);
        HDC hdc = GetDC(hwnd);
        err |= ChangeSize(vd, hdc);
        err |= ChangePlace(vd, hdc);
        ReleaseDC(hwnd, hdc);

        if (unlikely(err))
            return;

        sys->place_changed = false;
    }

    assert((LONG)picture->format.i_visible_width  == sys->bmiInfo.bmiHeader.biWidth &&
           (LONG)picture->format.i_visible_height == -sys->bmiInfo.bmiHeader.biHeight);

    plane_CopyPixels(&sys->pic_buf, picture->p);
}

static int SetDisplaySize(vout_display_t *vd, unsigned width, unsigned height)
{
    VLC_UNUSED(width); VLC_UNUSED(height);
    vout_display_sys_t *sys = vd->sys;
    CommonDisplaySizeChanged(sys->video_wnd);
    return VLC_SUCCESS;
}

static int Control(vout_display_t *vd, int query)
{
    vout_display_sys_t *sys = vd->sys;
    switch (query) {
    case VOUT_DISPLAY_CHANGE_SOURCE_PLACE:
        sys->place_changed = true;
        break;
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        break;
    }
    return VLC_SUCCESS;
}

static const struct vlc_display_operations ops = {
    .close = Close,
    .prepare = Prepare,
    .display = Display,
    .set_display_size = SetDisplaySize,
    .control = Control,
};

/* */
static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    VLC_UNUSED(context);
    vout_display_sys_t *sys;

    if ( !vd->obj.force && vd->source->projection_mode != PROJECTION_MODE_RECTANGULAR)
        return VLC_EGENERIC; /* let a module who can handle it do it */

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    if (CommonWindowInit(vd, &sys->video_wnd, false))
        goto error;

    /* */
    if (Init(vd, fmtp))
        goto error;

    vlc_window_SetTitle(vd->cfg->window, VOUT_TITLE " (WinGDI output)");

    /* */
    vd->ops = &ops;
    return VLC_SUCCESS;

error:
    Close(vd);
    return VLC_EGENERIC;
}

/* */
static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Clean(vd);

    CommonWindowClean(sys->video_wnd);

    free(sys);
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

    HDC hdc = GetDC(CommonVideoHWND(sys->video_wnd));

    SelectObject(sys->off_dc, sys->off_bitmap);

    video_format_t fmt_rot;
    video_format_ApplyRotation(&fmt_rot, vd->source);

    if (vd->place->width  != fmt_rot.i_visible_width ||
        vd->place->height != fmt_rot.i_visible_height) {
        SetStretchBltMode(hdc, COLORONCOLOR);

        StretchBlt(hdc, vd->place->x, vd->place->y,
                   vd->place->width, vd->place->height,
                   sys->off_dc,
                   fmt_rot.i_x_offset, fmt_rot.i_y_offset,
                   fmt_rot.i_x_offset + fmt_rot.i_visible_width,
                   fmt_rot.i_y_offset + fmt_rot.i_visible_height,
                   SRCCOPY);
    } else {
        BitBlt(hdc, vd->place->x, vd->place->y,
               vd->place->width, vd->place->height,
               sys->off_dc,
               fmt_rot.i_x_offset, fmt_rot.i_y_offset,
               SRCCOPY);
    }

    ReleaseDC(CommonVideoHWND(sys->video_wnd), hdc);
}

static int Init(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    /* Initialize an offscreen bitmap for direct buffer operations. */

    /* */
    HDC window_dc = GetDC(CommonVideoHWND(sys->video_wnd));

    /* */
    int i_depth = GetDeviceCaps(window_dc, PLANES) *
                  GetDeviceCaps(window_dc, BITSPIXEL);

    video_format_TransformTo(fmt, ORIENT_NORMAL);

    if (i_depth == 8)
        /*
         * We don't handle 256 color palettes, so let BitBlt()/StretchBlt()
         * perform the conversion to 256 color palettes for us.
         */
        i_depth = 24;

    /* */
    msg_Dbg(vd, "GDI depth is %i", i_depth);
    switch (i_depth) {
    case 16:
        fmt->i_chroma = VLC_CODEC_RGB555LE;
        break;
    case 24:
        fmt->i_chroma = VLC_CODEC_RGB24;
        break;
    case 32:
        if (vd->source->i_chroma == VLC_CODEC_BGRA)
            fmt->i_chroma = VLC_CODEC_BGRA;
        else
            fmt->i_chroma = VLC_CODEC_BGRX;
        break;
    default:
        msg_Err(vd, "screen depth %i not supported", i_depth);
        ReleaseDC(CommonVideoHWND(sys->video_wnd), window_dc);
        return VLC_EGENERIC;
    }

    /* Initialize offscreen bitmap */
    sys->bmiInfo.bmiHeader = (BITMAPINFOHEADER) {
        .biSize         = sizeof(BITMAPINFOHEADER),
        .biPlanes       = 1,
        .biBitCount     = i_depth,
        .biCompression  = BI_RGB,
    };

    sys->off_dc = CreateCompatibleDC(window_dc);

    int err = ChangeSize(vd, window_dc);
    if (err == VLC_SUCCESS)
        err = ChangePlace(vd, window_dc);
    if (err != VLC_SUCCESS)
        DeleteDC(sys->off_dc);

    ReleaseDC(CommonVideoHWND(sys->video_wnd), window_dc);

    return err;
}

static void Clean(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->off_dc)
        DeleteDC(sys->off_dc);
    if (sys->off_bitmap)
        DeleteObject(sys->off_bitmap);
}
