/**
 * @file xcb.c
 * @brief X11 C Bindings screen capture demux module for VLC media player
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
#include <stdarg.h>
#include <assert.h>
#include <xcb/xcb.h>
#include <xcb/composite.h>
#ifdef HAVE_SYS_SHM_H
# include <sys/shm.h>
# include <xcb/shm.h>
#endif
#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>

#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_( \
    "How many times the screen content should be refreshed per second.")

#define LEFT_TEXT N_("Region left column")
#define LEFT_LONGTEXT N_( \
    "Abscissa of the capture region in pixels.")

#define TOP_TEXT N_("Region top row")
#define TOP_LONGTEXT N_( \
    "Ordinate of the capture region in pixels.")

#define WIDTH_TEXT N_("Capture region width")
#define WIDTH_LONGTEXT N_( \
    "Pixel width of the capture region, or 0 for full width")

#define HEIGHT_TEXT N_("Capture region height")
#define HEIGHT_LONGTEXT N_( \
    "Pixel height of the capture region, or 0 for full height")

#define FOLLOW_MOUSE_TEXT N_( "Follow the mouse" )
#define FOLLOW_MOUSE_LONGTEXT N_( \
    "Follow the mouse when capturing a subscreen." )

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Screen"))
    set_description (N_("Screen capture (with X11/XCB)"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_capability ("access_demux", 0)
    set_callbacks (Open, Close)

    add_float ("screen-fps", 2.0, FPS_TEXT, FPS_LONGTEXT, true)
    add_integer ("screen-left", 0, LEFT_TEXT, LEFT_LONGTEXT, true)
        change_integer_range (-32768, 32767)
        change_safe ()
    add_integer ("screen-top", 0, TOP_TEXT, TOP_LONGTEXT, true)
        change_integer_range (-32768, 32767)
        change_safe ()
    add_integer ("screen-width", 0, WIDTH_TEXT, WIDTH_LONGTEXT, true)
        change_integer_range (0, 65535)
        change_safe ()
    add_integer ("screen-height", 0, HEIGHT_TEXT, HEIGHT_LONGTEXT, true)
        change_integer_range (0, 65535)
        change_safe ()
    add_bool ("screen-follow-mouse", false, FOLLOW_MOUSE_TEXT,
              FOLLOW_MOUSE_LONGTEXT, true)

    add_shortcut ("screen", "window")
vlc_module_end ()

/*
 * Local prototypes
 */
static void Demux (void *);
static int Control (demux_t *, int, va_list);
static es_out_id_t *InitES (demux_t *, uint_fast16_t, uint_fast16_t,
                            uint_fast8_t, uint8_t *);

struct demux_sys_t
{
    /* All owned by timer thread while timer is armed: */
    xcb_connection_t *conn; /**< XCB connection */
    es_out_id_t      *es; /**< Window capture stream */
    float             rate; /**< Frame rate */
    xcb_window_t      window; /**< Captured window XID  */
    xcb_pixmap_t      pixmap; /**< Pixmap for composited capture */
    xcb_shm_seg_t     segment; /**< SHM segment XID */
    int16_t           x, y; /**< Requested capture top-left coordinates */
    uint16_t          w, h; /**< Requested capture pixel dimensions */
    uint8_t           bpp; /**< Actual bytes per pixel *es */
    bool              shm; /**< Whether to use MIT-SHM */
    bool              follow_mouse;
    uint16_t          cur_w, cur_h; /**< Actual capture pixel dimensions */
    /* Timer does not use this, only input thread: */
    vlc_timer_t       timer;
};

/** Checks MIT-SHM shared memory support */
static bool CheckSHM (xcb_connection_t *conn)
{
#ifdef HAVE_SYS_SHM_H
    xcb_shm_query_version_cookie_t ck = xcb_shm_query_version (conn);
    xcb_shm_query_version_reply_t *r;

    r = xcb_shm_query_version_reply (conn, ck, NULL);
    free (r);
    return r != NULL;
#else
    (void) conn;
    return false;
#endif
}

/**
 * Probes and initializes.
 */
