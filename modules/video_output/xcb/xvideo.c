/**
 * @file xvideo.c
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
#include <xcb/xv.h>

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
    set_shortname (N_("XVideo"))
    set_description (N_("(Experimental) XVideo output"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("video output", 0)
    set_callbacks (Open, Close)

    add_string ("x11-display", NULL, NULL,
                DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
    add_bool ("x11-shm", true, NULL, SHM_TEXT, SHM_LONGTEXT, true)
    add_shortcut ("xcb-xv")
vlc_module_end ()

struct vout_sys_t
{
    xcb_connection_t *conn;
    xcb_xv_query_adaptors_reply_t *adaptors;
    vout_window_t *embed;/* VLC window */

    xcb_window_t window; /* drawable X window */
    xcb_gcontext_t gc;   /* context to put images */
    xcb_xv_port_t port;  /* XVideo port */
    uint32_t id;         /* XVideo format */
    uint16_t width;      /* display width */
    uint16_t height;     /* display height */
    uint32_t data_size;  /* picture byte size (for non-SHM) */
    bool shm;            /* whether to use MIT-SHM */
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

/**
 * Check that the X server supports the XVideo extension.
 */
static bool CheckXVideo (vout_thread_t *vout, xcb_connection_t *conn)
{
    xcb_xv_query_extension_reply_t *r;
    xcb_xv_query_extension_cookie_t ck = xcb_xv_query_extension (conn);
    bool ok = false;

    r = xcb_xv_query_extension_reply (conn, ck, NULL);
    if (r != NULL)
    {   /* We need XVideo 2.2 for PutImage */
        if ((r->major > 2) || (r->major == 2 && r->minor >= 2))
        {
            msg_Dbg (vout, "using XVideo extension v%"PRIu8".%"PRIu8,
                     r->major, r->minor);
            ok = true;
        }
        else
            msg_Dbg (vout, "XVideo extension too old (v%"PRIu8".%"PRIu8,
                     r->major, r->minor);
        free (r);
    }
    else
        msg_Dbg (vout, "XVideo extension not available");
    return ok;
}

/**
 * Get a list of XVideo adaptors for a given window.
 */
static xcb_xv_query_adaptors_reply_t *GetAdaptors (vout_window_t *wnd,
                                                   xcb_connection_t *conn)
{
    xcb_xv_query_adaptors_cookie_t ck;

    ck = xcb_xv_query_adaptors (conn, wnd->handle.xid);
    return xcb_xv_query_adaptors_reply (conn, ck, NULL);
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

    if (!CheckXVideo (vout, p_sys->conn))
    {
        msg_Warn (vout, "Please enable XVideo 2.2 for faster video display");
        xcb_disconnect (p_sys->conn);
        free (p_sys);
        return VLC_EGENERIC;
    }

    const xcb_screen_t *screen;
    p_sys->embed = GetWindow (vout, p_sys->conn, &screen, &p_sys->shm);
    if (p_sys->embed == NULL)
    {
        xcb_disconnect (p_sys->conn);
        free (p_sys);
        return VLC_EGENERIC;
    }

    /* Cache adaptors infos */
    p_sys->adaptors = GetAdaptors (p_sys->embed, p_sys->conn);
    if (p_sys->adaptors == NULL)
        goto error;

    /* Create window */
    {
        const uint32_t mask =
            /* XCB_CW_EVENT_MASK */
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION;
        xcb_void_cookie_t c;
        xcb_window_t window = xcb_generate_id (p_sys->conn);

        c = xcb_create_window_checked (p_sys->conn, screen->root_depth, window,
                                       p_sys->embed->handle.xid, 0, 0, 1, 1, 0,
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       screen->root_visual,
                                       XCB_CW_EVENT_MASK, &mask);
        if (CheckError (vout, "cannot create X11 window", c))
            goto error;
        p_sys->window = window;
        msg_Dbg (vout, "using X11 window %08"PRIx32, p_sys->window);
        xcb_map_window (p_sys->conn, window);
    }

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

    free (p_sys->adaptors);
    vout_ReleaseWindow (p_sys->embed);
    xcb_disconnect (p_sys->conn);
    free (p_sys);
}

