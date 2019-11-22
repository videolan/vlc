/**
 * @file render.c
 * @brief X C Bindings video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009-2018 Rémi Denis-Courmont
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/render.h>
#include <xcb/shm.h>

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include "pictures.h"
#include "events.h"

struct vout_display_sys_t {
    xcb_connection_t *conn;

    struct {
        xcb_pixmap_t source;
        xcb_pixmap_t crop;
        xcb_pixmap_t scale;
        xcb_pixmap_t subpic;
        xcb_pixmap_t alpha;
        xcb_window_t dest;
    } drawable;
    struct {
        xcb_render_picture_t source;
        xcb_render_picture_t crop;
        xcb_render_picture_t scale;
        xcb_render_picture_t subpic;
        xcb_render_picture_t alpha;
        xcb_render_picture_t dest;
    } picture;
    struct {
        xcb_render_pictformat_t argb;
        xcb_render_pictformat_t alpha;
    } format;

    xcb_gcontext_t gc;
    xcb_shm_seg_t segment;
    xcb_window_t root;
    char *filter;

    vout_display_place_t place;
    int32_t src_x;
    int32_t src_y;
    vlc_fourcc_t spu_chromas[2];
};

static size_t PictureAttach(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    xcb_connection_t *conn = sys->conn;
    xcb_shm_seg_t segment = sys->segment;
    const picture_buffer_t *buf = pic->p_sys;

    if (segment == 0  /* SHM extension not supported */
     || buf->fd == -1 /* picture buffer not in shared memory */)
        return -1;

    int fd = vlc_dup(buf->fd);
    if (fd == -1)
        return -1;

    xcb_void_cookie_t c = xcb_shm_attach_fd_checked(conn, segment, fd, 1);
    xcb_generic_error_t *e = xcb_request_check(conn, c);
    if (e != NULL) /* attach failure (likely remote access) */
    {
        free(e);
        return -1;
    }
    return buf->offset;
}

static void PictureDetach(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    xcb_shm_detach(sys->conn, sys->segment);
}

static void RenderRegion(vout_display_t *vd, const subpicture_t *subpic,
                         const subpicture_region_t *reg)
{
    vout_display_sys_t *sys = vd->sys;
    xcb_connection_t *conn = sys->conn;
    const vout_display_place_t *place = &sys->place;
    picture_t *pic = reg->p_picture;
    unsigned sw = reg->fmt.i_width;
    unsigned sh = reg->fmt.i_height;
    xcb_rectangle_t rects[] = { { 0, 0, sw, sh }, };

    xcb_create_pixmap(conn, 32, sys->drawable.subpic, sys->root, sw, sh);
    xcb_create_pixmap(conn, 8, sys->drawable.alpha, sys->root, sw, sh);
    xcb_render_create_picture(conn, sys->picture.subpic, sys->drawable.subpic,
                              sys->format.argb, 0, NULL);
    xcb_render_create_picture(conn, sys->picture.alpha, sys->drawable.alpha,
                              sys->format.alpha, 0, NULL);

    /* Upload region (TODO: use FD passing for SPU?) */
    xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, sys->drawable.subpic,
                  sys->gc, pic->p->i_pitch / pic->p->i_pixel_pitch,
                  pic->p->i_lines, 0, 0, 0, 32,
                  pic->p->i_pitch * pic->p->i_lines, pic->p->p_pixels);

    /* Copy alpha channel */
    xcb_render_composite(conn, XCB_RENDER_PICT_OP_SRC,
                         sys->picture.subpic, XCB_RENDER_PICTURE_NONE,
                         sys->picture.alpha, 0, 0, 0, 0, 0, 0, sw, sh);

    /* Force alpha channel to maximum (add 100% and clip).
     * This is to compensate RENDER expecting pre-multiplied RGB
     * while VLC uses straight RGB.
     */
    static const xcb_render_color_t alpha_one_color = { 0, 0, 0, 0xffff };

    xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_ADD,
                               sys->picture.subpic, alpha_one_color,
                               ARRAY_SIZE(rects), rects);

    /* Multiply by region and subpicture alpha factors */
    static const float alpha_fixed = 0xffffp0f / (0xffp0f * 0xffp0f);
    xcb_render_color_t alpha_color = {
        0, 0, 0, lroundf(reg->i_alpha * subpic->i_alpha * alpha_fixed) };

    xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_IN_REVERSE,
                               sys->picture.subpic, alpha_color,
                               ARRAY_SIZE(rects), rects);

    /* Mask in the original alpha channel then renver over the scaled pixmap.
     * Mask (pre)multiplies RGB channels and restores the alpha channel.
     */
    int_fast16_t dx = place->x + reg->i_x * place->width
                      / subpic->i_original_picture_width;
    int_fast16_t dy = place->y + reg->i_y * place->height
                      / subpic->i_original_picture_height;
    uint_fast16_t dw = (reg->i_x + reg->fmt.i_visible_width) * place->width
                       / subpic->i_original_picture_width;
    uint_fast16_t dh = (reg->i_y + reg->fmt.i_visible_height) * place->height
                       / subpic->i_original_picture_height;

    xcb_render_composite(conn, XCB_RENDER_PICT_OP_OVER,
                         sys->picture.subpic, sys->picture.alpha,
                         sys->picture.scale, 0, 0, 0, 0, dx, dy, dw, dh);

    xcb_render_free_picture(conn, sys->picture.alpha);
    xcb_render_free_picture(conn, sys->picture.subpic);
    xcb_free_pixmap(conn, sys->drawable.alpha);
    xcb_free_pixmap(conn, sys->drawable.subpic);
}

