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
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include "xcb_vlc.h"

#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
    "X11 hardware display to use. By default, VLC will " \
    "use the value of the DISPLAY environment variable.")

#define ADAPTOR_TEXT N_("XVideo adaptor number")
#define ADAPTOR_LONGTEXT N_( \
    "XVideo hardware adaptor to use. By default, VLC will " \
    "use the first functional adaptor.")

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
    set_description (N_("XVideo output (XCB)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", 155)
    set_callbacks (Open, Close)

    add_string ("x11-display", NULL, NULL,
                DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
        add_deprecated_alias ("xvideo-display")
    add_integer ("xvideo-adaptor", -1, NULL,
                 ADAPTOR_TEXT, ADAPTOR_LONGTEXT, true)
    add_bool ("x11-shm", true, NULL, SHM_TEXT, SHM_LONGTEXT, true)
        add_deprecated_alias ("xvideo-shm")
    add_shortcut ("xcb-xv")
    add_shortcut ("xv")
    add_shortcut ("xvideo")
vlc_module_end ()

#define MAX_PICTURES (VOUT_MAX_PICTURES)

struct vout_display_sys_t
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

    xcb_xv_query_image_attributes_reply_t *att;
    picture_pool_t *pool; /* picture pool */
    picture_resource_t resource[MAX_PICTURES];
};

static picture_t *Get (vout_display_t *);
static void Display (vout_display_t *, picture_t *);
static int Control (vout_display_t *, int, va_list);
static void Manage (vout_display_t *);

/**
 * Check that the X server supports the XVideo extension.
 */
static bool CheckXVideo (vout_display_t *vd, xcb_connection_t *conn)
{
    xcb_xv_query_extension_reply_t *r;
    xcb_xv_query_extension_cookie_t ck = xcb_xv_query_extension (conn);
    bool ok = false;

    r = xcb_xv_query_extension_reply (conn, ck, NULL);
    if (r != NULL)
    {   /* We need XVideo 2.2 for PutImage */
        if ((r->major > 2) || (r->major == 2 && r->minor >= 2))
        {
            msg_Dbg (vd, "using XVideo extension v%"PRIu8".%"PRIu8,
                     r->major, r->minor);
            ok = true;
        }
        else
            msg_Dbg (vd, "XVideo extension too old (v%"PRIu8".%"PRIu8,
                     r->major, r->minor);
        free (r);
    }
    else
        msg_Dbg (vd, "XVideo extension not available");
    return ok;
}

static vlc_fourcc_t ParseFormat (vout_display_t *vd,
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
                    return VLC_CODEC_RGB32;
                break;
              case 24:
                if (f->depth == 24)
                    return VLC_CODEC_RGB24;
                break;
              case 16:
                if (f->depth == 16)
                    return VLC_CODEC_RGB16;
                if (f->depth == 15)
                    return VLC_CODEC_RGB15;
                break;
              case 8:
                if (f->depth == 8)
                    return VLC_CODEC_RGB8;
                break;
            }
            break;
        }
        msg_Err (vd, "unknown XVideo RGB format %"PRIx32" (%.4s)",
                 f->id, f->guid);
        msg_Dbg (vd, " %"PRIu8" planes, %"PRIu8" bits/pixel, "
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
                    return VLC_CODEC_I444;
                break;*/
              case 16:
                if (f->vhorz_u_period == 2 && f->vvert_u_period == 1)
                {
                    if (!strcmp ((const char *)f->vcomp_order, "YUYV"))
                        return VLC_CODEC_YUYV;
                    if (!strcmp ((const char *)f->vcomp_order, "UYVY"))
                        return VLC_CODEC_UYVY;
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
                        return VLC_CODEC_YV12;
                    if (!strcmp ((const char *)f->vcomp_order, "YUV"))
                        return VLC_CODEC_I420;
                }
            }
            break;
        }
    bad:
        msg_Err (vd, "unknown XVideo YUV format %"PRIx32" (%.4s)", f->id,
                 f->guid);
        msg_Dbg (vd, " %"PRIu8" planes, %"PRIu32" bits/pixel, "
                 "%"PRIu32"/%"PRIu32"/%"PRIu32" bits/sample", f->num_planes,
                 f->bpp, f->y_sample_bits, f->u_sample_bits, f->v_sample_bits);
        msg_Dbg (vd, " period: %"PRIu32"/%"PRIu32"/%"PRIu32"x"
                 "%"PRIu32"/%"PRIu32"/%"PRIu32,
                 f->vhorz_y_period, f->vhorz_u_period, f->vhorz_v_period,
                 f->vvert_y_period, f->vvert_u_period, f->vvert_v_period);
        msg_Warn (vd, " order: %.32s", f->vcomp_order);
        break;
    }
    return 0;
}