static vlc_fourcc_t ParseFormat (vout_thread_t *vout,
                                 const xcb_xv_image_format_info_t *restrict f)
{
    if (f->byte_order != ORDER && f->bpp != 8)
        return 0; /* Argh! */

    switch (f->type)
    {
      case XCB_XV_IMAGE_FORMAT_INFO_TYPE_RGB:
        switch (f->num_planes)
        {
          case 1:
            switch (f->bpp)
            {
              case 32:
                if (f->depth == 24)
                    return VLC_FOURCC ('R', 'V', '3', '2');
                break;
              case 24:
                if (f->depth == 24)
                    return VLC_FOURCC ('R', 'V', '2', '4');
                break;
              case 16:
                if (f->depth == 16)
                    return VLC_FOURCC ('R', 'V', '1', '6');
                if (f->depth == 15)
                    return VLC_FOURCC ('R', 'V', '1', '5');
                break;
              case 8:
                if (f->depth == 8)
                    return VLC_FOURCC ('R', 'G', 'B', '2');
                break;
            }
            break;
        }
        msg_Err (vout, "unknown XVideo RGB format %"PRIx32" (%.4s)",
                 f->id, f->guid);
        msg_Dbg (vout, " %"PRIu8" planes, %"PRIu8" bits/pixel, "
                 "depth %"PRIu8, f->num_planes, f->bpp, f->depth);
        break;

      case XCB_XV_IMAGE_FORMAT_INFO_TYPE_YUV:
        if (f->u_sample_bits != f->v_sample_bits
         || f->vhorz_u_period != f->vhorz_v_period
         || f->vvert_u_period != f->vvert_v_period
         || f->y_sample_bits != 8 || f->u_sample_bits != 8
         || f->vhorz_y_period != 1 || f->vvert_y_period != 1)
            goto bad;
        switch (f->num_planes)
        {
          case 1:
            switch (f->bpp)
            {
              /*untested: case 24:
                if (f->vhorz_u_period == 1 && f->vvert_u_period == 1)
                    return VLC_FOURCC ('I', '4', '4', '4');
                break;*/
              case 16:
                if (f->vhorz_u_period == 2 && f->vvert_u_period == 1)
                {
                    if (!strcmp ((const char *)f->vcomp_order, "YUYV"))
                        return VLC_FOURCC ('Y', 'U', 'Y', '2');
                    if (!strcmp ((const char *)f->vcomp_order, "UYVY"))
                        return VLC_FOURCC ('U', 'Y', 'V', 'Y');
                }
                break;
            }
            break;
          case 3:
            switch (f->bpp)
            {
              case 12:
                if (f->vhorz_u_period == 2 && f->vvert_u_period == 2)
                {
                    if (!strcmp ((const char *)f->vcomp_order, "YVU"))
                        return VLC_FOURCC ('Y', 'V', '1', '2');
                    if (!strcmp ((const char *)f->vcomp_order, "YUV"))
                        return VLC_FOURCC ('I', '4', '2', '0');
                }
            }
            break;
        }
    bad:
        msg_Err (vout, "unknown XVideo YUV format %"PRIx32" (%.4s)", f->id,
                 f->guid);
        msg_Dbg (vout, " %"PRIu8" planes, %"PRIu32" bits/pixel, "
                 "%"PRIu32"/%"PRIu32"/%"PRIu32" bits/sample", f->num_planes,
                 f->bpp, f->y_sample_bits, f->u_sample_bits, f->v_sample_bits);
        msg_Dbg (vout, " period: %"PRIu32"/%"PRIu32"/%"PRIu32"x"
                 "%"PRIu32"/%"PRIu32"/%"PRIu32,
                 f->vhorz_y_period, f->vhorz_u_period, f->vhorz_v_period,
                 f->vvert_y_period, f->vvert_u_period, f->vvert_v_period);
        msg_Warn (vout, " order: %.32s", f->vcomp_order);
        break;
    }
    return 0;
}


static const xcb_xv_image_format_info_t *
FindFormat (vout_thread_t *vout, vlc_fourcc_t chroma, xcb_xv_port_t port,
            const xcb_xv_list_image_formats_reply_t *list,
            xcb_xv_query_image_attributes_reply_t **restrict pa)
{
    xcb_connection_t *conn = vout->p_sys->conn;
    const xcb_xv_image_format_info_t *f, *end;

    f = xcb_xv_list_image_formats_format (list);
    end = f + xcb_xv_list_image_formats_format_length (list);
    for (; f < end; f++)
    {
        if (chroma != ParseFormat (vout, f))
            continue;

        xcb_xv_query_image_attributes_reply_t *i;
        i = xcb_xv_query_image_attributes_reply (conn,
            xcb_xv_query_image_attributes (conn, port, f->id,
                vout->fmt_in.i_width, vout->fmt_in.i_height), NULL);
        if (i == NULL)
            continue;

        if (i->width != vout->fmt_in.i_width
         || i->height != vout->fmt_in.i_height)
        {
            msg_Warn (vout, "incompatible size %ux%u -> %"PRIu32"x%"PRIu32,
                      vout->fmt_in.i_width, vout->fmt_in.i_height,
                      i->width, i->height);
            free (i);
            continue;
        }
        *pa = i;
        return f;
    }
    return NULL;
}