static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *p_sys = malloc (sizeof (*p_sys));

    if (p_sys == NULL)
        return VLC_ENOMEM;
    demux->p_sys = p_sys;

    /* Connect to X server */
    char *display = var_InheritString (obj, "x11-display");
    int snum;
    xcb_connection_t *conn = xcb_connect (display, &snum);
    free (display);
    if (xcb_connection_has_error (conn))
    {
        free (p_sys);
        return VLC_EGENERIC;
    }
    p_sys->conn = conn;

   /* Find configured screen */
    if (!strcmp (demux->psz_access, "screen"))
    {
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
        {
            msg_Err (obj, "bad X11 screen number");
            goto error;
        }
        p_sys->window = scr->root;
    }
    else
    /* Determine capture window */
    if (!strcmp (demux->psz_access, "window"))
    {
        char *end;
        unsigned long ul = strtoul (demux->psz_location, &end, 0);
        if (*end || ul > 0xffffffff)
        {
            msg_Err (obj, "bad X11 drawable %s", demux->psz_location);
            goto error;
        }
        p_sys->window = ul;

        xcb_composite_query_version_reply_t *r =
            xcb_composite_query_version_reply (conn,
                xcb_composite_query_version (conn, 0, 4), NULL);
        if (r == NULL || r->minor_version < 2)
        {
            msg_Err (obj, "X Composite extension not available");
            free (r);
            goto error;
        }
        msg_Dbg (obj, "using Composite extension v%"PRIu32".%"PRIu32,
                 r->major_version, r->minor_version);
        free (r);

        xcb_composite_redirect_window (conn, p_sys->window,
                                       XCB_COMPOSITE_REDIRECT_AUTOMATIC);
    }
    else
        goto error;

    /* Window properties */
    p_sys->pixmap = xcb_generate_id (conn);
    p_sys->segment = xcb_generate_id (conn);
    p_sys->shm = CheckSHM (conn);
    p_sys->w = var_InheritInteger (obj, "screen-width");
    p_sys->h = var_InheritInteger (obj, "screen-height");
    if (p_sys->w != 0 || p_sys->h != 0)
        p_sys->follow_mouse = var_InheritBool (obj, "screen-follow-mouse");
    else /* Following mouse is meaningless if width&height are dynamic. */
        p_sys->follow_mouse = false;
    if (!p_sys->follow_mouse) /* X and Y are meaningless if following mouse */
    {
        p_sys->x = var_InheritInteger (obj, "screen-left");
        p_sys->y = var_InheritInteger (obj, "screen-top");
    }

    /* Initializes format */
    p_sys->rate = var_InheritFloat (obj, "screen-fps");
    if (!p_sys->rate)
        goto error;

    mtime_t interval = (float)CLOCK_FREQ / p_sys->rate;
    if (!interval)
        goto error;

    p_sys->cur_w = 0;
    p_sys->cur_h = 0;
    p_sys->bpp = 0;
    p_sys->es = NULL;
    if (vlc_timer_create (&p_sys->timer, Demux, demux))
        goto error;
    vlc_timer_schedule (p_sys->timer, false, 1, interval);

    /* Initializes demux */
    demux->pf_demux   = NULL;
    demux->pf_control = Control;
    return VLC_SUCCESS;

error:
    xcb_disconnect (p_sys->conn);
    free (p_sys);
    return VLC_EGENERIC;
}


/**
 * Releases resources
 */
static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *p_sys = demux->p_sys;

    vlc_timer_destroy (p_sys->timer);
    xcb_disconnect (p_sys->conn);
    free (p_sys);
}


/**
 * Control callback
 */
static int Control (demux_t *demux, int query, va_list args)
{
    switch (query)
    {
        case DEMUX_GET_POSITION:
        {
            float *v = va_arg (args, float *);
            *v = 0.;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_LENGTH:
        case DEMUX_GET_TIME:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = 0;
            return VLC_SUCCESS;
        }

        /* TODO: get title info -> crawl visible windows */

        case DEMUX_GET_PTS_DELAY:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = INT64_C(1000) * var_InheritInteger (demux, "live-caching");
            return VLC_SUCCESS;
        }

        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_CAN_SEEK:
        {
            bool *v = va_arg (args, bool *);
            *v = false;
            return VLC_SUCCESS;
        }

        case DEMUX_SET_PAUSE_STATE:
            return VLC_SUCCESS; /* should not happen */
    }

    return VLC_EGENERIC;
}


