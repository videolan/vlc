/**
 * @file xvideo.c
 * @brief X C Bindings video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <xcb/xv.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_dialog.h>

#include "pictures.h"
#include "events.h"

#define ADAPTOR_TEXT N_("XVideo adaptor number")
#define ADAPTOR_LONGTEXT N_( \
    "XVideo hardware adaptor to use. By default, VLC will " \
    "use the first functional adaptor.")

#define FORMAT_TEXT N_("XVideo format id")
#define FORMAT_LONGTEXT N_( \
    "XVideo image format id to use. By default, VLC will " \
    "try to use the best match for the video being played.")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);
static int EnumAdaptors (vlc_object_t *, const char *, int64_t **, char ***);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("XVideo"))
    set_description (N_("XVideo output (XCB)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", 200)
    set_callbacks (Open, Close)

    add_integer ("xvideo-adaptor", -1,
                 ADAPTOR_TEXT, ADAPTOR_LONGTEXT, true)
        change_integer_cb (EnumAdaptors)
    add_integer ("xvideo-format-id", 0,
                 FORMAT_TEXT, FORMAT_LONGTEXT, true)
    add_obsolete_bool ("xvideo-shm") /* removed in 2.0.0 */
    add_shortcut ("xcb-xv", "xv", "xvideo", "xid")
vlc_module_end ()

#define MAX_PICTURES (128)

struct vout_display_sys_t
{
    xcb_connection_t *conn;
    vout_window_t *embed;/* VLC window */

    xcb_cursor_t cursor; /* blank cursor */
    xcb_window_t window; /* drawable X window */
    xcb_gcontext_t gc;   /* context to put images */
    xcb_xv_port_t port;  /* XVideo port */
    uint32_t id;         /* XVideo format */
    uint16_t width;      /* display width */
    uint16_t height;     /* display height */
    uint32_t data_size;  /* picture byte size (for non-SHM) */
    bool     swap_uv;    /* U/V pointer must be swapped in a picture */
    bool shm;            /* whether to use MIT-SHM */
    bool visible;        /* whether it makes sense to draw at all */

    xcb_xv_query_image_attributes_reply_t *att;
    picture_pool_t *pool; /* picture pool */
    picture_resource_t resource[MAX_PICTURES];
};

static picture_pool_t *Pool (vout_display_t *, unsigned);
static void Display (vout_display_t *, picture_t *, subpicture_t *subpicture);
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

    /* We need XVideo 2.2 for PutImage */
    r = xcb_xv_query_extension_reply (conn, ck, NULL);
    if (r == NULL)
        msg_Dbg (vd, "XVideo extension not available");
    else
    if (r->major != 2)
        msg_Dbg (vd, "XVideo extension v%"PRIu8".%"PRIu8" unknown",
                 r->major, r->minor);
    else
    if (r->minor < 2)
        msg_Dbg (vd, "XVideo extension v%"PRIu8".%"PRIu8" too old",
                 r->major, r->minor);
    else
    {
        msg_Dbg (vd, "using XVideo extension v%"PRIu8".%"PRIu8,
                 r->major, r->minor);
        ok = true;
    }
    free (r);
    return ok;
}

