/**
 * @file xcb.c
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

#include <sys/types.h>
#include <sys/shm.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>

#include <xcb/xcb_aux.h>
#include <xcb/xcb_image.h>

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
    xcb_screen_t *screen;
    vout_window_t *embed; /* VLC window (when windowed) */

    xcb_visualid_t vid;
    xcb_window_t parent; /* parent X window */
    xcb_window_t window; /* drawable X window */
    xcb_gcontext_t gc; /* context to put images */
    bool shm; /* whether to use MIT-SHM */
    bool gray; /* whether display is grayscale */
    uint8_t bpp; /* bits per pixel */
};

static int Init (vout_thread_t *);
static void Deinit (vout_thread_t *);
static void Display (vout_thread_t *, picture_t *);
static int Manage (vout_thread_t *);

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
    p_sys->conn = NULL;
#ifndef NDEBUG
    p_sys->embed = NULL;
#endif

    /* Connect to X */
    char *display = var_CreateGetNonEmptyString (vout, "x11-display");
    int snum;
    p_sys->conn = xcb_connect (display, &snum);
    if (xcb_connection_has_error (p_sys->conn) /*== NULL*/)
    {
        msg_Err (vout, "cannot connect to X server %s",
                 display ? display : "");
        goto error;
    }

    /* Get the preferred screen */
    xcb_screen_t *scr = xcb_aux_get_screen (p_sys->conn, snum);
    p_sys->screen = scr;
    assert (p_sys->screen);
    msg_Dbg (vout, "using screen %d (depth %"PRIu8")", snum, scr->root_depth);

    if (strchr ("\x08\x0f\x10\x18\x20", scr->root_depth) == NULL)
    {
        msg_Err (vout, "unsupported %"PRIu8"-bits color depth",
                 scr->root_depth);
        goto error;
    }

    /* Determine the visual (color depth and palette) */
    xcb_visualtype_t *vt = NULL;
    p_sys->gray = false;
    if ((vt = xcb_aux_find_visual_by_attrs (scr, XCB_VISUAL_CLASS_TRUE_COLOR,
                                            scr->root_depth)) != NULL)
        msg_Dbg (vout, "using TrueColor visual ID %d", (int)vt->visual_id);
    else
    if ((vt = xcb_aux_find_visual_by_attrs (scr, XCB_VISUAL_CLASS_STATIC_COLOR,
                                            scr->root_depth)) != NULL)
        msg_Dbg (vout, "using static color visual ID %d", (int)vt->visual_id);
    else
    if ((scr->root_depth == 8)
     && (vt = xcb_aux_find_visual_by_attrs (scr, XCB_VISUAL_CLASS_STATIC_GRAY,
                                            scr->root_depth)) != NULL)
    {
        msg_Dbg (vout, "using static gray visual ID %d", (int)vt->visual_id);
        p_sys->gray = true;
    }
    else
    {
        vt = xcb_aux_find_visual_by_id (scr, scr->root_visual);
        assert (vt);
        msg_Err (vout, "unsupported visual class %"PRIu8, vt->_class);
        goto error;
    }
    p_sys->vid = vt->visual_id;

    p_sys->shm = var_CreateGetBool (vout, "x11-shm") > 0;
    if (p_sys->shm)
    {
        xcb_shm_query_version_cookie_t ck;
        xcb_shm_query_version_reply_t *r;
        xcb_generic_error_t *err;

        ck = xcb_shm_query_version (p_sys->conn);
        r = xcb_shm_query_version_reply (p_sys->conn, ck, &err);
        if (!r)
        {
            msg_Err (vout, "shared memory (MIT-SHM) not available");
            msg_Warn (vout, "display will be slow");
            p_sys->shm = false;
        }
    }

    vout->pf_init = Init;
    vout->pf_end = Deinit;
    vout->pf_display = Display;
    vout->pf_manage = Manage;
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

    assert (p_sys->embed == NULL);
    if (p_sys->conn)
        xcb_disconnect (p_sys->conn);
    free (p_sys);
}

struct picture_sys_t
{
    xcb_connection_t *conn;
    xcb_image_t *image;
    xcb_shm_seg_t segment;
};

static void PictureRelease (picture_t *pic);
static void PictureShmRelease (picture_t *pic);
#define SHM_ERR ((void *)(intptr_t)(-1))

