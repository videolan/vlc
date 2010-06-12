/**
 * @file x11.c
 * @brief X C Bindings video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include "xcb_vlc.h"

#define SHM_TEXT N_("Use shared memory")
#define SHM_LONGTEXT N_( \
    "Use shared memory to communicate between VLC and the X server.")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("X11"))
    set_description (N_("X11 video output (XCB)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", 75)
    set_callbacks (Open, Close)
    add_shortcut ("xcb-x11", "x11")

    add_bool ("x11-shm", true, NULL, SHM_TEXT, SHM_LONGTEXT, true)
vlc_module_end ()

/* It must be large enough to absorb the server display jitter but it is
 * useless to used a too large value, direct rendering cannot be used with
 * xcb x11
 */
#define MAX_PICTURES (3)

struct vout_display_sys_t
{
    xcb_connection_t *conn;
    vout_window_t *embed; /* VLC window (when windowed) */

    xcb_cursor_t cursor; /* blank cursor */
    xcb_window_t window; /* drawable X window */
    xcb_gcontext_t gc; /* context to put images */
    bool shm; /* whether to use MIT-SHM */
    bool visible; /* whether to draw */
    uint8_t bpp; /* bits per pixel */
    uint8_t pad; /* scanline pad */
    uint8_t depth; /* useful bits per pixel */
    uint8_t byte_order; /* server byte order */

    picture_pool_t *pool; /* picture pool */
    picture_resource_t resource[MAX_PICTURES];
};

static picture_pool_t *Pool (vout_display_t *, unsigned);
static void Display (vout_display_t *, picture_t *);
static int Control (vout_display_t *, int, va_list);
static void Manage (vout_display_t *);

static void ResetPictures (vout_display_t *);

static const xcb_depth_t *FindDepth (const xcb_screen_t *scr,
                                     uint_fast8_t depth)
{
    xcb_depth_t *d = NULL;
    for (xcb_depth_iterator_t it = xcb_screen_allowed_depths_iterator (scr);
         it.rem > 0 && d == NULL;
         xcb_depth_next (&it))
    {
        if (it.data->depth == depth)
            d = it.data;
    }

    return d;
}


/**
 * Probe the X server.
 */