static void Prepare(vout_display_t *vd, picture_t *pic, subpicture_t *subpic,
                    vlc_tick_t date)
{
    const video_format_t *fmt = &vd->source;
    vout_display_sys_t *sys = vd->sys;
    xcb_connection_t *conn = sys->conn;

    size_t offset = PictureAttach(vd, pic);
    if (offset != (size_t)-1) {
        xcb_shm_put_image(conn, sys->drawable.source, sys->gc,
                          pic->p->i_pitch / pic->p->i_pixel_pitch,
                          pic->p->i_lines, 0, 0,
                          pic->p->i_pitch / pic->p->i_pixel_pitch,
                          pic->p->i_lines, 0, 0, 32, XCB_IMAGE_FORMAT_Z_PIXMAP,
                          0, sys->segment, offset);
    } else {
        xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, sys->drawable.source,
                      sys->gc, pic->p->i_pitch / pic->p->i_pixel_pitch,
                      pic->p->i_lines, 0, 0, 0, 32,
                      pic->p->i_pitch * pic->p->i_lines, pic->p->p_pixels);
    }

    /* Crop the picture with pixel accuracy */
    xcb_render_composite(conn, XCB_RENDER_PICT_OP_SRC,
                         sys->picture.source, XCB_RENDER_PICTURE_NONE,
                         sys->picture.crop,
                         fmt->i_x_offset, fmt->i_y_offset, 0, 0,
                         0, 0, fmt->i_visible_width, fmt->i_visible_height);

    /* Blank background */
    static const xcb_render_color_t black_color = { 0, 0, 0, 0xffff };
    xcb_rectangle_t rects[] = {
        { 0, 0, vd->cfg->display.width, vd->cfg->display.height },
    };

    xcb_render_fill_rectangles(conn, XCB_RENDER_PICT_OP_SRC,
                               sys->picture.scale, black_color,
                               ARRAY_SIZE(rects), rects);

    /* Scale and orient the picture */
    xcb_render_composite(conn, XCB_RENDER_PICT_OP_SRC,
                         sys->picture.crop, XCB_RENDER_PICTURE_NONE,
                         sys->picture.scale, sys->src_x, sys->src_y, 0, 0,
                         sys->place.x, sys->place.y,
                         sys->place.width, sys->place.height);
    if (offset != (size_t)-1)
        PictureDetach(vd);

    /* Blend subpictures */
    if (subpic != NULL)
        for (subpicture_region_t *r = subpic->p_region; r != NULL;
             r = r->p_next)
            RenderRegion(vd, subpic, r);

    xcb_flush(conn);
    (void) date;
}