static int PictureInit (vout_thread_t *vout, picture_t *pic)
{
    vout_sys_t *p_sys = vout->p_sys;
    picture_sys_t *priv = malloc (sizeof (*p_sys));

    if (priv == NULL)
        return VLC_ENOMEM;

    assert (pic->i_status == FREE_PICTURE);
    vout_InitPicture (vout, pic, vout->output.i_chroma,
                      vout->output.i_width, vout->output.i_height,
                      vout->output.i_aspect);

    void *shm = SHM_ERR;
    const size_t size = pic->p->i_pitch * pic->p->i_lines;

    if (p_sys->shm)
    {   /* Allocate shared memory segment */
        int id = shmget (IPC_PRIVATE, size, IPC_CREAT | 0700);

        if (id == -1)
        {
            msg_Err (vout, "shared memory allocation error: %m");
            goto error;
        }

        /* Attach the segment to VLC */
        shm = shmat (id, NULL, 0 /* read/write */);
        if (shm == SHM_ERR)
        {
            msg_Err (vout, "shared memory attachment error: %m");
            shmctl (id, IPC_RMID, 0);
            goto error;
        }

        /* Attach the segment to X */
        xcb_void_cookie_t ck;

        priv->segment = xcb_generate_id (p_sys->conn);
        ck = xcb_shm_attach_checked (p_sys->conn, priv->segment, id, 1);
        shmctl (id, IPC_RMID, 0);

        if (CheckError (vout, "shared memory server-side error", ck))
        {
            msg_Info (vout, "using buggy X (remote) server? SSH?");
            shmdt (shm);
            shm = SHM_ERR;
        }
    }

    const unsigned real_width = pic->p->i_pitch / (p_sys->bpp >> 3);
    /* FIXME: anyway to getthing more intuitive than that?? */

    /* NOTE: 32-bits scanline_pad assumed, FIXME? (see xdpyinfo) */
    xcb_image_t *img;
    img = xcb_image_create (real_width, pic->p->i_lines,
                            XCB_IMAGE_FORMAT_Z_PIXMAP, 32,
                            p_sys->screen->root_depth, p_sys->bpp, p_sys->bpp,
#ifdef WORDS_BIGENDIAN
                            XCB_IMAGE_ORDER_MSB_FIRST,
#else
                            XCB_IMAGE_ORDER_LSB_FIRST,
#endif
                            XCB_IMAGE_ORDER_MSB_FIRST,
                            NULL,
                            (shm != SHM_ERR) ? size : 0,
                            (shm != SHM_ERR) ? shm : NULL);

    if (img == NULL)
    {
        if (shm != SHM_ERR)
            xcb_shm_detach (p_sys->conn, priv->segment);
        goto error;
    }
    if (shm != SHM_ERR && xcb_image_native (p_sys->conn, img, 0) == NULL)
    {
        msg_Err (vout, "incompatible X server image format");
        xcb_image_destroy (img);
        goto error;
    }

    priv->conn = p_sys->conn;
    priv->image = img;
    pic->p_sys = priv;
    pic->p->p_pixels = img->data;
    pic->pf_release = (shm != SHM_ERR) ? PictureShmRelease
                                       : PictureRelease;
    pic->i_status = DESTROYED_PICTURE;
    pic->i_type = DIRECT_PICTURE;
    return VLC_SUCCESS;

error:
    if (shm != SHM_ERR)
        shmdt (shm);
    free (p_sys);
    return VLC_EGENERIC;
}


/**
 * Release picture private data
 */
static void PictureRelease (picture_t *pic)
{
    struct picture_sys_t *p_sys = pic->p_sys;

    xcb_image_destroy (p_sys->image);
    free (p_sys);
}

/**
 * Release shared memory picture private data
 */
static void PictureShmRelease (picture_t *pic)
{
    struct picture_sys_t *p_sys = pic->p_sys;

    xcb_shm_detach (p_sys->conn, p_sys->segment);
    shmdt (p_sys->image->data);
    PictureRelease (pic);
}

/**
 * Allocate drawable window and picture buffers.
 */