/**
 * Processing callback
 */
static void Demux (void *opaque)
{
    demux_t *demux = opaque;
    demux_sys_t *sys = demux->p_sys;
    xcb_connection_t *conn = sys->conn;

    /* Determine capture region */
    xcb_get_geometry_cookie_t gc;
    xcb_query_pointer_cookie_t qc;

    gc = xcb_get_geometry (conn, sys->window);
    if (sys->follow_mouse)
        qc = xcb_query_pointer (conn, sys->window);

    xcb_get_geometry_reply_t *geo = xcb_get_geometry_reply (conn, gc, NULL);
    if (geo == NULL)
    {
        msg_Err (demux, "bad X11 drawable 0x%08"PRIx32, sys->window);
discard:
        if (sys->follow_mouse)
            xcb_discard_reply (conn, gc.sequence);
        return;
    }

    int w = sys->w;
    int h = sys->h;
    int x, y;

    if (sys->follow_mouse)
    {
        xcb_query_pointer_reply_t *ptr =
            xcb_query_pointer_reply (conn, qc, NULL);
        if (ptr == NULL)
        {
            free (geo);
            return;
        }

        if (w == 0 || w > geo->width)
            w = geo->width;
        x = ptr->win_x;
        if (x < w / 2)
            x = 0;
        else if (x >= (int)geo->width - (w / 2))
            x = geo->width - w;
        else
            x -= w / 2;

        if (h == 0 || h > geo->height)
            h = geo->height;
        y = ptr->win_y;
        if (y < h / 2)
            y = 0;
        else if (y >= (int)geo->height - (h / 2))
            y = geo->height - h;
        else
            y -= h / 2;
    }
    else
    {
        int max;

        x = sys->x;
        max = (int)geo->width - x;
        if (max <= 0)
            goto discard;
        if (w == 0 || w > max)
            w = max;

        y = sys->y;
        max = (int)geo->height - y;
        if (max <= 0)
            goto discard;
        if (h == 0 || h > max)
            h = max;
    }

    /* Update elementary stream format (if needed) */
    if (w != sys->cur_w || h != sys->cur_h)
    {
        if (sys->es != NULL)
            es_out_Del (demux->out, sys->es);

        /* Update composite pixmap */
        if (sys->window != geo->root)
        {
            xcb_free_pixmap (conn, sys->pixmap); /* no-op first time */
            xcb_composite_name_window_pixmap (conn, sys->window, sys->pixmap);
            xcb_create_pixmap (conn, geo->depth, sys->pixmap,
                               geo->root, geo->width, geo->height);
        }

        sys->es = InitES (demux, w, h, geo->depth, &sys->bpp);
        if (sys->es != NULL)
        {
            sys->cur_w = w;
            sys->cur_h = h;
            sys->bpp /= 8; /* bits -> bytes */
        }
    }

    /* Capture screen */
    xcb_drawable_t drawable =
        (sys->window != geo->root) ? sys->pixmap : sys->window;
    free (geo);

    block_t *block = NULL;
#if HAVE_SYS_SHM_H
    if (sys->shm)
    {   /* Capture screen through shared memory */
        size_t size = w * h * sys->bpp;
        int id = shmget (IPC_PRIVATE, size, IPC_CREAT | 0777);
        if (id == -1) /* XXX: fallback */
        {
            msg_Err (demux, "shared memory allocation error: %m");
            goto noshm;
        }

        /* Attach the segment to X and capture */
        xcb_shm_get_image_reply_t *img;
        xcb_shm_get_image_cookie_t ck;

        xcb_shm_attach (conn, sys->segment, id, 0 /* read/write */);
        ck = xcb_shm_get_image (conn, drawable, x, y, w, h, ~0,
                                XCB_IMAGE_FORMAT_Z_PIXMAP, sys->segment, 0);
        xcb_shm_detach (conn, sys->segment);
        img = xcb_shm_get_image_reply (conn, ck, NULL);
        xcb_flush (conn); /* ensure eventual detach */

        if (img == NULL)
        {
            shmctl (id, IPC_RMID, 0);
            goto noshm;
        }
        free (img);

        /* Attach the segment to VLC */
        void *shm = shmat (id, NULL, 0 /* read/write */);
        shmctl (id, IPC_RMID, 0);
        if (-1 == (intptr_t)shm)
        {
            msg_Err (demux, "shared memory attachment error: %m");
            return;
        }

        block = block_shm_Alloc (shm, size);
        if (unlikely(block == NULL))
            shmdt (shm);
    }
noshm:
#endif
    if (block == NULL)
    {   /* Capture screen through socket (fallback) */
        xcb_get_image_reply_t *img;

        img = xcb_get_image_reply (conn,
            xcb_get_image (conn, XCB_IMAGE_FORMAT_Z_PIXMAP, drawable,
                           x, y, w, h, ~0), NULL);
        if (img == NULL)
            return;

        uint8_t *data = xcb_get_image_data (img);
        size_t datalen = xcb_get_image_data_length (img);
        block = block_heap_Alloc (img, data + datalen - (uint8_t *)img);
        if (block == NULL)
            return;
        block->p_buffer = data;
        block->i_buffer = datalen;
    }

    /* Send block - zero copy */
    if (sys->es != NULL)
    {
        block->i_pts = block->i_dts = mdate ();

        es_out_Control (demux->out, ES_OUT_SET_PCR, block->i_pts);
        es_out_Send (demux->out, sys->es, block);
    }
}