static vlc_fourcc_t ParseFormat (vlc_object_t *obj,
                                 const xcb_xv_image_format_info_t *restrict f)
{
    switch (f->type)
    {
      case XCB_XV_IMAGE_FORMAT_INFO_TYPE_RGB:
        switch (f->num_planes)
        {
          case 1:
            switch (popcount (f->red_mask | f->green_mask | f->blue_mask))
            {
              case 24:
                if (f->bpp == 32 && f->depth == 32)
                    return VLC_CODEC_RGBA;
                if (f->bpp == 32 && f->depth == 24)
                    return VLC_CODEC_RGB32;
                if (f->bpp == 24 && f->depth == 24)
                    return VLC_CODEC_RGB24;
                break;
              case 16:
                if (f->byte_order != ORDER)
                    return 0; /* Mixed endian! */
                if (f->bpp == 16 && f->depth == 16)
                    return VLC_CODEC_RGB16;
                break;
              case 15:
                if (f->byte_order != ORDER)
                    return 0; /* Mixed endian! */
                if (f->bpp == 16 && f->depth == 16)
                    return VLC_CODEC_RGBT;
                if (f->bpp == 16 && f->depth == 15)
                    return VLC_CODEC_RGB15;
                break;
              case 12:
                if (f->bpp == 16 && f->depth == 16)
                    return VLC_CODEC_RGBA16;
                if (f->bpp == 16 && f->depth == 12)
                    return VLC_CODEC_RGB12;
              case 8:
                if (f->bpp == 8 && f->depth == 8)
                    return VLC_CODEC_RGB8;
                break;
            }
            break;
        }
        msg_Err (obj, "unknown XVideo RGB format %"PRIx32" (%.4s)",
                 f->id, f->guid);
        msg_Dbg (obj, " %"PRIu8" planes, %"PRIu8" bits/pixel, "
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
        msg_Err (obj, "unknown XVideo YUV format %"PRIx32" (%.4s)", f->id,
                 f->guid);
        msg_Dbg (obj, " %"PRIu8" planes, %"PRIu32" bits/pixel, "
                 "%"PRIu32"/%"PRIu32"/%"PRIu32" bits/sample", f->num_planes,
                 f->bpp, f->y_sample_bits, f->u_sample_bits, f->v_sample_bits);
        msg_Dbg (obj, " period: %"PRIu32"/%"PRIu32"/%"PRIu32"x"
                 "%"PRIu32"/%"PRIu32"/%"PRIu32,
                 f->vhorz_y_period, f->vhorz_u_period, f->vhorz_v_period,
                 f->vvert_y_period, f->vvert_u_period, f->vvert_v_period);
        msg_Warn (obj, " order: %.32s", f->vcomp_order);
        break;
    }
    return 0;
}

static bool BetterFormat (vlc_fourcc_t a, const vlc_fourcc_t *tab,
                          unsigned *rankp)
{
    for (unsigned i = 0, max = *rankp; i < max && tab[i] != 0; i++)
        if (tab[i] == a)
        {
            *rankp = i;
            return true;
        }
    return false;
}

static xcb_xv_query_image_attributes_reply_t *
FindFormat (vlc_object_t *obj, xcb_connection_t *conn, video_format_t *fmt,
            const xcb_xv_adaptor_info_t *a, uint32_t *idp)
{
    /* Order chromas by preference */
    vlc_fourcc_t tab[7];
    const vlc_fourcc_t *chromav = tab;

    vlc_fourcc_t chroma = var_InheritInteger (obj, "xvideo-format-id");
    if (chroma != 0) /* Forced chroma */
    {
        tab[0] = chroma;
        tab[1] = 0;
    }
    else if (vlc_fourcc_IsYUV (fmt->i_chroma)) /* YUV chroma */
    {
        chromav = vlc_fourcc_GetYUVFallback (fmt->i_chroma);
    }
    else /* RGB chroma */
    {
        tab[0] = fmt->i_chroma;
        tab[1] = VLC_CODEC_RGB32;
        tab[2] = VLC_CODEC_RGB24;
        tab[3] = VLC_CODEC_RGB16;
        tab[4] = VLC_CODEC_RGB15;
        tab[5] = VLC_CODEC_YUYV;
        tab[6] = 0;
    }

    /* Get available image formats */
    xcb_xv_list_image_formats_reply_t *list =
        xcb_xv_list_image_formats_reply (conn,
            xcb_xv_list_image_formats (conn, a->base_id), NULL);
    if (list == NULL)
        return NULL;

    /* Check available XVideo chromas */
    xcb_xv_query_image_attributes_reply_t *attr = NULL;
    unsigned rank = UINT_MAX;

    for (const xcb_xv_image_format_info_t *f =
             xcb_xv_list_image_formats_format (list),
                                          *f_end =
             f + xcb_xv_list_image_formats_format_length (list);
         f < f_end;
         f++)
    {
        chroma = ParseFormat (obj, f);
        if (chroma == 0)
            continue;

        /* Oink oink! */
        if ((chroma == VLC_CODEC_I420 || chroma == VLC_CODEC_YV12)
         && a->name_size >= 4
         && !memcmp ("OMAP", xcb_xv_adaptor_info_name (a), 4))
        {
            msg_Dbg (obj, "skipping slow I420 format");
            continue; /* OMAP framebuffer sucks at YUV 4:2:0 */
        }

        if (!BetterFormat (chroma, chromav, &rank))
            continue;

        /* VLC pads scanline to 16 pixels internally */
        unsigned width = fmt->i_width;
        unsigned height = fmt->i_height;
        xcb_xv_query_image_attributes_reply_t *i;
        i = xcb_xv_query_image_attributes_reply (conn,
            xcb_xv_query_image_attributes (conn, a->base_id, f->id,
                                           width, height), NULL);
        if (i == NULL)
            continue;

        if (i->width != width || i->height != height)
        {
            msg_Warn (obj, "incompatible size %ux%u -> %"PRIu32"x%"PRIu32,
                      fmt->i_width, fmt->i_height,
                      i->width, i->height);
            var_Create (obj->p_libvlc, "xvideo-res-error", VLC_VAR_BOOL);
            if (!var_GetBool (obj->p_libvlc, "xvideo-res-error"))
            {
                dialog_FatalWait (obj, _("Video acceleration not available"),
                    _("The XVideo rendering acceleration driver does not "
                      "support the required resolution of %ux%u pixels but "
                      "%"PRIu32"x%"PRIu32" pixels instead.\n"
                      "Acceleration will thus be disabled. Performance may "
                      "be degraded severely if the resolution is large."),
                                  width, height, i->width, i->height);
                var_SetBool (obj->p_libvlc, "xvideo-res-error", true);
            }
            free (i);
            continue;
        }

        fmt->i_chroma = chroma;
        if (f->type == XCB_XV_IMAGE_FORMAT_INFO_TYPE_RGB)
        {
            fmt->i_rmask = f->red_mask;
            fmt->i_gmask = f->green_mask;
            fmt->i_bmask = f->blue_mask;
        }
        *idp = f->id;
        free (attr);
        attr = i;
        if (rank == 0)
            break; /* shortcut for perfect match */
    }

    free (list);
    return attr;
}


/**
 * Probe the X server.
 */
static int Open (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *p_sys;

    if (!var_InheritBool (obj, "overlay"))
        return VLC_EGENERIC;
    p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    vd->sys = p_sys;

    /* Connect to X */
    xcb_connection_t *conn;
    const xcb_screen_t *screen;
    uint16_t width, height;
    p_sys->embed = XCB_parent_Create (vd, &conn, &screen, &width, &height);
    if (p_sys->embed == NULL)
    {
        free (p_sys);
        return VLC_EGENERIC;
    }

    p_sys->conn = conn;
    p_sys->att = NULL;
    p_sys->pool = NULL;

    if (!CheckXVideo (vd, conn))
    {
        msg_Warn (vd, "Please enable XVideo 2.2 for faster video display");
        goto error;
    }

    p_sys->window = xcb_generate_id (conn);
    xcb_pixmap_t pixmap = xcb_generate_id (conn);

    /* Cache adaptors infos */
    xcb_xv_query_adaptors_reply_t *adaptors =
        xcb_xv_query_adaptors_reply (conn,
            xcb_xv_query_adaptors (conn, p_sys->embed->handle.xid), NULL);
    if (adaptors == NULL)
        goto error;

    int adaptor_selected = var_InheritInteger (obj, "xvideo-adaptor");
    int adaptor_current = -1;

    /* */
    video_format_t fmt;
    vout_display_place_t place;

    p_sys->port = 0;
    vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);
    p_sys->width  = place.width;
    p_sys->height = place.height;

    xcb_xv_adaptor_info_iterator_t it;
    for (it = xcb_xv_query_adaptors_info_iterator (adaptors);
         it.rem > 0;
         xcb_xv_adaptor_info_next (&it))
    {
        const xcb_xv_adaptor_info_t *a = it.data;

        adaptor_current++;
        if (adaptor_selected != -1 && adaptor_selected != adaptor_current)
            continue;
        if (!(a->type & XCB_XV_TYPE_INPUT_MASK)
         || !(a->type & XCB_XV_TYPE_IMAGE_MASK))
            continue;

        /* Look for an image format */
        fmt = vd->fmt;
        free (p_sys->att);
        p_sys->att = FindFormat (obj, conn, &fmt, a, &p_sys->id);
        if (p_sys->att == NULL) /* No acceptable image formats */
            continue;

        /* Grab a port */
        for (unsigned i = 0; i < a->num_ports; i++)
        {
             xcb_xv_port_t port = a->base_id + i;
             xcb_xv_grab_port_reply_t *gr =
                 xcb_xv_grab_port_reply (conn,
                     xcb_xv_grab_port (conn, port, XCB_CURRENT_TIME), NULL);
             uint8_t result = gr ? gr->result : 0xff;

             free (gr);
             if (result == 0)
             {
                 p_sys->port = port;
                 goto grabbed_port;
             }
             msg_Dbg (vd, "cannot grab port %"PRIu32": Xv error %"PRIu8, port,
                      result);
        }
        continue; /* No usable port */

    grabbed_port:
        /* Found port - initialize selected format */
        msg_Dbg (vd, "using adaptor %.*s", (int)a->name_size,
                 xcb_xv_adaptor_info_name (a));
        msg_Dbg (vd, "using port %"PRIu32, p_sys->port);
        msg_Dbg (vd, "using image format 0x%"PRIx32, p_sys->id);

        /* Look for an X11 visual, create a window */
        xcb_xv_format_t *f = xcb_xv_adaptor_info_formats (a);
        for (uint_fast16_t i = a->num_formats; i > 0; i--, f++)
        {
            if (f->depth != screen->root_depth)
                continue; /* this would fail anyway */

            uint32_t mask =
                XCB_CW_BACK_PIXMAP |
                XCB_CW_BACK_PIXEL |
                XCB_CW_BORDER_PIXMAP |
                XCB_CW_BORDER_PIXEL |
                XCB_CW_EVENT_MASK |
                XCB_CW_COLORMAP;
            const uint32_t list[] = {
                /* XCB_CW_BACK_PIXMAP */
                pixmap,
                /* XCB_CW_BACK_PIXEL */
                screen->black_pixel,
                /* XCB_CW_BORDER_PIXMAP */
                pixmap,
                /* XCB_CW_BORDER_PIXEL */
                screen->black_pixel,
                /* XCB_CW_EVENT_MASK */
                XCB_EVENT_MASK_VISIBILITY_CHANGE,
                /* XCB_CW_COLORMAP */
                screen->default_colormap,
            };
            xcb_void_cookie_t c;

            xcb_create_pixmap (conn, f->depth, pixmap, screen->root, 1, 1);
            c = xcb_create_window_checked (conn, f->depth, p_sys->window,
                 p_sys->embed->handle.xid, place.x, place.y,
                 place.width, place.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                 f->visual, mask, list);
            xcb_map_window (conn, p_sys->window);

            if (!XCB_error_Check (vd, conn, "cannot create X11 window", c))
            {
                msg_Dbg (vd, "using X11 visual ID 0x%"PRIx32
                         " (depth: %"PRIu8")", f->visual, f->depth);
                msg_Dbg (vd, "using X11 window 0x%08"PRIx32, p_sys->window);
                goto created_window;
            }
        }
        xcb_xv_ungrab_port (conn, p_sys->port, XCB_CURRENT_TIME);
        p_sys->port = 0;
        msg_Dbg (vd, "no usable X11 visual");
        continue; /* No workable XVideo format (visual/depth) */

    created_window:
        break;
    }
    free (adaptors);
    if (!p_sys->port)
    {
        msg_Err (vd, "no available XVideo adaptor");
        goto error;
    }

    /* Create graphic context */
    p_sys->gc = xcb_generate_id (conn);
    xcb_create_gc (conn, p_sys->gc, p_sys->window, 0, NULL);
    msg_Dbg (vd, "using X11 graphic context 0x%08"PRIx32, p_sys->gc);

    /* Disable color keying if applicable */
    {
        xcb_intern_atom_reply_t *r =
            xcb_intern_atom_reply (conn,
                xcb_intern_atom (conn, 1, 21, "XV_AUTOPAINT_COLORKEY"), NULL);
        if (r != NULL && r->atom != 0)
            xcb_xv_set_port_attribute(conn, p_sys->port, r->atom, 1);
        free(r);
    }

    /* Create cursor */
    p_sys->cursor = XCB_cursor_Create (conn, screen);

    p_sys->shm = XCB_shm_Check (obj, conn);
    p_sys->visible = false;

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = false;
    info.has_event_thread = true;

    /* Setup vout_display_t once everything is fine */
    p_sys->swap_uv = vlc_fourcc_AreUVPlanesSwapped (fmt.i_chroma,
                                                    vd->fmt.i_chroma);
    if (p_sys->swap_uv)
        fmt.i_chroma = vd->fmt.i_chroma;
    vd->fmt = fmt;
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

    if (p_sys->pool)
    {
        for (unsigned i = 0; i < MAX_PICTURES; i++)
        {
            picture_resource_t *res = &p_sys->resource[i];

            if (!res->p->p_pixels)
                break;
            XCB_pictures_Free (res, NULL);
        }
        picture_pool_Delete (p_sys->pool);
    }

    /* show the default cursor */
    xcb_change_window_attributes (p_sys->conn, p_sys->embed->handle.xid, XCB_CW_CURSOR,
                                  &(uint32_t) { XCB_CURSOR_NONE });
    xcb_flush (p_sys->conn);

    free (p_sys->att);
    xcb_disconnect (p_sys->conn);
    vout_display_DeleteWindow (vd, p_sys->embed);
    free (p_sys);
}