static int Open (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    vd->sys = p_sys;
    p_sys->pool = NULL;

    /* Get window, connect to X server */
    const xcb_screen_t *scr;
    p_sys->embed = GetWindow (vd, &p_sys->conn, &scr, &(uint8_t){ 0 });
    if (p_sys->embed == NULL)
    {
        free (p_sys);
        return VLC_EGENERIC;
    }

    const xcb_setup_t *setup = xcb_get_setup (p_sys->conn);
    p_sys->byte_order = setup->image_byte_order;

    /* Determine our pixel format */
    xcb_visualid_t vid = 0;
    p_sys->depth = 0;

    for (const xcb_format_t *fmt = xcb_setup_pixmap_formats (setup),
             *end = fmt + xcb_setup_pixmap_formats_length (setup);
         fmt < end;
         fmt++)
    {
        if (fmt->depth <= p_sys->depth)
            continue; /* no better than earlier format */

        video_format_t fmt_pic = vd->fmt;

        /* Check that the pixmap format is supported by VLC. */
        switch (fmt->depth)
        {
          case 32:
            if (fmt->bits_per_pixel != 32)
                continue;
#ifdef FIXED_VLC_RGBA_MASK
            fmt_pic.i_chroma = VLC_CODEC_RGBA;
            break;
#else
            msg_Dbg (vd, "X11 visual with alpha-channel not supported");
            continue;
#endif
          case 24:
            if (fmt->bits_per_pixel == 32)
                fmt_pic.i_chroma = VLC_CODEC_RGB32;
            else if (fmt->bits_per_pixel == 24)
                fmt_pic.i_chroma = VLC_CODEC_RGB24;
            else
                continue;
            break;
          case 16:
            if (fmt->bits_per_pixel != 16)
                continue;
            fmt_pic.i_chroma = VLC_CODEC_RGB16;
            break;
          case 15:
            if (fmt->bits_per_pixel != 16)
                continue;
            fmt_pic.i_chroma = VLC_CODEC_RGB15;
            break;
          case 8:
            if (fmt->bits_per_pixel != 8)
                continue;
            fmt_pic.i_chroma = VLC_CODEC_RGB8;
            break;
          default:
            continue;
        }

        /* VLC pads lines to 16 pixels internally */
        if ((fmt->bits_per_pixel << 4) % fmt->scanline_pad)
            continue;

        /* Byte sex is a non-issue for 8-bits. It can be worked around with
         * RGB masks for 24-bits. Too bad for 15-bits and 16-bits. */
        if (fmt->bits_per_pixel == 16 && setup->image_byte_order != ORDER)
            continue;

        /* Check that the selected screen supports this depth */
        const xcb_depth_t *d = FindDepth (scr, fmt->depth);
        if (d == NULL)
            continue;

        /* Find a visual type for the selected depth */
        const xcb_visualtype_t *vt = xcb_depth_visuals (d);

        /* First try True Color class */
        for (int i = xcb_depth_visuals_length (d); i > 0; i--)
        {
            if (vt->_class == XCB_VISUAL_CLASS_TRUE_COLOR)
            {
                fmt_pic.i_rmask = vt->red_mask;
                fmt_pic.i_gmask = vt->green_mask;
                fmt_pic.i_bmask = vt->blue_mask;
                goto found_visual;
            }
            vt++;
        }
        /* Then try Static Gray class */
        if (fmt->depth == 8)
            for (int i = xcb_depth_visuals_length (d); i > 0 && !vid; i--)
            {
                if (vt->_class == XCB_VISUAL_CLASS_STATIC_GRAY)
                    goto found_grey;
                vt++;
            }

        continue; /* Fail: unusable pixel format */

    found_grey:
       fmt_pic.i_chroma = VLC_CODEC_GREY;
    found_visual:
        p_sys->bpp = fmt->bits_per_pixel;
        p_sys->pad = fmt->scanline_pad;
        p_sys->depth = fmt->depth;
        vd->fmt = fmt_pic;
        vid = vt->visual_id;
    }

    if (!vid)
    {
        msg_Err (obj, "no supported pixel format & visual");
        goto error;
    }

    msg_Dbg (vd, "using X11 visual ID 0x%"PRIx32, vid);
    msg_Dbg (vd, " %"PRIu8" bits depth", p_sys->depth);
    msg_Dbg (vd, " %"PRIu8" bits per pixel", p_sys->bpp);
    msg_Dbg (vd, " %"PRIu8" bits line pad", p_sys->pad);

    /* Create colormap (needed to select non-default visual) */
    xcb_colormap_t cmap;
    if (vid != scr->root_visual)
    {
        cmap = xcb_generate_id (p_sys->conn);
        xcb_create_colormap (p_sys->conn, XCB_COLORMAP_ALLOC_NONE,
                             cmap, scr->root, vid);
    }
    else
        cmap = scr->default_colormap;

    /* Create window */
    unsigned width, height;
    if (GetWindowSize (p_sys->embed, p_sys->conn, &width, &height))
        goto error;

    p_sys->window = xcb_generate_id (p_sys->conn);
    p_sys->gc = xcb_generate_id (p_sys->conn);
    xcb_pixmap_t pixmap = xcb_generate_id (p_sys->conn);
    {
        const uint32_t mask =
            XCB_CW_BACK_PIXMAP |
            XCB_CW_BACK_PIXEL |
            XCB_CW_BORDER_PIXMAP |
            XCB_CW_BORDER_PIXEL |
            XCB_CW_EVENT_MASK |
            XCB_CW_COLORMAP;
        const uint32_t values[] = {
            /* XCB_CW_BACK_PIXMAP */
            pixmap,
            /* XCB_CW_BACK_PIXEL */
            scr->black_pixel,
            /* XCB_CW_BORDER_PIXMAP */
            pixmap,
            /* XCB_CW_BORDER_PIXEL */
            scr->black_pixel,
            /* XCB_CW_EVENT_MASK */
            XCB_EVENT_MASK_VISIBILITY_CHANGE,
            /* XCB_CW_COLORMAP */
            cmap,
        };
        xcb_void_cookie_t c;

        xcb_create_pixmap (p_sys->conn, p_sys->depth, pixmap, scr->root, 1, 1);
        c = xcb_create_window_checked (p_sys->conn, p_sys->depth,
                                       p_sys->window,
                                       p_sys->embed->handle.xid, 0, 0,
                                       width, height, 0,
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       vid, mask, values);
        xcb_map_window (p_sys->conn, p_sys->window);
        /* Create graphic context (I wonder why the heck do we need this) */
        xcb_create_gc (p_sys->conn, p_sys->gc, p_sys->window, 0, NULL);

        if (CheckError (vd, p_sys->conn, "cannot create X11 window", c))
            goto error;
    }
    msg_Dbg (vd, "using X11 window %08"PRIx32, p_sys->window);
    msg_Dbg (vd, "using X11 graphic context %08"PRIx32, p_sys->gc);
    p_sys->cursor = CreateBlankCursor (p_sys->conn, scr);

    p_sys->visible = false;

    CheckSHM (obj, p_sys->conn, &p_sys->shm);

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = true;
    info.has_event_thread = true;

    /* Setup vout_display_t once everything is fine */
    vd->info = info;

    vd->pool = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage = Manage;

    /* */
    bool is_fullscreen = vd->cfg->is_fullscreen;
    if (is_fullscreen && vout_window_SetFullScreen (p_sys->embed, true))
        is_fullscreen = false;
    vout_display_SendEventFullscreen (vd, is_fullscreen);
    vout_display_SendEventDisplaySize (vd, width, height, is_fullscreen);

    return VLC_SUCCESS;

error:
    Close (obj);
    return VLC_EGENERIC;
}


