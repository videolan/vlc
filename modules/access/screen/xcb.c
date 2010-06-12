/**
 * @file xcb.c
 * @brief X11 C Bindings screen capture demux module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
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
#include <stdarg.h>
#include <assert.h>
#include <xcb/xcb.h>
#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for screen capture. " \
    "This value should be set in milliseconds.")

#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_( \
    "How many times the screen content should be refreshed per second.")

#define LEFT_TEXT N_("Region left column")
#define LEFT_LONGTEXT N_( \
    "Abscissa of the capture reion in pixels.")

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

    add_integer ("screen-caching", DEFAULT_PTS_DELAY * 1000 / CLOCK_FREQ,
                 NULL, CACHING_TEXT, CACHING_LONGTEXT, true)
    add_float ("screen-fps", 2.0, NULL, FPS_TEXT, FPS_LONGTEXT, true)
    add_integer ("screen-left", 0, NULL, LEFT_TEXT, LEFT_LONGTEXT, true)
        change_integer_range (-32768, 32767)
        change_safe ()
    add_integer ("screen-top", 0, NULL, LEFT_TEXT, LEFT_LONGTEXT, true)
        change_integer_range (-32768, 32767)
        change_safe ()
    add_integer ("screen-width", 0, NULL, LEFT_TEXT, LEFT_LONGTEXT, true)
        change_integer_range (0, 65535)
        change_safe ()
    add_integer ("screen-height", 0, NULL, LEFT_TEXT, LEFT_LONGTEXT, true)
        change_integer_range (0, 65535)
        change_safe ()
    add_bool ("screen-follow-mouse", false, NULL, FOLLOW_MOUSE_TEXT,
              FOLLOW_MOUSE_LONGTEXT, true)

    add_shortcut ("screen", "window")
vlc_module_end ()

/*
 * Local prototypes
 */
static void Demux (void *);
static int Control (demux_t *, int, va_list);
static es_out_id_t *InitES (demux_t *, uint_fast16_t, uint_fast16_t,
                            uint_fast8_t);

struct demux_sys_t
{
    xcb_connection_t *conn;
    es_out_id_t      *es;
    mtime_t           pts, interval;
    float             rate;
    xcb_window_t      window;
    int16_t           x, y;
    uint16_t          w, h;
    uint16_t          cur_w, cur_h;
    bool              follow_mouse;
    /* fmt, es and pts are protected by the lock. The rest is read-only. */
    vlc_mutex_t       lock;
    /* Timer does not use this, only input thread: */
    vlc_timer_t       timer;
};

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
    char *display = var_CreateGetNonEmptyString (obj, "x11-display");
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
        unsigned long ul = strtoul (demux->psz_path, &end, 0);
        if (*end || ul > 0xffffffff)
        {
            msg_Err (obj, "bad X11 drawable %s", demux->psz_path);
            goto error;
        }
        p_sys->window = ul;
    }
    else
        goto error;

    /* Window properties */
    p_sys->x = var_CreateGetInteger (obj, "screen-left");
    p_sys->y = var_CreateGetInteger (obj, "screen-top");
    p_sys->w = var_CreateGetInteger (obj, "screen-width");
    p_sys->h = var_CreateGetInteger (obj, "screen-height");
    p_sys->follow_mouse = var_CreateGetBool (obj, "screen-follow-mouse");

    /* Initializes format */
    p_sys->rate = var_CreateGetFloat (obj, "screen-fps");
    if (!p_sys->rate)
        goto error;
    p_sys->interval = (float)CLOCK_FREQ / p_sys->rate;
    if (!p_sys->interval)
        goto error;
    var_Create (obj, "screen-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT);

    p_sys->cur_w = 0;
    p_sys->cur_h = 0;
    p_sys->es = NULL;
    p_sys->pts = VLC_TS_INVALID;
    vlc_mutex_init (&p_sys->lock);
    if (vlc_timer_create (&p_sys->timer, Demux, demux))
        goto error;
    vlc_timer_schedule (p_sys->timer, false, 1, p_sys->interval);

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
    vlc_mutex_destroy (&p_sys->lock);
    xcb_disconnect (p_sys->conn);
    free (p_sys);
}


/**
 * Control callback
 */
static int Control (demux_t *demux, int query, va_list args)
{
    demux_sys_t *p_sys = demux->p_sys;

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
            *v = var_GetInteger (demux, "screen-caching") * UINT64_C(1000);
            return VLC_SUCCESS;
        }

        case DEMUX_CAN_PAUSE:
        {
            bool *v = (bool*)va_arg( args, bool * );
            *v = true;
            return VLC_SUCCESS;
        }

        case DEMUX_SET_PAUSE_STATE:
        {
            bool pausing = va_arg (args, int);

            if (!pausing)
            {
                vlc_mutex_lock (&p_sys->lock);
                p_sys->pts = VLC_TS_INVALID;
                es_out_Control (demux->out, ES_OUT_RESET_PCR);
                vlc_mutex_unlock (&p_sys->lock);
            }
            vlc_timer_schedule (p_sys->timer, false,
                                pausing ? 0 : 1, p_sys->interval);
            return VLC_SUCCESS;
        }

        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_CAN_SEEK:
        {
            bool *v = (bool*)va_arg( args, bool * );
            *v = false;
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}


/**
 * Processing callback
 */