static void Display(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    xcb_connection_t *conn = sys->conn;
    xcb_void_cookie_t ck;

    vlc_xcb_Manage(vd, conn);

    /* Copy the scaled picture into the target picture, in other words
     * copy the rendered pixmap into the window.
     */
    ck = xcb_render_composite_checked(conn, XCB_RENDER_PICT_OP_SRC,
                                      sys->picture.scale,
                                      XCB_RENDER_PICTURE_NONE,
                                      sys->picture.dest, 0, 0, 0, 0, 0, 0,
                                      vd->cfg->display.width,
                                      vd->cfg->display.height);

    xcb_generic_error_t *e = xcb_request_check(conn, ck);
    if (e != NULL) { /* Not all errors will be detected here. */
        msg_Dbg(vd, "%s: RENDER error %d", "cannot composite",
                e->error_code);
        free(e);
    }
    (void) pic;
}

static void CreateBuffers(vout_display_t *vd, const vout_display_cfg_t *cfg)
{
    const video_format_t *fmt = &vd->source;
    vout_display_sys_t *sys = vd->sys;
    xcb_connection_t *conn = sys->conn;

    xcb_create_pixmap(conn, 32, sys->drawable.crop, sys->root,
                      fmt->i_visible_width, fmt->i_visible_height);
    xcb_create_pixmap(conn, 32, sys->drawable.scale, sys->root,
                      cfg->display.width, cfg->display.height);
    xcb_render_create_picture(conn, sys->picture.crop, sys->drawable.crop,
                              sys->format.argb, 0, NULL);
    xcb_render_create_picture(conn, sys->picture.scale, sys->drawable.scale,
                              sys->format.argb, 0, NULL);

    vout_display_place_t *place = &sys->place;
    vout_display_PlacePicture(place, fmt, cfg);

    /* Homogeneous coordinates transform from destination(place)
     * to source(fmt) */
    int32_t ax = place->height; /* multiply x instead of dividing y */
    int32_t ay = place->width; /* multiply y instead of dividing x */
    int32_t bx = 0;
    int32_t by = 0;

    switch (fmt->orientation) {
        case ORIENT_TOP_LEFT:
        case ORIENT_LEFT_TOP:
            break;
        case ORIENT_TOP_RIGHT:
        case ORIENT_RIGHT_TOP:
            ax *= -1;
            bx -= place->width;
            break;
        case ORIENT_BOTTOM_LEFT:
        case ORIENT_LEFT_BOTTOM:
            ay *= -1;
            by -= place->height;
            break;
        case ORIENT_BOTTOM_RIGHT:
        case ORIENT_RIGHT_BOTTOM:
            ax *= -1;
            ay *= -1;
            bx -= place->width;
            by -= place->height;
            break;
    }

    sys->src_x = bx;
    sys->src_y = by;

    xcb_render_transform_t transform = {
        0, 0, 0,
        0, 0, 0,
        /* Multiply z by width and height to compensate for x and y above */
        0, 0, place->width * place->height,
    };

    if (ORIENT_IS_SWAP(fmt->orientation)) {
        transform.matrix12 = ay * fmt->i_visible_width;
        transform.matrix21 = ax * fmt->i_visible_height;
    } else {
        transform.matrix11 = ax * fmt->i_visible_width;
        transform.matrix22 = ay * fmt->i_visible_height;
    }

    xcb_render_set_picture_transform(conn, sys->picture.crop, transform);

    if (likely(sys->filter != NULL))
        xcb_render_set_picture_filter(conn, sys->picture.crop,
                                      strlen(sys->filter), sys->filter,
                                      0, NULL);
}

static void DeleteBuffers(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    xcb_connection_t *conn = sys->conn;

    xcb_render_free_picture(conn, sys->picture.scale);
    xcb_render_free_picture(conn, sys->picture.crop);
    xcb_free_pixmap(conn, sys->drawable.scale);
    xcb_free_pixmap(conn, sys->drawable.crop);
}

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP: {
            const vout_display_cfg_t *cfg = va_arg(ap,
                                                   const vout_display_cfg_t *);

            /* Update the window size */
            uint32_t mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
            const uint32_t values[] = {
                cfg->display.width, cfg->display.height
            };

            xcb_configure_window(sys->conn, sys->drawable.dest, mask, values);
            DeleteBuffers(vd);
            CreateBuffers(vd, cfg);
            xcb_flush(sys->conn);
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            vlc_assert_unreachable();
        default:
            msg_Err(vd, "Unknown request in XCB RENDER display");
            return VLC_EGENERIC;
    }
}

