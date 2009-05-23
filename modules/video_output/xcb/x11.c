/**
 * @file x11.c
 * @brief X C Bindings video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
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
#include <vlc_vout.h>
#include <vlc_window.h>

#include "xcb_vlc.h"

#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
    "X11 hardware display to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

#define SHM_TEXT N_("Use shared memory")
#define SHM_LONGTEXT N_( \
    "Use shared memory to communicate between VLC and the X server.")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("XCB"))
    set_description (N_("(Experimental) XCB video output"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("video output", 0)
    set_callbacks (Open, Close)

    add_string ("x11-display", NULL, NULL,
                DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
    add_bool ("x11-shm", true, NULL, SHM_TEXT, SHM_LONGTEXT, true)
vlc_module_end ()

struct vout_sys_t
{
    xcb_connection_t *conn;
    vout_window_t *embed; /* VLC window (when windowed) */

    xcb_window_t window; /* drawable X window */
    xcb_gcontext_t gc; /* context to put images */
    bool shm; /* whether to use MIT-SHM */
    uint8_t bpp; /* bits per pixel */
    uint8_t pad; /* scanline pad */
    uint8_t depth; /* useful bits per pixel */
    uint8_t byte_order; /* server byte order */
};

static int Init (vout_thread_t *);
static void Deinit (vout_thread_t *);
static void Display (vout_thread_t *, picture_t *);
static int Manage (vout_thread_t *);
static int Control (vout_thread_t *, int, va_list);