static void PoolAlloc (vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *p_sys = vd->sys;

    memset (p_sys->resource, 0, sizeof(p_sys->resource));

    const uint32_t *pitches= xcb_xv_query_image_attributes_pitches (p_sys->att);
    const uint32_t *offsets= xcb_xv_query_image_attributes_offsets (p_sys->att);
    const unsigned num_planes= __MIN(p_sys->att->num_planes, PICTURE_PLANE_MAX);
    p_sys->data_size = p_sys->att->data_size;

    picture_t *pic_array[MAX_PICTURES];
    requested_count = __MIN(requested_count, MAX_PICTURES);

    unsigned count;
    for (count = 0; count < requested_count; count++)
    {
        picture_resource_t *res = &p_sys->resource[count];

        for (unsigned i = 0; i < num_planes; i++)
        {
            uint32_t data_size;
            data_size = (i < num_planes - 1) ? offsets[i+1] : p_sys->data_size;

            res->p[i].i_lines = (data_size - offsets[i]) / pitches[i];
            res->p[i].i_pitch = pitches[i];
        }

        if (XCB_pictures_Alloc (vd, res, p_sys->att->data_size,
                                p_sys->conn, p_sys->shm))
            break;

        /* Allocate further planes as specified by XVideo */
        /* We assume that offsets[0] is zero */
        for (unsigned i = 1; i < num_planes; i++)
            res->p[i].p_pixels = res->p[0].p_pixels + offsets[i];

        if (p_sys->swap_uv)
        {   /* YVU: swap U and V planes */
            uint8_t *buf = res->p[2].p_pixels;
            res->p[2].p_pixels = res->p[1].p_pixels;
            res->p[1].p_pixels = buf;
        }

        pic_array[count] = picture_NewFromResource (&vd->fmt, res);
        if (!pic_array[count])
        {
            XCB_pictures_Free (res, p_sys->conn);
            memset (res, 0, sizeof(*res));
            break;
        }
    }

    if (count == 0)
        return;

    p_sys->pool = picture_pool_New (count, pic_array);
    /* TODO release picture resources if NULL */
    xcb_flush (p_sys->conn);
}