/**
 * Check that the X server supports the RENDER extension.
 */
static bool CheckRender(vout_display_t *vd, xcb_connection_t *conn)
{
    xcb_render_query_version_reply_t *r;
    xcb_render_query_version_cookie_t ck;
    bool ok = false;

    ck = xcb_render_query_version(conn, 0, 11);
    r = xcb_render_query_version_reply(conn, ck, NULL);

    if (r == NULL)
        msg_Err(vd, "RENDER extension not available");
    else if (r->major_version > 0)
        msg_Dbg(vd, "RENDER extension v%"PRIu32".%"PRIu32" unknown",
                r->major_version, r->minor_version);
    else if (r->major_version == 0 && r->minor_version < 6)
        msg_Dbg(vd, "RENDER extension v%"PRIu32".%"PRIu32" too old",
                r->major_version, r->minor_version);
    else {
        msg_Dbg(vd, "using RENDER extension v%"PRIu32".%"PRIu32,
                r->major_version, r->minor_version);
        ok = true;
    }
    free(r);
    return ok;
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    free(sys->filter);
    xcb_disconnect(sys->conn);
}

/* Convert RENDER picture format to VLC video chroma */
static vlc_fourcc_t ParseFormat(const xcb_setup_t *setup,
                                const xcb_render_pictforminfo_t *pfi)
{
    if (pfi->type != XCB_RENDER_PICT_TYPE_DIRECT)
        return 0;

    const xcb_format_t *pixfmt = vlc_xcb_DepthToPixmapFormat(setup,
                                                             pfi->depth);
    if (unlikely(pixfmt == NULL))
        return 0;

    const uint_fast8_t bpp = pixfmt->bits_per_pixel;
    const xcb_render_directformat_t *d = &pfi->direct;

    switch (pfi->depth) {
        case 32:
            if (bpp == 32 && d->red_mask == 0xff && d->green_mask == 0xff
             && d->blue_mask == 0xff && d->alpha_mask == 0xff) {
#ifdef WORDS_BIGENDIAN
                if (d->red_shift == 24 && d->green_shift == 16
                 && d->blue_shift == 8)
                    return VLC_CODEC_RGBA;
                if (d->red_shift == 8 && d->green_shift == 16
                 && d->blue_shift == 24)
                    return VLC_CODEC_BGRA;
                if (d->red_shift == 16 && d->green_shift == 8
                 && d->blue_shift == 0)
                    return VLC_CODEC_ARGB;
#else
                if (d->red_shift == 0 && d->green_shift == 8
                 && d->blue_shift == 16)
                    return VLC_CODEC_RGBA;
                if (d->red_shift == 16 && d->green_shift == 8
                 && d->blue_shift == 0)
                    return VLC_CODEC_BGRA;
                if (d->red_shift == 8 && d->green_shift == 16
                 && d->blue_shift == 24)
                    return VLC_CODEC_ARGB;
#endif
            }
            break;
        /* TODO 30 bits HDR */
        case 24:
            if (bpp == 32 && d->red_mask == 0xff && d->green_mask == 0xff
             && d->blue_mask == 0xff && d->alpha_mask == 0x00)
                return VLC_CODEC_RGB32;
            if (bpp == 24 && d->red_mask == 0xff && d->green_mask == 0xff
             && d->blue_mask == 0xff && d->alpha_mask == 0x00)
                return VLC_CODEC_RGB24;
            break;
        case 16:
            if (bpp == 16 && d->red_mask == 0x1f && d->green_mask == 0x3f
             && d->blue_mask == 0x1f && d->alpha_mask == 0x00)
                return VLC_CODEC_RGB16;
            break;
        case 15:
            if (bpp == 16 && d->red_mask == 0x1f && d->green_mask == 0x1f
             && d->blue_mask == 0x1f && d->alpha_mask == 0x00)
                return VLC_CODEC_RGB15;
            break;
    }

    return 0;
}