int CheckError (vout_thread_t *vout, const char *str, xcb_void_cookie_t ck)
{
    xcb_generic_error_t *err;

    err = xcb_request_check (vout->p_sys->conn, ck);
    if (err)
    {
        msg_Err (vout, "%s: X11 error %d", str, err->error_code);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

#define p_vout vout

/**
 * Probe the X server.
 */
static int Open (vlc_object_t *obj)
{
    vout_thread_t *vout = (vout_thread_t *)obj;
    vout_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    vout->p_sys = p_sys;

    /* Connect to X */
    p_sys->conn = Connect (obj);
    if (p_sys->conn == NULL)
    {
        free (p_sys);
        return VLC_EGENERIC;
    }

    /* Get window */
    const xcb_screen_t *scr;
    p_sys->embed = GetWindow (vout, p_sys->conn, &scr, &p_sys->shm);
    if (p_sys->embed == NULL)
    {
        xcb_disconnect (p_sys->conn);
        free (p_sys);
        return VLC_EGENERIC;
    }

    const xcb_setup_t *setup = xcb_get_setup (p_sys->conn);
    p_sys->byte_order = setup->image_byte_order;

    /* Determine our video format. Normally, this is done in pf_init(), but
     * this plugin always uses the same format for a given X11 screen. */
    xcb_visualid_t vid = 0;
    uint8_t depth = 0;
    bool gray = true;
    for (const xcb_format_t *fmt = xcb_setup_pixmap_formats (setup),
             *end = fmt + xcb_setup_pixmap_formats_length (setup);
         fmt < end; fmt++)
    {
        vlc_fourcc_t chroma = 0;

        if (fmt->depth < depth)
            continue; /* We already found a better format! */

        /* Check that the pixmap format is supported by VLC. */
        switch (fmt->depth)
        {
          case 24:
            if (fmt->bits_per_pixel == 32)
                chroma = VLC_FOURCC ('R', 'V', '3', '2');
            else if (fmt->bits_per_pixel == 24)
                chroma = VLC_FOURCC ('R', 'V', '2', '4');
            else
                continue;
            break;
          case 16:
            if (fmt->bits_per_pixel != 16)
                continue;
            chroma = VLC_FOURCC ('R', 'V', '1', '6');
            break;
          case 15:
            if (fmt->bits_per_pixel != 16)
                continue;
            chroma = VLC_FOURCC ('R', 'V', '1', '5');
            break;
          case 8:
            if (fmt->bits_per_pixel != 8)
                continue;
            chroma = VLC_FOURCC ('R', 'G', 'B', '2');
            break;
          default:
            continue;
        }
        if ((fmt->bits_per_pixel << 4) % fmt->scanline_pad)
            continue; /* VLC pads lines to 16 pixels internally */

        /* Byte sex is a non-issue for 8-bits. It can be worked around with
         * RGB masks for 24-bits. Too bad for 15-bits and 16-bits. */
        if (fmt->bits_per_pixel == 16 && setup->image_byte_order != ORDER)
            continue;

        /* Check that the selected screen supports this depth */
        xcb_depth_iterator_t it = xcb_screen_allowed_depths_iterator (scr);
        while (it.rem > 0 && it.data->depth != fmt->depth)
             xcb_depth_next (&it);
        if (!it.rem)
            continue; /* Depth not supported on this screen */

        /* Find a visual type for the selected depth */
        const xcb_visualtype_t *vt = xcb_depth_visuals (it.data);
        for (int i = xcb_depth_visuals_length (it.data); i > 0; i--)
        {
            if (vt->_class == XCB_VISUAL_CLASS_TRUE_COLOR)
            {
                vid = vt->visual_id;
                gray = false;
                break;
            }
            if (fmt->depth == 8 && vt->_class == XCB_VISUAL_CLASS_STATIC_GRAY)
            {
                if (!gray)
                    continue; /* Prefer color over gray scale */
                vid = vt->visual_id;
                chroma = VLC_FOURCC ('G', 'R', 'E', 'Y');
            }
        }

        if (!vid)
            continue; /* The screen does not *really* support this depth */

        vout->fmt_out.i_chroma = vout->output.i_chroma = chroma;
        if (!gray)
        {
            vout->fmt_out.i_rmask = vout->output.i_rmask = vt->red_mask;
            vout->fmt_out.i_gmask = vout->output.i_gmask = vt->green_mask;
            vout->fmt_out.i_bmask = vout->output.i_bmask = vt->blue_mask;
        }
        p_sys->bpp = fmt->bits_per_pixel;
        p_sys->pad = fmt->scanline_pad;
        p_sys->depth = depth = fmt->depth;
    }

    if (depth == 0)
    {
        msg_Err (vout, "no supported pixmap formats or visual types");
        goto error;
    }

    msg_Dbg (vout, "using X11 visual ID 0x%"PRIx32, vid);
    msg_Dbg (vout, " %"PRIu8" bits per pixels, %"PRIu8" bits line pad",
             p_sys->bpp, p_sys->pad);

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
    {
        const uint32_t mask = XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
        const uint32_t values[] = {
            /* XCB_CW_EVENT_MASK */
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            /* XCB_CW_COLORMAP */
            cmap,
        };
        xcb_void_cookie_t c;
        xcb_window_t window = xcb_generate_id (p_sys->conn);

        c = xcb_create_window_checked (p_sys->conn, depth, window,
                                       p_sys->embed->handle.xid, 0, 0, 1, 1, 0,
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       vid, mask, values);
        if (CheckError (vout, "cannot create X11 window", c))
            goto error;
        p_sys->window = window;
        msg_Dbg (vout, "using X11 window %08"PRIx32, p_sys->window);
        xcb_map_window (p_sys->conn, window);
    }

    /* Create graphic context (I wonder why the heck do we need this) */
    p_sys->gc = xcb_generate_id (p_sys->conn);
    xcb_create_gc (p_sys->conn, p_sys->gc, p_sys->window, 0, NULL);
    msg_Dbg (vout, "using X11 graphic context %08"PRIx32, p_sys->gc);

    vout->pf_init = Init;
    vout->pf_end = Deinit;
    vout->pf_display = Display;
    vout->pf_manage = Manage;
    vout->pf_control = Control;
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
    vout_thread_t *vout = (vout_thread_t *)obj;
    vout_sys_t *p_sys = vout->p_sys;

    vout_ReleaseWindow (p_sys->embed);
    /* colormap and window are garbage-collected by X */
    xcb_disconnect (p_sys->conn);
    free (p_sys);
}


/**
 * Allocate drawable window and picture buffers.
 */
static int Init (vout_thread_t *vout)
{
    vout_sys_t *p_sys = vout->p_sys;
    unsigned x, y, width, height;

    if (GetWindowSize (p_sys->embed, p_sys->conn, &width, &height))
        return VLC_EGENERIC;
    vout_PlacePicture (vout, width, height, &x, &y, &width, &height);

    const uint32_t values[] = { x, y, width, height, };
    xcb_configure_window (p_sys->conn, p_sys->window,
                          XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                          XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                          values);

    /* FIXME: I don't get the subtlety between output and fmt_out here */
    vout->fmt_out.i_visible_width = width;
    vout->fmt_out.i_visible_height = height;
    vout->fmt_out.i_sar_num = vout->fmt_out.i_sar_den = 1;

    vout->output.i_width = vout->fmt_out.i_width =
        width * vout->fmt_in.i_width / vout->fmt_in.i_visible_width;
    vout->output.i_height = vout->fmt_out.i_height =
        height * vout->fmt_in.i_height / vout->fmt_in.i_visible_height;
    vout->fmt_out.i_x_offset =
        width * vout->fmt_in.i_x_offset / vout->fmt_in.i_visible_width;
    p_vout->fmt_out.i_y_offset =
        height * vout->fmt_in.i_y_offset / vout->fmt_in.i_visible_height;

    assert (height > 0);
    vout->output.i_aspect = vout->fmt_out.i_aspect =
        width * VOUT_ASPECT_FACTOR / height;

    /* Allocate picture buffers */
    I_OUTPUTPICTURES = 0;
    for (size_t index = 0; I_OUTPUTPICTURES < 2; index++)
    {
        picture_t *pic = vout->p_picture + index;

        if (index > sizeof (vout->p_picture) / sizeof (pic))
            break;
        if (pic->i_status != FREE_PICTURE)
            continue;

        vout_InitPicture (vout, pic, vout->output.i_chroma,
                          vout->output.i_width, vout->output.i_height,
                          vout->output.i_aspect);
        if (PictureAlloc (vout, pic, pic->p->i_pitch * pic->p->i_lines,
                          p_sys->shm ? p_sys->conn : NULL))
            break;
        PP_OUTPUTPICTURE[I_OUTPUTPICTURES++] = pic;
    }
    xcb_flush (p_sys->conn);
    return VLC_SUCCESS;
}

/**
 * Free picture buffers.
 */
static void Deinit (vout_thread_t *vout)
{
    for (int i = 0; i < I_OUTPUTPICTURES; i++)
        PictureFree (PP_OUTPUTPICTURE[i], vout->p_sys->conn);
}

/**
 * Sends an image to the X server.
 */
static void Display (vout_thread_t *vout, picture_t *pic)
{
    vout_sys_t *p_sys = vout->p_sys;
    xcb_shm_seg_t segment = (uintptr_t)pic->p_sys;

    if (segment != 0)
        xcb_shm_put_image (p_sys->conn, p_sys->window, p_sys->gc,
          /* real width */ pic->p->i_pitch / pic->p->i_pixel_pitch,
         /* real height */ pic->p->i_lines,
                   /* x */ vout->fmt_out.i_x_offset,
                   /* y */ vout->fmt_out.i_y_offset,
               /* width */ vout->fmt_out.i_visible_width,
              /* height */ vout->fmt_out.i_visible_height,
                           0, 0, p_sys->depth, XCB_IMAGE_FORMAT_Z_PIXMAP,
                           0, segment, 0);
    else
    {
        const size_t offset = vout->fmt_out.i_y_offset * pic->p->i_pitch;
        const unsigned lines = pic->p->i_lines - vout->fmt_out.i_y_offset;

        xcb_put_image (p_sys->conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
                       p_sys->window, p_sys->gc,
                       pic->p->i_pitch / pic->p->i_pixel_pitch,
                       lines, -vout->fmt_out.i_x_offset, 0, 0, p_sys->depth,
                       pic->p->i_pitch * lines, pic->p->p_pixels + offset);
    }
    xcb_flush (p_sys->conn);
}

/**
 * Process incoming X events.
 */
static int Manage (vout_thread_t *vout)
{
    vout_sys_t *p_sys = vout->p_sys;
    xcb_generic_event_t *ev;

    while ((ev = xcb_poll_for_event (p_sys->conn)) != NULL)
        ProcessEvent (vout, p_sys->conn, p_sys->window, ev);

    if (xcb_connection_has_error (p_sys->conn))
    {
        msg_Err (vout, "X server failure");
        return VLC_EGENERIC;
    }

    CommonManage (vout); /* FIXME: <-- move that to core */
    return VLC_SUCCESS;
}

void
HandleParentStructure (vout_thread_t *vout, xcb_connection_t *conn,
                       xcb_window_t xid, xcb_configure_notify_event_t *ev)
{
    unsigned width, height, x, y;

    vout_PlacePicture (vout, ev->width, ev->height, &x, &y, &width, &height);
    if (width != vout->fmt_out.i_visible_width
     || height != vout->fmt_out.i_visible_height)
    {
        vout->i_changes |= VOUT_SIZE_CHANGE;
        return; /* vout will be reinitialized */
    }

    /* Move the picture within the window */
    const uint32_t values[] = { x, y, };
    xcb_configure_window (conn, xid,
                          XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                          values);
}

static int Control (vout_thread_t *vout, int query, va_list ap)
{
    return vout_ControlWindow (vout->p_sys->embed, query, ap);
}