/**
 * Return a direct buffer
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *p_sys = vd->sys;

    if (!p_sys->pool)
        PoolAlloc (vd, requested_count);

    return p_sys->pool;
}

/**
 * Sends an image to the X server.
 */
static void Display (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *p_sys = vd->sys;
    xcb_shm_seg_t segment = pic->p_sys->segment;
    xcb_void_cookie_t ck;

    if (!p_sys->visible)
        goto out;
    if (segment)
        ck = xcb_xv_shm_put_image_checked (p_sys->conn, p_sys->port,
                              p_sys->window, p_sys->gc, segment, p_sys->id, 0,
                   /* Src: */ vd->source.i_x_offset,
                              vd->source.i_y_offset,
                              vd->source.i_visible_width,
                              vd->source.i_visible_height,
                   /* Dst: */ 0, 0, p_sys->width, p_sys->height,
                /* Memory: */ pic->p->i_pitch / pic->p->i_pixel_pitch,
                              pic->p->i_lines, false);
    else
        ck = xcb_xv_put_image_checked (p_sys->conn, p_sys->port, p_sys->window,
                          p_sys->gc, p_sys->id,
                          vd->source.i_x_offset,
                          vd->source.i_y_offset,
                          vd->source.i_visible_width,
                          vd->source.i_visible_height,
                          0, 0, p_sys->width, p_sys->height,
                          pic->p->i_pitch / pic->p->i_pixel_pitch,
                          pic->p->i_lines,
                          p_sys->data_size, pic->p->p_pixels);

    /* Wait for reply. See x11.c for rationale. */
    xcb_generic_error_t *e = xcb_request_check (p_sys->conn, ck);
    if (e != NULL)
    {
        msg_Dbg (vd, "%s: X11 error %d", "cannot put image", e->error_code);
        free (e);
    }
out:
    picture_Release (pic);
    (void)subpicture;
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

        if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE && va_arg (ap, int))
        {
            vout_window_SetSize (p_sys->embed,
                                 cfg->display.width, cfg->display.height);
            return VLC_EGENERIC; /* Always fail. See x11.c for rationale. */
        }

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
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    {
        unsigned state = va_arg (ap, unsigned);
        return vout_window_SetState (p_sys->embed, state);
    }

    /* Hide the mouse. It will be send when
     * vout_display_t::info.b_hide_mouse is false */
    case VOUT_DISPLAY_HIDE_MOUSE:
        xcb_change_window_attributes (p_sys->conn, p_sys->embed->handle.xid,
                                  XCB_CW_CURSOR, &(uint32_t){ p_sys->cursor });
        xcb_flush (p_sys->conn);
        return VLC_SUCCESS;
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

    XCB_Manage (vd, p_sys->conn, &p_sys->visible);
}