/**
 * Disconnect from the X server.
 */
static void Close (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *p_sys = vd->sys;

    ResetPictures (vd);

    /* show the default cursor */
    xcb_change_window_attributes (p_sys->conn, p_sys->embed->handle.xid, XCB_CW_CURSOR,
                                  &(uint32_t) { XCB_CURSOR_NONE });
    xcb_flush (p_sys->conn);

    /* colormap, window and context are garbage-collected by X */
    xcb_disconnect (p_sys->conn);
    vout_display_DeleteWindow (vd, p_sys->embed);
    free (p_sys);
}

/**
 * Return a direct buffer
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *p_sys = vd->sys;
    (void)requested_count;

    if (!p_sys->pool)
    {
        vout_display_place_t place;

        vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);

        /* */
        const uint32_t values[] = { place.x, place.y, place.width, place.height };
        xcb_configure_window (p_sys->conn, p_sys->window,
                              XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                              XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                              values);

        picture_t *pic = picture_NewFromFormat (&vd->fmt);
        if (!pic)
            return NULL;

        assert (pic->i_planes == 1);
        memset (p_sys->resource, 0, sizeof(p_sys->resource));

        unsigned count;
        picture_t *pic_array[MAX_PICTURES];
        for (count = 0; count < MAX_PICTURES; count++)
        {
            picture_resource_t *res = &p_sys->resource[count];

            res->p->i_lines = pic->p->i_lines;
            res->p->i_pitch = pic->p->i_pitch;
            if (PictureResourceAlloc (vd, res, res->p->i_pitch * res->p->i_lines,
                                      p_sys->conn, p_sys->shm))
                break;
            pic_array[count] = picture_NewFromResource (&vd->fmt, res);
            if (!pic_array[count])
            {
                PictureResourceFree (res, p_sys->conn);
                memset (res, 0, sizeof(*res));
                break;
            }
        }
        picture_Release (pic);

        if (count == 0)
            return NULL;

        p_sys->pool = picture_pool_New (count, pic_array);
        /* TODO release picture resources if NULL */
        xcb_flush (p_sys->conn);
    }

    return p_sys->pool;
}

/**
 * Sends an image to the X server.
 */