/**
 * Allocate drawable window and picture buffers.
 */
static int Init (vout_thread_t *vout)
{
    vout_sys_t *p_sys = vout->p_sys;
    xcb_xv_query_image_attributes_reply_t *att = NULL;
    bool swap_planes = false; /* whether X wants V before U */

    /* FIXME: check max image size */
    xcb_xv_adaptor_info_iterator_t it;
    for (it = xcb_xv_query_adaptors_info_iterator (p_sys->adaptors);
         it.rem > 0;
         xcb_xv_adaptor_info_next (&it))
    {
        const xcb_xv_adaptor_info_t *a = it.data;

        /* FIXME: Open() should fail if none of the ports are usable to VLC */
        if (!(a->type & XCB_XV_TYPE_IMAGE_MASK))
            continue;

        xcb_xv_list_image_formats_reply_t *r;
        r = xcb_xv_list_image_formats_reply (p_sys->conn,
            xcb_xv_list_image_formats (p_sys->conn, a->base_id), NULL);
        if (r == NULL)
            continue;

        const xcb_xv_image_format_info_t *fmt;

        /* Video chroma in preference order */
        const vlc_fourcc_t chromas[] = {
            vout->fmt_in.i_chroma,
            VLC_FOURCC ('Y', 'U', 'Y', '2'),
            VLC_FOURCC ('R', 'V', '2', '4'),
            VLC_FOURCC ('R', 'V', '1', '5'),
        };
        for (size_t i = 0; i < sizeof (chromas) / sizeof (chromas[0]); i++)
        {
            vlc_fourcc_t chroma = chromas[i];
            fmt = FindFormat (vout, chroma, a->base_id, r, &att);
            if (fmt != NULL)
            {
                vout->output.i_chroma = chroma;
                goto found_format;
            }
        }
        free (r);
        continue;

    found_format:
        /* TODO: grab port */
        p_sys->port = a->base_id;
        msg_Dbg (vout, "using port %"PRIu32, p_sys->port);

        p_sys->id = fmt->id;
        msg_Dbg (vout, "using image format 0x%"PRIx32, p_sys->id);
        if (fmt->type == XCB_XV_IMAGE_FORMAT_INFO_TYPE_RGB)
        {
            vout->fmt_out.i_rmask = vout->output.i_rmask = fmt->red_mask;
            vout->fmt_out.i_gmask = vout->output.i_gmask = fmt->green_mask;
            vout->fmt_out.i_bmask = vout->output.i_bmask = fmt->blue_mask;
        }
        else
        if (fmt->num_planes == 3)
            swap_planes = !strcmp ((const char *)fmt->vcomp_order, "YVU");
        free (r);
        goto found_adaptor;
    }
    msg_Err (vout, "no available XVideo adaptor");
    return VLC_EGENERIC; /* no usable adaptor */

    /* Allocate picture buffers */
    const uint32_t *offsets;
found_adaptor:
    offsets = xcb_xv_query_image_attributes_offsets (att);
    p_sys->data_size = att->data_size;

    I_OUTPUTPICTURES = 0;
    for (size_t index = 0; I_OUTPUTPICTURES < 2; index++)
    {
        picture_t *pic = vout->p_picture + index;

        if (index > sizeof (vout->p_picture) / sizeof (pic))
            break;
        if (pic->i_status != FREE_PICTURE)
            continue;

        vout_InitPicture (vout, pic, vout->output.i_chroma,
                          att->width, att->height,
                          vout->fmt_in.i_aspect);
        if (PictureAlloc (vout, pic, att->data_size,
                          p_sys->shm ? p_sys->conn : NULL))
            break;
        /* Allocate further planes as specified by XVideo */
        /* We assume that offsets[0] is zero */
        for (int i = 1; i < pic->i_planes; i++)
             pic->p[i].p_pixels =
                 pic->p->p_pixels + offsets[swap_planes ? (3 - i) : i];
        PP_OUTPUTPICTURE[I_OUTPUTPICTURES++] = pic;
    }
    free (att);

    unsigned x, y, width, height;

    if (GetWindowSize (p_sys->embed, p_sys->conn, &width, &height))
        return VLC_EGENERIC;
    vout_PlacePicture (vout, width, height, &x, &y, &width, &height);

    const uint32_t values[] = { x, y, width, height, };
    xcb_configure_window (p_sys->conn, p_sys->window,
                          XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                          XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                          values);
    xcb_flush (p_sys->conn);
    p_sys->height = height;
    p_sys->width = width;

    vout->fmt_out.i_chroma = vout->output.i_chroma;
    vout->fmt_out.i_visible_width = vout->fmt_in.i_visible_width;
    vout->fmt_out.i_visible_height = vout->fmt_in.i_visible_height;
    vout->fmt_out.i_sar_num = vout->fmt_out.i_sar_den = 1;

    vout->output.i_width = vout->fmt_out.i_width = vout->fmt_in.i_width;
    vout->output.i_height = vout->fmt_out.i_height = vout->fmt_in.i_height;
    vout->fmt_out.i_x_offset = vout->fmt_in.i_x_offset;
    vout->fmt_out.i_y_offset = vout->fmt_in.i_y_offset;

    assert (height > 0);
    vout->output.i_aspect = vout->fmt_out.i_aspect =
        width * VOUT_ASPECT_FACTOR / height;

    return VLC_SUCCESS;
}