static es_out_id_t *InitES (demux_t *demux, uint_fast16_t width,
                            uint_fast16_t height, uint_fast8_t depth,
                            uint8_t *bpp)
{
    demux_sys_t *p_sys = demux->p_sys;
    const xcb_setup_t *setup = xcb_get_setup (p_sys->conn);
    uint32_t chroma = 0;

    for (const xcb_format_t *fmt = xcb_setup_pixmap_formats (setup),
             *end = fmt + xcb_setup_pixmap_formats_length (setup);
         fmt < end; fmt++)
    {
        if (fmt->depth != depth)
            continue;
        switch (fmt->depth)
        {
            case 32:
                if (fmt->bits_per_pixel == 32)
                    chroma = VLC_CODEC_RGBA;
                break;
            case 24:
                if (fmt->bits_per_pixel == 32)
                    chroma = VLC_CODEC_RGB32;
                else if (fmt->bits_per_pixel == 24)
                    chroma = VLC_CODEC_RGB24;
                break;
            case 16:
                if (fmt->bits_per_pixel == 16)
                    chroma = VLC_CODEC_RGB16;
                break;
            case 15:
                if (fmt->bits_per_pixel == 16)
                    chroma = VLC_CODEC_RGB15;
                break;
            case 8: /* XXX: screw grey scale! */
                if (fmt->bits_per_pixel == 8)
                    chroma = VLC_CODEC_RGB8;
                break;
        }
        if (chroma != 0)
        {
            *bpp = fmt->bits_per_pixel;
            break;
        }
    }

    if (!chroma)
    {
        msg_Err (demux, "unsupported pixmap formats");
        return NULL;
    }

    es_format_t fmt;

    es_format_Init (&fmt, VIDEO_ES, chroma);
    fmt.video.i_chroma = chroma;
    fmt.video.i_bits_per_pixel = *bpp;
    fmt.video.i_sar_num = fmt.video.i_sar_den = 1;
    fmt.video.i_frame_rate = 1000 * p_sys->rate;
    fmt.video.i_frame_rate_base = 1000;
    fmt.video.i_visible_width = fmt.video.i_width = width;
    fmt.video.i_visible_height = fmt.video.i_height = height;

    return es_out_Add (demux->out, &fmt);
}