static int EnumAdaptors (vlc_object_t *obj, const char *var,
                         int64_t **vp, char ***tp)
{
    size_t n = 0;

    /* Connect to X */
    char *display = var_InheritString (obj, "x11-display");
    xcb_connection_t *conn;
    int snum;

    conn = xcb_connect (display, &snum);
    free (display);
    if (xcb_connection_has_error (conn) /*== NULL*/)
        goto error;

    /* Find configured screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    const xcb_screen_t *scr = NULL;
    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator (setup);
         i.rem > 0; xcb_screen_next (&i))
    {
        if (snum == 0)
        {
            scr = i.data;
            break;
        }
        snum--;
    }
    if (scr == NULL)
        goto error;

    xcb_xv_query_adaptors_reply_t *adaptors =
        xcb_xv_query_adaptors_reply (conn,
            xcb_xv_query_adaptors (conn, scr->root), NULL);
    if (adaptors == NULL)
        goto error;

    xcb_xv_adaptor_info_iterator_t it;

    for (it = xcb_xv_query_adaptors_info_iterator (adaptors);
         it.rem > 0;
         xcb_xv_adaptor_info_next (&it))
        n++;

    int64_t *values = xmalloc ((n + 1) * sizeof (*values));
    char **texts = xmalloc ((n + 1) * sizeof (*texts));
    *vp = values;
    *tp = texts;
    n = 0;

    *(values++) = -1;
    *(texts++) = strdup (N_("Auto"));
    n++;

    for (it = xcb_xv_query_adaptors_info_iterator (adaptors);
         it.rem > 0;
         xcb_xv_adaptor_info_next (&it))
    {
        const xcb_xv_adaptor_info_t *a = it.data;

        n++;

        if (!(a->type & XCB_XV_TYPE_INPUT_MASK)
         || !(a->type & XCB_XV_TYPE_IMAGE_MASK))
            continue;

        *(values++) = n - 2;
        *(texts++) = strndup (xcb_xv_adaptor_info_name (a), a->name_size);
    }
    free (adaptors);
error:
    xcb_disconnect (conn);
    (void) obj; (void) var;
    return n;
}