static void Display (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *p_sys = vd->sys;
    xcb_shm_seg_t segment = pic->p_sys->segment;
    xcb_void_cookie_t ck;

    if (!p_sys->visible)
        goto out;
    if (segment != 0)
        ck = xcb_shm_put_image_checked (p_sys->conn, p_sys->window, p_sys->gc,
          /* real width */ pic->p->i_pitch / pic->p->i_pixel_pitch,
         /* real height */ pic->p->i_lines,
                   /* x */ vd->fmt.i_x_offset,
                   /* y */ vd->fmt.i_y_offset,
               /* width */ vd->fmt.i_visible_width,
              /* height */ vd->fmt.i_visible_height,
                           0, 0, p_sys->depth, XCB_IMAGE_FORMAT_Z_PIXMAP,
                           0, segment, 0);
    else
    {
        const size_t offset = vd->fmt.i_y_offset * pic->p->i_pitch;
        const unsigned lines = pic->p->i_lines - vd->fmt.i_y_offset;

        ck = xcb_put_image_checked (p_sys->conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
                       p_sys->window, p_sys->gc,
                       pic->p->i_pitch / pic->p->i_pixel_pitch,
                       lines, -vd->fmt.i_x_offset, 0, 0, p_sys->depth,
                       pic->p->i_pitch * lines, pic->p->p_pixels + offset);
    }

    /* Wait for reply. This makes sure that the X server gets CPU time to
     * display the picture. xcb_flush() is *not* sufficient: especially with
     * shared memory the PUT requests are so short that many of them can fit in
     * X11 socket output buffer before the kernel preempts VLC. */
    xcb_generic_error_t *e = xcb_request_check (p_sys->conn, ck);
    if (e != NULL)
    {
        msg_Dbg (vd, "%s: X11 error %d", "cannot put image", e->error_code);
        free (e);
    }

    /* FIXME might be WAY better to wait in some case (be carefull with
     * VOUT_DISPLAY_RESET_PICTURES if done) + does not work with
     * vout_display wrapper. */
out:
    picture_Release (pic);
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *p_sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    {
        const vout_display_cfg_t *c = va_arg (ap, const vout_display_cfg_t *);
        return vout_window_SetFullScreen (p_sys->embed, c->is_fullscreen);
    }

    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    {
        const vout_display_cfg_t *p_cfg =
            (const vout_display_cfg_t*)va_arg (ap, const vout_display_cfg_t *);
        const bool is_forced = (bool)va_arg (ap, int);

        if (is_forced
         && vout_window_SetSize (p_sys->embed,
                                 p_cfg->display.width,
                                 p_cfg->display.height))
            return VLC_EGENERIC;

        vout_display_place_t place;
        vout_display_PlacePicture (&place, &vd->source, p_cfg, false);

        if (place.width  != vd->fmt.i_visible_width ||
            place.height != vd->fmt.i_visible_height)
        {
            vout_display_SendEventPicturesInvalid (vd);
            return VLC_SUCCESS;
        }

        /* Move the picture within the window */
        const uint32_t values[] = { place.x, place.y };
        xcb_configure_window (p_sys->conn, p_sys->window,
                              XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                              values);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    {
        unsigned state = va_arg (ap, unsigned);
        return vout_window_SetState (p_sys->embed, state);
    }

    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        /* I am not sure it is always necessary, but it is way simpler ... */
        vout_display_SendEventPicturesInvalid (vd);
        return VLC_SUCCESS;

    case VOUT_DISPLAY_RESET_PICTURES:
    {
        ResetPictures (vd);

        vout_display_place_t place;
        vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);

        vd->fmt.i_width  = vd->source.i_width  * place.width  / vd->source.i_visible_width;
        vd->fmt.i_height = vd->source.i_height * place.height / vd->source.i_visible_height;

        vd->fmt.i_visible_width  = place.width;
        vd->fmt.i_visible_height = place.height;
        vd->fmt.i_x_offset = vd->source.i_x_offset * place.width  / vd->source.i_visible_width;
        vd->fmt.i_y_offset = vd->source.i_y_offset * place.height / vd->source.i_visible_height;
        return VLC_SUCCESS;
    }

    /* Hide the mouse. It will be send when
     * vout_display_t::info.b_hide_mouse is false */
    case VOUT_DISPLAY_HIDE_MOUSE:
        xcb_change_window_attributes (p_sys->conn, p_sys->embed->handle.xid,
                                  XCB_CW_CURSOR, &(uint32_t){ p_sys->cursor });
        return VLC_SUCCESS;

    default:
        msg_Err (vd, "Unknown request in XCB vout display");
        return VLC_EGENERIC;
    }
}

static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *p_sys = vd->sys;

    ManageEvent (vd, p_sys->conn, &p_sys->visible);
}

static void ResetPictures (vout_display_t *vd)
{
    vout_display_sys_t *p_sys = vd->sys;

    if (!p_sys->pool)
        return;

    for (unsigned i = 0; i < MAX_PICTURES; i++)
    {
        picture_resource_t *res = &p_sys->resource[i];

        if (!res->p->p_pixels)
            break;
        PictureResourceFree (res, p_sys->conn);
    }
    picture_pool_Delete (p_sys->pool);
    p_sys->pool = NULL;
}
