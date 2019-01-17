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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vout_display_t *, const vout_display_cfg_t *,
                  video_format_t *, vlc_video_context *);
static void Close(vout_display_t *);

vlc_module_begin ()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("GDI")
    set_description(N_("Windows GDI video output"))
    set_capability("vout display", 110)
    set_callbacks(Open, Close)
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;

    int  i_depth;

    /* Our offscreen bitmap and its framebuffer */
    HDC        off_dc;
    HBITMAP    off_bitmap;

    struct
    {
        BITMAPINFO bitmapinfo;
        RGBQUAD    red;
        RGBQUAD    green;
        RGBQUAD    blue;
    };
};

static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);

static int            Init(vout_display_t *, video_format_t *);
static void           Clean(vout_display_t *);

/* */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;

    if ( !vd->obj.force && vd->source.projection_mode != PROJECTION_MODE_RECTANGULAR)
        return VLC_EGENERIC; /* let a module who can handle it do it */

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    if (CommonInit(vd, false, cfg))
        goto error;

    /* */
    if (Init(vd, fmtp))
        goto error;

    /* */
    vd->info.has_double_click     = true;

    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    return VLC_SUCCESS;

error:
    Close(vd);
    return VLC_EGENERIC;
}

/* */
static void Close(vout_display_t *vd)
{
    Clean(vd);

    CommonClean(vd);

    free(vd->sys);
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);
    return vd->sys->sys.pool;
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

#define rect_src vd->sys->rect_src
#define rect_src_clipped vd->sys->sys.rect_src_clipped
#define rect_dest vd->sys->sys.rect_dest
#define rect_dest_clipped vd->sys->sys.rect_dest_clipped
    RECT rect_dst = rect_dest_clipped;
    HDC hdc = GetDC(sys->sys.hvideownd);

    OffsetRect(&rect_dst, -rect_dest.left, -rect_dest.top);
    SelectObject(sys->off_dc, sys->off_bitmap);

    if (rect_dest_clipped.right - rect_dest_clipped.left !=
        rect_src_clipped.right - rect_src_clipped.left ||
        rect_dest_clipped.bottom - rect_dest_clipped.top !=
        rect_src_clipped.bottom - rect_src_clipped.top) {
        StretchBlt(hdc, rect_dst.left, rect_dst.top,
                   rect_dst.right, rect_dst.bottom,
                   sys->off_dc,
                   rect_src_clipped.left,  rect_src_clipped.top,
                   rect_src_clipped.right, rect_src_clipped.bottom,
                   SRCCOPY);
    } else {
        BitBlt(hdc, rect_dst.left, rect_dst.top,
               rect_dst.right, rect_dst.bottom,
               sys->off_dc,
               rect_src_clipped.left, rect_src_clipped.top,
               SRCCOPY);
    }

    ReleaseDC(sys->sys.hvideownd, hdc);
#undef rect_src
#undef rect_src_clipped
#undef rect_dest
#undef rect_dest_clipped

    CommonDisplay(vd);
    CommonManage(vd);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    switch (query) {
    case VOUT_DISPLAY_RESET_PICTURES:
        vlc_assert_unreachable();
        return VLC_EGENERIC;
    default:
        return CommonControl(vd, query, args);
    }

}