/* Find the RENDER screen for the X11 screen */
static const xcb_render_pictscreen_t *
FindPictScreen(const xcb_setup_t *setup, const xcb_screen_t *scr,
               const xcb_render_query_pict_formats_reply_t *r)
{
    xcb_screen_iterator_t si = xcb_setup_roots_iterator(setup);
    unsigned n = 0;

    while (si.data != scr) {
        assert(si.rem > 0);
        n++;
        xcb_screen_next(&si);
    }

    xcb_render_pictscreen_iterator_t rsi =
        xcb_render_query_pict_formats_screens_iterator(r);

    while (n > 0) {
        if (unlikely(rsi.rem == 0))
            return NULL; /* buggy server */

        n--;
        xcb_render_pictscreen_next(&rsi);
    }
    return rsi.data;
}

/* Find an X11 visual for a RENDER picture format */
static xcb_visualid_t
FindVisual(const xcb_setup_t *setup, const xcb_screen_t *scr,
           const xcb_render_query_pict_formats_reply_t *r,
           xcb_render_pictformat_t fmt_id)
{
    const xcb_render_pictscreen_t *rs = FindPictScreen(setup, scr, r);
    if (rs == NULL)
        return 0;

    xcb_render_pictdepth_iterator_t rdi =
        xcb_render_pictscreen_depths_iterator(rs);

    while (rdi.rem > 0) {
        const xcb_render_pictdepth_t *rd = rdi.data;
        xcb_render_pictvisual_iterator_t rvi =
            xcb_render_pictdepth_visuals_iterator(rd);

        while (rvi.rem > 0) {
            const xcb_render_pictvisual_t *pv = rvi.data;

            if (pv->format == fmt_id)
                return pv->visual;

            xcb_render_pictvisual_next(&rvi);
        }
        xcb_render_pictdepth_next(&rdi);
    }
    return 0;
}