static const xcb_xv_image_format_info_t *
FindFormat (vout_display_t *vd,
            vlc_fourcc_t chroma, const video_format_t *fmt,
            xcb_xv_port_t port,
            const xcb_xv_list_image_formats_reply_t *list,
            xcb_xv_query_image_attributes_reply_t **restrict pa)
{
    xcb_connection_t *conn = vd->sys->conn;
    const xcb_xv_image_format_info_t *f, *end;

#ifndef XCB_XV_OLD
    f = xcb_xv_list_image_formats_format (list);
#else
    f = (xcb_xv_image_format_info_t *) (list + 1);
#endif
    end = f + xcb_xv_list_image_formats_format_length (list);
    for (; f < end; f++)
    {
        if (chroma != ParseFormat (vd, f))
            continue;

        xcb_xv_query_image_attributes_reply_t *i;
        i = xcb_xv_query_image_attributes_reply (conn,
            xcb_xv_query_image_attributes (conn, port, f->id,
                fmt->i_width, fmt->i_height), NULL);
        if (i == NULL)
            continue;

        if (i->width != fmt->i_width
         || i->height != fmt->i_height)
        {
            msg_Warn (vd, "incompatible size %ux%u -> %"PRIu32"x%"PRIu32,
                      fmt->i_width, fmt->i_height,
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
 * Get a list of XVideo adaptors for a given window.
 */
static xcb_xv_query_adaptors_reply_t *GetAdaptors (vout_window_t *wnd,
                                                   xcb_connection_t *conn)
{
    xcb_xv_query_adaptors_cookie_t ck;

    ck = xcb_xv_query_adaptors (conn, wnd->handle.xid);
    return xcb_xv_query_adaptors_reply (conn, ck, NULL);
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

    /* Connect to X */
    p_sys->conn = Connect (obj);
    if (p_sys->conn == NULL)
    {
        free (p_sys);
        return VLC_EGENERIC;
    }

    if (!CheckXVideo (vd, p_sys->conn))
    {
        msg_Warn (vd, "Please enable XVideo 2.2 for faster video display");
        xcb_disconnect (p_sys->conn);
        free (p_sys);
        return VLC_EGENERIC;
    }

    const xcb_screen_t *screen;
    p_sys->embed = GetWindow (vd, p_sys->conn, &screen, &p_sys->shm);
    if (p_sys->embed == NULL)
    {
        xcb_disconnect (p_sys->conn);
        free (p_sys);
        return VLC_EGENERIC;
    }

    /* */
    p_sys->att = NULL;
    p_sys->pool = NULL;

    /* Cache adaptors infos */
    p_sys->adaptors = GetAdaptors (p_sys->embed, p_sys->conn);
    if (p_sys->adaptors == NULL)
        goto error;

    int forced_adaptor = var_CreateGetInteger (obj, "xvideo-adaptor");

    /* */
    video_format_t fmt = vd->fmt;
    bool found_adaptor = false;

    /* FIXME: check max image size */
    xcb_xv_adaptor_info_iterator_t it;
    for (it = xcb_xv_query_adaptors_info_iterator (p_sys->adaptors);
         it.rem > 0;
         xcb_xv_adaptor_info_next (&it))
    {
        const xcb_xv_adaptor_info_t *a = it.data;

        if (forced_adaptor != -1 && forced_adaptor != 0)
        {
            forced_adaptor--;
            continue;
        }

        /* FIXME: Open() should fail if none of the ports are usable to VLC */
        if (!(a->type & XCB_XV_TYPE_IMAGE_MASK))
            continue;

        xcb_xv_list_image_formats_reply_t *r;
        r = xcb_xv_list_image_formats_reply (p_sys->conn,
            xcb_xv_list_image_formats (p_sys->conn, a->base_id), NULL);
        if (r == NULL)
            continue;

        const xcb_xv_image_format_info_t *xfmt;

        /* */
        const vlc_fourcc_t *chromas, chromas_default[] = {
            fmt.i_chroma,
            VLC_CODEC_RGB24,
            VLC_CODEC_RGB15,
            VLC_CODEC_YUYV,
            0
        };
        if (vlc_fourcc_IsYUV (fmt.i_chroma))
            chromas = vlc_fourcc_GetYUVFallback (fmt.i_chroma);
        else
            chromas = chromas_default;

        for (size_t i = 0; chromas[i]; i++)
        {
            vlc_fourcc_t chroma = chromas[i];
            xfmt = FindFormat (vd, chroma, &fmt, a->base_id, r, &p_sys->att);
            if (xfmt != NULL)
            {
                fmt.i_chroma = chroma;
                goto found_format;
            }
        }
        free (r);
        continue;

    found_format:
        /* Grab a port */
        /* XXX: assume all of an adapter's ports have the same formats */
        for (unsigned i = 0; i < a->num_ports; i++)
        {
             xcb_xv_port_t port = a->base_id + i;
             xcb_xv_grab_port_reply_t *gr =
                 xcb_xv_grab_port_reply (p_sys->conn,
                     xcb_xv_grab_port (p_sys->conn, port,
                                       XCB_CURRENT_TIME), NULL);
             uint8_t result = gr ? gr->result : 0xff;

             free (gr);
             if (result == 0)
             {
                 p_sys->port = port;
                 goto grabbed_port;
             }
             msg_Dbg (vd, "cannot grab port %"PRIu32, port);
        }
        continue;

    grabbed_port:
        msg_Dbg (vd, "using port %"PRIu32, p_sys->port);

        p_sys->id = xfmt->id;
        msg_Dbg (vd, "using image format 0x%"PRIx32, p_sys->id);
        if (xfmt->type == XCB_XV_IMAGE_FORMAT_INFO_TYPE_RGB)
        {
            fmt.i_rmask = xfmt->red_mask;
            fmt.i_gmask = xfmt->green_mask;
            fmt.i_bmask = xfmt->blue_mask;
        }
        else
        if (xfmt->num_planes == 3
         && !strcmp ((const char *)xfmt->vcomp_order, "YVU"))
            fmt.i_chroma = VLC_CODEC_YV12;
        free (r);
        found_adaptor = true;
        break;
    }
    if (!found_adaptor)
    {
        msg_Err (vd, "no available XVideo adaptor");
        goto error;
    }

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
        if (CheckError (vd, p_sys->conn, "cannot create X11 window", c))
            goto error;
        p_sys->window = window;
        msg_Dbg (vd, "using X11 window %08"PRIx32, p_sys->window);
        xcb_map_window (p_sys->conn, window);

        vout_display_place_t place;

        vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);
        p_sys->width  = place.width;
        p_sys->height = place.height;

        /* */
        const uint32_t values[] = { place.x, place.y, place.width, place.height };
        xcb_configure_window (p_sys->conn, p_sys->window,
                              XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                              XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                              values);
    }

    /* Create graphic context */
    p_sys->gc = xcb_generate_id (p_sys->conn);
    xcb_create_gc (p_sys->conn, p_sys->gc, p_sys->window, 0, NULL);
    msg_Dbg (vd, "using X11 graphic context %08"PRIx32, p_sys->gc);

    /* */
    p_sys->pool = NULL;

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = false;

    /* Setup vout_display_t once everything is fine */
    vd->fmt = fmt;
    vd->info = info;

    vd->get = Get;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage = Manage;

    /* */
    unsigned width, height;
    if (!GetWindowSize (p_sys->embed, p_sys->conn, &width, &height))
        vout_display_SendEventDisplaySize (vd, width, height);
    vout_display_SendEventFullscreen (vd, false);

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

    if (p_sys->pool)
    {
        for (unsigned i = 0; i < MAX_PICTURES; i++)
        {
            picture_resource_t *res = &p_sys->resource[i];

            if (!res->p->p_pixels)
                break;
            PictureResourceFree (res, p_sys->conn);
        }
        picture_pool_Delete (p_sys->pool);
    }

    free (p_sys->att);
    free (p_sys->adaptors);
    vout_display_DeleteWindow (vd, p_sys->embed);
    xcb_disconnect (p_sys->conn);
    free (p_sys);
}

/**
 * Return a direct buffer
 */
static picture_t *Get (vout_display_t *vd)
{
    vout_display_sys_t *p_sys = vd->sys;

    if (!p_sys->pool)
    {
        picture_t *pic = picture_New (vd->fmt.i_chroma,
                                      p_sys->att->width, p_sys->att->height, 0);
        if (!pic)
            return NULL;

        memset (p_sys->resource, 0, sizeof(p_sys->resource));

        const uint32_t *offsets =
            xcb_xv_query_image_attributes_offsets (p_sys->att);
        p_sys->data_size = p_sys->att->data_size;

        unsigned count;
        picture_t *pic_array[MAX_PICTURES];
        for (count = 0; count < MAX_PICTURES; count++)
        {
            picture_resource_t *res = &p_sys->resource[count];

            for (int i = 0; i < pic->i_planes; i++)
            {
                res->p[i].i_lines = pic->p[i].i_lines; /* FIXME seems wrong*/
                res->p[i].i_pitch = pic->p[i].i_pitch;
            }
            if (PictureResourceAlloc (vd, res, p_sys->att->data_size,
                                      p_sys->conn, p_sys->shm))
                break;

            /* Allocate further planes as specified by XVideo */
            /* We assume that offsets[0] is zero */
            for (int i = 1; i < pic->i_planes; i++)
                res->p[i].p_pixels = res->p[0].p_pixels + offsets[i];
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
        if (!p_sys->pool)
        {
            /* TODO release picture resources */
            return NULL;
        }
        /* FIXME should also do it in case of error ? */
        xcb_flush (p_sys->conn);
    }

    return picture_pool_Get (p_sys->pool);
}

/**
 * Sends an image to the X server.
 */
static void Display (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *p_sys = vd->sys;
    xcb_shm_seg_t segment = pic->p_sys->segment;
    xcb_void_cookie_t ck;

    if (segment)
        ck = xcb_xv_shm_put_image_checked (p_sys->conn, p_sys->port,
                              p_sys->window, p_sys->gc, segment, p_sys->id, 0,
                   /* Src: */ vd->source.i_x_offset,
                              vd->source.i_y_offset,
                              vd->source.i_visible_width,
                              vd->source.i_visible_height,
                   /* Dst: */ 0, 0, p_sys->width, p_sys->height,
                /* Memory: */ pic->p->i_pitch / pic->p->i_pixel_pitch,
                              pic->p->i_visible_lines, false);
    else
        ck = xcb_xv_put_image_checked (p_sys->conn, p_sys->port, p_sys->window,
                          p_sys->gc, p_sys->id,
                          vd->source.i_x_offset,
                          vd->source.i_y_offset,
                          vd->source.i_visible_width,
                          vd->source.i_visible_height,
                          0, 0, p_sys->width, p_sys->height,
                          vd->source.i_width, vd->source.i_height,
                          p_sys->data_size, pic->p->p_pixels);

    /* Wait for reply. See x11.c for rationale. */
    xcb_generic_error_t *e = xcb_request_check (p_sys->conn, ck);
    if (e != NULL)
    {
        msg_Dbg (vd, "%s: X11 error %d", "cannot put image", e->error_code);
        free (e);
    }

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
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    {
        const vout_display_cfg_t *cfg;
        const video_format_t *source;

        if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT
         || query == VOUT_DISPLAY_CHANGE_SOURCE_CROP)
        {
            source = (const video_format_t *)va_arg (ap, const video_format_t *);
            cfg = vd->cfg;
        }
        else
        {
            source = &vd->source;
            cfg = (const vout_display_cfg_t*)va_arg (ap, const vout_display_cfg_t *);
        }

        /* */
        if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE
         && (cfg->display.width  != vd->cfg->display.width
           ||cfg->display.height != vd->cfg->display.height)
         && vout_window_SetSize (p_sys->embed,
                                  cfg->display.width,
                                  cfg->display.height))
            return VLC_EGENERIC;

        vout_display_place_t place;
        vout_display_PlacePicture (&place, source, cfg, false);
        p_sys->width  = place.width;
        p_sys->height = place.height;

        /* Move the picture within the window */
        const uint32_t values[] = { place.x, place.y,
                                    place.width, place.height, };
        xcb_configure_window (p_sys->conn, p_sys->window,
                              XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                            | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                              values);
        xcb_flush (p_sys->conn);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_ON_TOP:
    {
        int on_top = (int)va_arg (ap, int);
        return vout_window_SetOnTop (p_sys->embed, on_top);
    }

    /* TODO */
#if 0
    /* Hide the mouse. It will be send when
     * vout_display_t::info.b_hide_mouse is false */
    VOUT_DISPLAY_HIDE_MOUSE,
#endif
    case VOUT_DISPLAY_RESET_PICTURES:
        assert(0);
    default:
        msg_Err (vd, "Unknown request in XCB vout display");
        return VLC_EGENERIC;
    }
}

static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *p_sys = vd->sys;

    ManageEvent (vd, p_sys->conn, p_sys->window);
}