/**
 * Free picture buffers.
 */
static void Deinit (vout_thread_t *vout)
{
    vout_sys_t *p_sys = vout->p_sys;

    for (int i = 0; i < I_OUTPUTPICTURES; i++)
        PictureFree (PP_OUTPUTPICTURE[i], p_sys->conn);
}

/**
 * Sends an image to the X server.
 */
static void Display (vout_thread_t *vout, picture_t *pic)
{
    vout_sys_t *p_sys = vout->p_sys;
    xcb_shm_seg_t segment = (uintptr_t)pic->p_sys;

    if (segment)
        xcb_xv_shm_put_image (p_sys->conn, p_sys->port, p_sys->window,
                              p_sys->gc, segment, p_sys->id, 0,
                              /* Src: */ vout->fmt_out.i_x_offset,
                              vout->fmt_out.i_y_offset,
                              vout->fmt_out.i_visible_width,
                              vout->fmt_out.i_visible_height,
                              /* Dst: */ 0, 0, p_sys->width, p_sys->height,
                              /* Memory: */
                              pic->p->i_pitch / pic->p->i_pixel_pitch,
                              pic->p->i_lines, false);
    else
        xcb_xv_put_image (p_sys->conn, p_sys->port, p_sys->window,
                          p_sys->gc, p_sys->id,
                          vout->fmt_out.i_x_offset, vout->fmt_out.i_y_offset,
                          vout->fmt_out.i_visible_width,
                          vout->fmt_out.i_visible_height,
                          0, 0, p_sys->width, p_sys->height,
                          vout->fmt_out.i_width, vout->fmt_out.i_height,
                          p_sys->data_size, pic->p->p_pixels);
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

    CommonManage (vout);
    if (vout->i_changes & VOUT_SIZE_CHANGE)
    {   /* TODO: factor this code with XV and X11 Init() */
        unsigned x, y, width, height;

        if (GetWindowSize (p_sys->embed, p_sys->conn, &width, &height))
            return VLC_EGENERIC;
        vout_PlacePicture (vout, width, height, &x, &y, &width, &height);

        const uint32_t values[] = { x, y, width, height, };
        xcb_configure_window (p_sys->conn, p_sys->window, XCB_CONFIG_WINDOW_X |
                              XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                              XCB_CONFIG_WINDOW_HEIGHT, values);
        vout->p_sys->width = width; // XXX: <-- this is useless, as the zoom is
        vout->p_sys->height = height; // handled with VOUT_SET_SIZE anyway.
        vout->i_changes &= ~VOUT_SIZE_CHANGE;
    }
    return VLC_SUCCESS;
}

void
HandleParentStructure (vout_thread_t *vout, xcb_connection_t *conn,
                       xcb_window_t xid, xcb_configure_notify_event_t *ev)
{
    unsigned width, height, x, y;

    vout_PlacePicture (vout, ev->width, ev->height, &x, &y, &width, &height);

    /* Move the picture within the window */
    const uint32_t values[] = { x, y, width, height, };
    xcb_configure_window (conn, xid,
                          XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                        | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                          values);
    vout->p_sys->width = width;
    vout->p_sys->height = height;
}

static int Control (vout_thread_t *vout, int query, va_list ap)
{
    return vout_ControlWindow (vout->p_sys->embed, query, ap);
}