static int Init (vout_thread_t *vout)
{
    vout_sys_t *p_sys = vout->p_sys;
    const xcb_screen_t *screen = p_sys->screen;
    unsigned x, y, width, height;

    /* Determine parent window */
    if (vout->b_fullscreen)
    {
        p_sys->embed = NULL;
        p_sys->parent = screen->root;
        width = screen->width_in_pixels;
        height = screen->height_in_pixels;
    }
    else
    {
        p_sys->embed = vout_RequestWindow (vout, &(int){ 0 }, &(int){ 0 },
                                            &width, &height);
        if (p_sys->embed == NULL)
        {
            msg_Err (vout, "cannot get window");
            return VLC_EGENERIC;
        }
        p_sys->parent = (intptr_t)p_sys->embed->handle;
    }

    /* Determine our input format */
    p_sys->bpp = screen->root_depth;
    switch (screen->root_depth)
    {
        case 24:
            p_sys->bpp = 32;
        case 32: /* FIXME: untested */
            vout->output.i_chroma = VLC_FOURCC ('R', 'V', '3', '2');
            break;

        case 16:
            vout->output.i_chroma = VLC_FOURCC ('R', 'V', '1', '6');
            break;

        case 15:
            p_sys->bpp = 16;
            vout->output.i_chroma = VLC_FOURCC ('R', 'V', '1', '5');
            break;

        case 8: /* FIXME: VLC cannot convert */
            vout->output.i_chroma =
                p_sys->gray ? VLC_FOURCC ('G', 'R', 'E', 'Y')
                            : VLC_FOURCC ('R', 'G', 'B', '2');
            break;

        default:
            assert (0);
    }
    vout_PlacePicture (vout, width, height, &x, &y, &width, &height);

    /* FIXME: I don't get the subtlety between output and fmt_out here */
    vout->fmt_out.i_chroma = vout->output.i_chroma;
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

    /* Create window */
    const uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {
        /* XCB_CW_BACK_PIXEL */
        screen->black_pixel,
        /* XCB_CW_EVENT_MASK */
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION,
    };
    xcb_void_cookie_t c;
    xcb_window_t window = xcb_generate_id (p_sys->conn);

    c = xcb_create_window_checked (p_sys->conn, screen->root_depth, window,
                                   p_sys->parent, x, y, width, height, 0,
                                   XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                   screen->root_visual, mask, values);
    if (CheckError (vout, "cannot create X11 window", c))
        goto error;
    p_sys->window = window;
    msg_Dbg (vout, "using X11 window %08"PRIx32, p_sys->window);
    xcb_map_window (p_sys->conn, window);

    /* Create graphic context (I wonder why the heck do we need this) */
    p_sys->gc = xcb_generate_id (p_sys->conn);
    xcb_create_gc (p_sys->conn, p_sys->gc, p_sys->window, 0, NULL);
    msg_Dbg (vout, "using X11 graphic context %08"PRIx32, p_sys->gc);
    xcb_flush (p_sys->conn);

    /* Allocate picture buffers */
    do
    {
        picture_t *pic = vout->p_picture + I_OUTPUTPICTURES;

        if (PictureInit (vout, pic))
            break;
        PP_OUTPUTPICTURE[I_OUTPUTPICTURES++] = pic;
    }
    while (I_OUTPUTPICTURES < 2);

    return VLC_SUCCESS;

error:
    Deinit (vout);
    return VLC_EGENERIC;
}

/**
 * Free picture buffers.
 */
static void Deinit (vout_thread_t *vout)
{
    vout_sys_t *p_sys = vout->p_sys;

    while (I_OUTPUTPICTURES > 0)
        picture_Release (PP_OUTPUTPICTURE[--I_OUTPUTPICTURES]);

    xcb_unmap_window (p_sys->conn, p_sys->window);
    xcb_destroy_window (p_sys->conn, p_sys->window);
    vout_ReleaseWindow (p_sys->embed);
    p_sys->embed = NULL;
}

/**
 * Sends an image to the X server.
 */
static void Display (vout_thread_t *vout, picture_t *pic)
{
    vout_sys_t *p_sys = vout->p_sys;
    picture_sys_t *priv = pic->p_sys;
    xcb_image_t *img = priv->image;

    if (img->base == NULL)
    {
        xcb_shm_segment_info_t info = {
            .shmseg = priv->segment,
            .shmid = -1, /* lost track of it, unimportant */
            .shmaddr = img->data,
        };

        xcb_image_shm_put (p_sys->conn, p_sys->window, p_sys->gc, img, info,
                        0, 0, 0, 0, img->width, img->height, 0);
    }
    else
    {
        xcb_image_t *native = xcb_image_native (p_sys->conn, img, 1);

        if (native == NULL)
            return;

        xcb_image_put (p_sys->conn, p_sys->window, p_sys->gc, native, 0, 0, 0);
        if (native != img)
            xcb_image_destroy (native);
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
        ProcessEvent (vout, ev);

    if (xcb_connection_has_error (p_sys->conn))
    {
        msg_Err (vout, "X server failure");
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