static int Init(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    RECT *display = &sys->sys.rect_display;
    display->left   = 0;
    display->top    = 0;
    display->right  = GetSystemMetrics(SM_CXSCREEN);;
    display->bottom = GetSystemMetrics(SM_CYSCREEN);;

    /* Initialize an offscreen bitmap for direct buffer operations. */

    /* */
    HDC window_dc = GetDC(sys->sys.hvideownd);

    /* */
    sys->i_depth = GetDeviceCaps(window_dc, PLANES) *
                   GetDeviceCaps(window_dc, BITSPIXEL);

    /* */
    msg_Dbg(vd, "GDI depth is %i", sys->i_depth);
    switch (sys->i_depth) {
    case 8:
        fmt->i_chroma = VLC_CODEC_RGB8;
        break;
    case 15:
        fmt->i_chroma = VLC_CODEC_RGB15;
        fmt->i_rmask  = 0x7c00;
        fmt->i_gmask  = 0x03e0;
        fmt->i_bmask  = 0x001f;
        break;
    case 16:
        fmt->i_chroma = VLC_CODEC_RGB16;
        fmt->i_rmask  = 0xf800;
        fmt->i_gmask  = 0x07e0;
        fmt->i_bmask  = 0x001f;
        break;
    case 24:
        fmt->i_chroma = VLC_CODEC_RGB24;
        fmt->i_rmask  = 0x00ff0000;
        fmt->i_gmask  = 0x0000ff00;
        fmt->i_bmask  = 0x000000ff;
        break;
    case 32:
        fmt->i_chroma = VLC_CODEC_RGB32;
        fmt->i_rmask  = 0x00ff0000;
        fmt->i_gmask  = 0x0000ff00;
        fmt->i_bmask  = 0x000000ff;
        break;
    default:
        msg_Err(vd, "screen depth %i not supported", sys->i_depth);
        return VLC_EGENERIC;
    }

    void *p_pic_buffer;
    int     i_pic_pitch;
    /* Initialize offscreen bitmap */
    BITMAPINFO *bi = &sys->bitmapinfo;
    memset(bi, 0, sizeof(BITMAPINFO) + 3 * sizeof(RGBQUAD));
    if (sys->i_depth > 8) {
        ((DWORD*)bi->bmiColors)[0] = fmt->i_rmask;
        ((DWORD*)bi->bmiColors)[1] = fmt->i_gmask;
        ((DWORD*)bi->bmiColors)[2] = fmt->i_bmask;;
    }

    BITMAPINFOHEADER *bih = &sys->bitmapinfo.bmiHeader;
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biSizeImage     = 0;
    bih->biPlanes        = 1;
    bih->biCompression   = (sys->i_depth == 15 ||
                            sys->i_depth == 16) ? BI_BITFIELDS : BI_RGB;
    bih->biBitCount      = sys->i_depth;
    bih->biWidth         = fmt->i_width;
    bih->biHeight        = -fmt->i_height;
    bih->biClrImportant  = 0;
    bih->biClrUsed       = 0;
    bih->biXPelsPerMeter = 0;
    bih->biYPelsPerMeter = 0;

    i_pic_pitch = bih->biBitCount * bih->biWidth / 8;
    sys->off_bitmap = CreateDIBSection(window_dc,
                                       (BITMAPINFO *)bih,
                                       DIB_RGB_COLORS,
                                       &p_pic_buffer, NULL, 0);

    sys->off_dc = CreateCompatibleDC(window_dc);

    SelectObject(sys->off_dc, sys->off_bitmap);
    ReleaseDC(sys->sys.hvideownd, window_dc);

    if (!sys->sys.b_windowless)
        EventThreadUpdateTitle(sys->sys.event, VOUT_TITLE " (WinGDI output)");

    /* */
    picture_resource_t rsc;
    memset(&rsc, 0, sizeof(rsc));
    rsc.p[0].p_pixels = p_pic_buffer;
    rsc.p[0].i_lines  = fmt->i_height;
    rsc.p[0].i_pitch  = i_pic_pitch;;

    picture_t *picture = picture_NewFromResource(fmt, &rsc);
    if (picture != NULL)
        sys->sys.pool = picture_pool_New(1, &picture);
    else
        sys->sys.pool = NULL;

    UpdateRects(vd, true);

    return VLC_SUCCESS;
}

static void Clean(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->sys.pool)
        picture_pool_Release(sys->sys.pool);
    sys->sys.pool = NULL;

    if (sys->off_dc)
        DeleteDC(sys->off_dc);
    if (sys->off_bitmap)
        DeleteObject(sys->off_bitmap);
}