/**
 * Probe the X server.
 */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *ctx)
{
    vlc_object_t *obj = VLC_OBJECT(vd);

    vout_display_sys_t *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    vd->sys = sys;

    /* Connect to X */
    xcb_connection_t *conn;
    const xcb_screen_t *screen;

    if (vlc_xcb_parent_Create(vd, cfg->window, &conn, &screen) != VLC_SUCCESS)
        return VLC_EGENERIC;

    sys->conn = conn;
    sys->root = screen->root;
    sys->format.argb = 0;
    sys->format.alpha = 0;

    if (!CheckRender(vd, conn))
        goto error;

    xcb_render_query_pict_formats_cookie_t pic_fmt_ck =
        xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *pic_fmt_r =
        xcb_render_query_pict_formats_reply(conn, pic_fmt_ck, NULL);
    if (pic_fmt_r == NULL)
        goto error;

    const xcb_setup_t *setup = xcb_get_setup(conn);
    const xcb_render_pictforminfo_t *const pic_fmts =
        xcb_render_query_pict_formats_formats(pic_fmt_r);
    xcb_visualid_t visual = 0;

    for (unsigned i = 0; i < pic_fmt_r->num_formats; i++) {
        const xcb_render_pictforminfo_t *const pic_fmt = pic_fmts + i;
        const xcb_render_directformat_t *const d = &pic_fmt->direct;

        if (pic_fmt->depth == 8 && pic_fmt->direct.alpha_mask == 0xff) {
            /* Alpha mask format */
            sys->format.alpha = pic_fmt->id;
            continue;
        }

        xcb_visualid_t vid = FindVisual(setup, screen, pic_fmt_r, pic_fmt->id);
        if (vid == 0)
            continue;

        /* Use only ARGB for now. 32-bits is guaranteed to work. */
        if (pic_fmt->depth != 32)
            continue;

        vlc_fourcc_t chroma = ParseFormat(setup, pic_fmt);
        if (chroma == 0)
            continue;

        fmtp->i_chroma = chroma;
        fmtp->i_rmask = ((uint32_t)d->red_mask) << d->red_shift;
        fmtp->i_gmask = ((uint32_t)d->green_mask) << d->green_shift;
        fmtp->i_bmask = ((uint32_t)d->blue_mask) << d->blue_shift;
        sys->format.argb = pic_fmt->id;
        visual = vid;
    }

    free(pic_fmt_r);

    if (unlikely(sys->format.argb == 0 || sys->format.alpha == 0))
        goto error; /* Buggy server */

    msg_Dbg(obj, "using RENDER picture format %u", sys->format.argb);
    msg_Dbg(obj, "using X11 visual 0x%"PRIx32, visual);

    char *filter = var_InheritString(obj, "x11-render-filter");
    if (filter != NULL) {
        msg_Dbg(obj, "using filter \"%s\"", filter);
        sys->filter = ToCharset("ISO 8859-1", filter, &(size_t){ 0 });
        free(filter);
    } else
        sys->filter = NULL;

    sys->drawable.source = xcb_generate_id(conn);
    sys->drawable.crop = xcb_generate_id(conn);
    sys->drawable.scale = xcb_generate_id(conn);
    sys->drawable.subpic = xcb_generate_id(conn);
    sys->drawable.alpha = xcb_generate_id(conn);
    sys->drawable.dest = xcb_generate_id(conn);
    sys->picture.source = xcb_generate_id(conn);
    sys->picture.crop = xcb_generate_id(conn);
    sys->picture.scale = xcb_generate_id(conn);
    sys->picture.subpic = xcb_generate_id(conn);
    sys->picture.alpha = xcb_generate_id(conn);
    sys->picture.dest = xcb_generate_id(conn);
    sys->gc = xcb_generate_id(conn);

    if (XCB_shm_Check(obj, conn))
        sys->segment = xcb_generate_id(conn);
    else
        sys->segment = 0;

    xcb_colormap_t cmap = xcb_generate_id(conn);
    uint32_t cw_mask =
        XCB_CW_BACK_PIXEL |
        XCB_CW_BORDER_PIXEL |
        XCB_CW_EVENT_MASK |
        XCB_CW_COLORMAP;
    const uint32_t cw_list[] = {
        /* XCB_CW_BACK_PIXEL */
        screen->black_pixel,
        /* XCB_CW_BORDER_PIXEL */
        screen->black_pixel,
        /* XCB_CW_EVENT_MASK */
        0,
        /* XCB_CW_COLORMAP */
        cmap,
    };

    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, cmap, screen->root,
                        visual);
    xcb_create_pixmap(conn, 32, sys->drawable.source, screen->root,
                      vd->source.i_width, vd->source.i_height);
    xcb_create_gc(conn, sys->gc, sys->drawable.source, 0, NULL);
    xcb_create_window(conn, 32, sys->drawable.dest, cfg->window->handle.xid,
                      0, 0, cfg->display.width, cfg->display.height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual, cw_mask, cw_list);
    xcb_render_create_picture(conn, sys->picture.source, sys->drawable.source,
                              sys->format.argb, 0, NULL);
    xcb_render_create_picture(conn, sys->picture.dest, sys->drawable.dest,
                              sys->format.argb, 0, NULL);
    CreateBuffers(vd, cfg);
    xcb_map_window(conn, sys->drawable.dest);

    sys->spu_chromas[0] = fmtp->i_chroma;
    sys->spu_chromas[1] = 0;

    vd->info.subpicture_chromas = sys->spu_chromas;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->close = Close;

    (void) ctx;
    return VLC_SUCCESS;

error:
    xcb_disconnect(conn);
    return VLC_EGENERIC;
}

static const char *filter_names[] = {
    "nearest", "bilinear", "fast", "good", "best",
};

static const char *filter_descs[] = {
    N_("Nearest neighbor (bad quality)"),
    N_("Bilinear"), N_("Fast"), N_("Good"), N_("Best"),
};

vlc_module_begin()
    set_shortname(N_("RENDER"))
    set_description(N_("X11 RENDER video output (XCB)"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 200)
    add_shortcut("x11-render", "xcb-render", "render")
    add_string("x11-render-filter", "good", N_("Scaling mode"),
               N_("Scaling mode"), true)
        change_string_list(filter_names, filter_descs)
vlc_module_end()