static void Demux (void *data)
{
    demux_t *demux = data;
    demux_sys_t *p_sys = demux->p_sys;
    xcb_connection_t *conn = p_sys->conn;

    /* Update capture region (if needed) */

    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply (conn,
            xcb_get_geometry (conn, p_sys->window), NULL);
    if (geo == NULL)
    {
        msg_Err (demux, "bad X11 drawable 0x%08"PRIx32, p_sys->window);
        return;
    }

    xcb_window_t root = geo->root;
    int16_t x = p_sys->x, y = p_sys->y;
    xcb_translate_coordinates_cookie_t tc;
    xcb_query_pointer_cookie_t qc;

    if (p_sys->window != root)
        tc = xcb_translate_coordinates (conn, p_sys->window, root,
                                        x, y);
    if (p_sys->follow_mouse)
        qc = xcb_query_pointer (conn, p_sys->window);

    uint16_t ow = geo->width - x;
    uint16_t oh = geo->height - y;
    uint16_t w = p_sys->w;
    uint16_t h = p_sys->h;
    if (w == 0 || w > ow)
        w = ow;
    if (h == 0 || h > oh)
        h = oh;
    uint8_t depth = geo->depth;
    free (geo);

    if (p_sys->window != root)
    {
        xcb_translate_coordinates_reply_t *coords =
             xcb_translate_coordinates_reply (conn, tc, NULL);
        if (coords != NULL)
        {
            x = coords->dst_x;
            y = coords->dst_y;
            free (coords);
        }
    }

    if (p_sys->follow_mouse)
    {
        xcb_query_pointer_reply_t *ptr =
            xcb_query_pointer_reply (conn, qc, NULL);
        if (ptr != NULL)
        {
            int16_t min_x = x + (w / 2);
            int16_t min_y = y + (h / 2);
            int16_t max_x = x + ow - ((w + 1) / 2);
            int16_t max_y = y + oh - ((h + 1) / 2);

            assert (max_x >= min_x); /* max_x - min_x = ow - w >= 0 */
            if (ptr->root_x > max_x)
                x += ow - w;
            else if (ptr->root_x > min_x)
                x += ptr->root_x - min_x;

            assert (max_y >= min_y);
            if (ptr->root_y > max_y)
                y += oh - h;
            else if (ptr->root_y > min_y)
                y += ptr->root_y - min_y;
        }
    }

    xcb_get_image_reply_t *img;
    img = xcb_get_image_reply (conn,
        xcb_get_image (conn, XCB_IMAGE_FORMAT_Z_PIXMAP, root,
                       x, y, w, h, ~0), NULL);
    if (img == NULL)
        return;

    /* Send block - zero copy */
    block_t *block = block_heap_Alloc (img, xcb_get_image_data (img),
                                       xcb_get_image_data_length (img));
    if (block == NULL)
        return;

    vlc_mutex_lock (&p_sys->lock);
    if (w != p_sys->cur_w || h != p_sys->cur_h)
    {
        if (p_sys->es != NULL)
            es_out_Del (demux->out, p_sys->es);
        p_sys->es = InitES (demux, w, h, depth);
        if (p_sys->es != NULL)
        {
            p_sys->cur_w = w;
            p_sys->cur_h = h;
        }
    }

    /* Capture screen */
    if (p_sys->es != NULL)
    {
        if (p_sys->pts == VLC_TS_INVALID)
            p_sys->pts = mdate ();
        block->i_pts = block->i_dts = p_sys->pts;

        es_out_Control (demux->out, ES_OUT_SET_PCR, p_sys->pts);
        es_out_Send (demux->out, p_sys->es, block);
        p_sys->pts += p_sys->interval;
    }
    vlc_mutex_unlock (&p_sys->lock);
}

static es_out_id_t *InitES (demux_t *demux, uint_fast16_t width,
                            uint_fast16_t height, uint_fast8_t depth)
{
    demux_sys_t *p_sys = demux->p_sys;
    const xcb_setup_t *setup = xcb_get_setup (p_sys->conn);
    uint32_t chroma = 0;
    uint8_t bpp;

    for (const xcb_format_t *fmt = xcb_setup_pixmap_formats (setup),
             *end = fmt + xcb_setup_pixmap_formats_length (setup);
         fmt < end; fmt++)
    {
        if (fmt->depth != depth)
            continue;
        bpp = fmt->depth;
        switch (fmt->depth)
        {
            case 32:
                if (fmt->bits_per_pixel == 32)
                    chroma = VLC_CODEC_RGBA;
                break;
            case 24:
                if (fmt->bits_per_pixel == 32)
                {
                    chroma = VLC_CODEC_RGB32;
                    bpp = 32;
                }
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
            break;
    }

    if (!chroma)
    {
        msg_Err (demux, "unsupported pixmap formats");
        return NULL;
    }

    es_format_t fmt;

    es_format_Init (&fmt, VIDEO_ES, chroma);
    fmt.video.i_chroma = chroma;
    fmt.video.i_bits_per_pixel = bpp;
    fmt.video.i_sar_num = fmt.video.i_sar_den = 1;
    fmt.video.i_frame_rate = 1000 * p_sys->rate;
    fmt.video.i_frame_rate_base = 1000;
    fmt.video.i_visible_width = fmt.video.i_width = width;
    fmt.video.i_visible_height = fmt.video.i_height = height;

    return es_out_Add (demux->out, &fmt);
}
