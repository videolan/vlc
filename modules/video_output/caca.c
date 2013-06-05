/*****************************************************************************
 * caca.c: Color ASCII Art "vout display" module using libcaca
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#if !defined(_WIN32) && !defined(__APPLE__)
# ifdef X_DISPLAY_MISSING
#  error Xlib required due to XInitThreads
# endif
# include <vlc_xlib.h>
#endif

#include <caca.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname("Caca")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description(N_("Color ASCII art video output"))
    set_capability("vout display", 15)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Prepare(vout_display_t *, picture_t *, subpicture_t *);
static void    PictureDisplay(vout_display_t *, picture_t *, subpicture_t *);
static int            Control(vout_display_t *, int, va_list);

/* */
static void Manage(vout_display_t *);
static void Refresh(vout_display_t *);
static void Place(vout_display_t *, vout_display_place_t *);

/* */
struct vout_display_sys_t {
    cucul_canvas_t *cv;
    caca_display_t *dp;
    cucul_dither_t *dither;

    picture_pool_t *pool;
};

/**
 * This function initializes libcaca vout method.
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

#if !defined(__APPLE__) && !defined(_WIN32)
# ifndef X_DISPLAY_MISSING
    if (!vlc_xlib_init(object))
        return VLC_EGENERIC;
# endif
#endif

#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    SMALL_RECT rect;
    COORD coord;
    HANDLE hstdout;

    if (!AllocConsole()) {
        msg_Err(vd, "cannot create console");
        return VLC_EGENERIC;
    }

    hstdout =
        CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (!hstdout || hstdout == INVALID_HANDLE_VALUE) {
        msg_Err(vd, "cannot create screen buffer");
        FreeConsole();
        return VLC_EGENERIC;
    }

    if (!SetConsoleActiveScreenBuffer(hstdout)) {
        msg_Err(vd, "cannot set active screen buffer");
        FreeConsole();
        return VLC_EGENERIC;
    }

    coord = GetLargestConsoleWindowSize(hstdout);
    msg_Dbg(vd, "SetConsoleWindowInfo: %ix%i", coord.X, coord.Y);

    /* Force size for now */
    coord.X = 100;
    coord.Y = 40;

    if (!SetConsoleScreenBufferSize(hstdout, coord))
        msg_Warn(vd, "SetConsoleScreenBufferSize %i %i",
                  coord.X, coord.Y);

    /* Get the current screen buffer size and window position. */
    if (GetConsoleScreenBufferInfo(hstdout, &csbiInfo)) {
        rect.Top = 0; rect.Left = 0;
        rect.Right = csbiInfo.dwMaximumWindowSize.X - 1;
        rect.Bottom = csbiInfo.dwMaximumWindowSize.Y - 1;
        if (!SetConsoleWindowInfo(hstdout, TRUE, &rect))
            msg_Dbg(vd, "SetConsoleWindowInfo failed: %ix%i",
                     rect.Right, rect.Bottom);
    }
#endif

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        goto error;

    sys->cv = cucul_create_canvas(0, 0);
    if (!sys->cv) {
        msg_Err(vd, "cannot initialize libcucul");
        goto error;
    }

    const char *driver = NULL;
#ifdef __APPLE__
    // Make sure we don't try to open a window.
    driver = "ncurses";
#endif

    sys->dp = caca_create_display_with_driver(sys->cv, driver);
    if (!sys->dp) {
        msg_Err(vd, "cannot initialize libcaca");
        goto error;
    }
    vout_display_DeleteWindow(vd, NULL);

    if (vd->cfg->display.title)
        caca_set_display_title(sys->dp,
                               vd->cfg->display.title);
    else
        caca_set_display_title(sys->dp,
                               VOUT_TITLE "(Colour AsCii Art)");

    /* Fix format */
    video_format_t fmt = vd->fmt;
    if (fmt.i_chroma != VLC_CODEC_RGB32) {
        fmt.i_chroma = VLC_CODEC_RGB32;
        fmt.i_rmask = 0x00ff0000;
        fmt.i_gmask = 0x0000ff00;
        fmt.i_bmask = 0x000000ff;
    }

    /* TODO */
    vout_display_info_t info = vd->info;

    /* Setup vout_display now that everything is fine */
    vd->fmt = fmt;
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = PictureDisplay;
    vd->control = Control;
    vd->manage  = Manage;

    /* Fix initial state */
    vout_display_SendEventFullscreen(vd, false);
    Refresh(vd);

    return VLC_SUCCESS;

error:
    if (sys) {
        if (sys->pool)
            picture_pool_Delete(sys->pool);
        if (sys->dither)
            cucul_free_dither(sys->dither);
        if (sys->dp)
            caca_free_display(sys->dp);
        if (sys->cv)
            cucul_free_canvas(sys->cv);

        free(sys);
    }
#if defined(_WIN32)
    FreeConsole();
#endif
    return VLC_EGENERIC;
}

/**
 * Close a libcaca video output
 */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);
    if (sys->dither)
        cucul_free_dither(sys->dither);
    caca_free_display(sys->dp);
    cucul_free_canvas(sys->cv);

#if defined(_WIN32)
    FreeConsole();
#endif

    free(sys);
}

/**
 * Return a pool of direct buffers
 */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return sys->pool;
}

/**
 * Prepare a picture for display */
static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->dither) {
        /* Create the libcaca dither object */
        sys->dither = cucul_create_dither(32,
                                            vd->source.i_visible_width,
                                            vd->source.i_visible_height,
                                            picture->p[0].i_pitch,
                                            vd->fmt.i_rmask,
                                            vd->fmt.i_gmask,
                                            vd->fmt.i_bmask,
                                            0x00000000);

        if (!sys->dither) {
            msg_Err(vd, "could not create libcaca dither object");
            return;
        }
    }

    vout_display_place_t place;
    Place(vd, &place);

    cucul_set_color_ansi(sys->cv, CUCUL_COLOR_DEFAULT, CUCUL_COLOR_BLACK);
    cucul_clear_canvas(sys->cv);

    const int crop_offset = vd->source.i_y_offset * picture->p->i_pitch +
                            vd->source.i_x_offset * picture->p->i_pixel_pitch;
    cucul_dither_bitmap(sys->cv, place.x, place.y,
                        place.width, place.height,
                        sys->dither,
                        &picture->p->p_pixels[crop_offset]);
    VLC_UNUSED(subpicture);
}

/**
 * Display a picture
 */
static void PictureDisplay(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    Refresh(vd);
    picture_Release(picture);
    VLC_UNUSED(subpicture);
}

/**
 * Control for vout display
 */
static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_HIDE_MOUSE:
        caca_set_mouse(sys->dp, 0);
        return VLC_SUCCESS;

    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE: {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);

        caca_refresh_display(sys->dp);

        /* Not quite good but not sure how to resize it */
        if ((int)cfg->display.width  != caca_get_display_width(sys->dp) ||
            (int)cfg->display.height != caca_get_display_height(sys->dp))
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        return VLC_SUCCESS;

    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        if (sys->dither)
            cucul_free_dither(sys->dither);
        sys->dither = NULL;
        return VLC_SUCCESS;

    default:
        msg_Err(vd, "Unsupported query in vout display caca");
        return VLC_EGENERIC;
    }
}

/**
 * Refresh the display and send resize event
 */
static void Refresh(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /* */
    caca_refresh_display(sys->dp);

    /* */
    const unsigned width  = caca_get_display_width(sys->dp);
    const unsigned height = caca_get_display_height(sys->dp);

    if (width  != vd->cfg->display.width ||
        height != vd->cfg->display.height)
        vout_display_SendEventDisplaySize(vd, width, height, false);
}

/**
 * Compute the place in canvas unit.
 */
static void Place(vout_display_t *vd, vout_display_place_t *place)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_PlacePicture(place, &vd->source, vd->cfg, false);

    const int canvas_width   = cucul_get_canvas_width(sys->cv);
    const int canvas_height  = cucul_get_canvas_height(sys->cv);
    const int display_width  = caca_get_display_width(sys->dp);
    const int display_height = caca_get_display_height(sys->dp);

    if (display_width > 0 && display_height > 0) {
        place->x      =  place->x      * canvas_width  / display_width;
        place->y      =  place->y      * canvas_height / display_height;
        place->width  = (place->width  * canvas_width  + display_width/2)  / display_width;
        place->height = (place->height * canvas_height + display_height/2) / display_height;
    } else {
        place->x = 0;
        place->y = 0;
        place->width  = canvas_width;
        place->height = display_height;
    }
}

/* */
static const struct {
    int caca;
    int vlc;
} keys[] = {

    { CACA_KEY_CTRL_A,  KEY_MODIFIER_CTRL | 'a' },
    { CACA_KEY_CTRL_B,  KEY_MODIFIER_CTRL | 'b' },
    { CACA_KEY_CTRL_C,  KEY_MODIFIER_CTRL | 'c' },
    { CACA_KEY_CTRL_D,  KEY_MODIFIER_CTRL | 'd' },
    { CACA_KEY_CTRL_E,  KEY_MODIFIER_CTRL | 'e' },
    { CACA_KEY_CTRL_F,  KEY_MODIFIER_CTRL | 'f' },
    { CACA_KEY_CTRL_G,  KEY_MODIFIER_CTRL | 'g' },
    { CACA_KEY_BACKSPACE, KEY_BACKSPACE },
    { CACA_KEY_TAB,     KEY_TAB },
    { CACA_KEY_CTRL_J,  KEY_MODIFIER_CTRL | 'j' },
    { CACA_KEY_CTRL_K,  KEY_MODIFIER_CTRL | 'k' },
    { CACA_KEY_CTRL_L,  KEY_MODIFIER_CTRL | 'l' },
    { CACA_KEY_RETURN,  KEY_ENTER },

    { CACA_KEY_CTRL_N,  KEY_MODIFIER_CTRL | 'n' },
    { CACA_KEY_CTRL_O,  KEY_MODIFIER_CTRL | 'o' },
    { CACA_KEY_CTRL_P,  KEY_MODIFIER_CTRL | 'p' },
    { CACA_KEY_CTRL_Q,  KEY_MODIFIER_CTRL | 'q' },
    { CACA_KEY_CTRL_R,  KEY_MODIFIER_CTRL | 'r' },

    { CACA_KEY_PAUSE,   -1 },
    { CACA_KEY_CTRL_T,  KEY_MODIFIER_CTRL | 't' },
    { CACA_KEY_CTRL_U,  KEY_MODIFIER_CTRL | 'u' },
    { CACA_KEY_CTRL_V,  KEY_MODIFIER_CTRL | 'v' },
    { CACA_KEY_CTRL_W,  KEY_MODIFIER_CTRL | 'w' },
    { CACA_KEY_CTRL_X,  KEY_MODIFIER_CTRL | 'x' },
    { CACA_KEY_CTRL_Y,  KEY_MODIFIER_CTRL | 'y' },
    { CACA_KEY_CTRL_Z,  KEY_MODIFIER_CTRL | 'z' },

    { CACA_KEY_ESCAPE,  KEY_ESC },
    { CACA_KEY_DELETE,  KEY_DELETE },

    { CACA_KEY_F1,      KEY_F1 },
    { CACA_KEY_F2,      KEY_F2 },
    { CACA_KEY_F3,      KEY_F3 },
    { CACA_KEY_F4,      KEY_F4 },
    { CACA_KEY_F5,      KEY_F5 },
    { CACA_KEY_F6,      KEY_F6 },
    { CACA_KEY_F7,      KEY_F7 },
    { CACA_KEY_F8,      KEY_F8 },
    { CACA_KEY_F9,      KEY_F9 },
    { CACA_KEY_F10,     KEY_F10 },
    { CACA_KEY_F11,     KEY_F11 },
    { CACA_KEY_F12,     KEY_F12 },
    { CACA_KEY_F13,     -1 },
    { CACA_KEY_F14,     -1 },
    { CACA_KEY_F15,     -1 },

    { CACA_KEY_UP,      KEY_UP },
    { CACA_KEY_DOWN,    KEY_DOWN },
    { CACA_KEY_LEFT,    KEY_LEFT },
    { CACA_KEY_RIGHT,   KEY_RIGHT },

    { CACA_KEY_INSERT,  KEY_INSERT },
    { CACA_KEY_HOME,    KEY_HOME },
    { CACA_KEY_END,     KEY_END },
    { CACA_KEY_PAGEUP,  KEY_PAGEUP },
    { CACA_KEY_PAGEDOWN,KEY_PAGEDOWN },

    /* */
    { -1, -1 }
};

static const struct {
    int caca;
    int vlc;
} mouses[] = {
    { 1, MOUSE_BUTTON_LEFT },
    { 2, MOUSE_BUTTON_CENTER },
    { 3, MOUSE_BUTTON_RIGHT },
    { 4, MOUSE_BUTTON_WHEEL_UP },
    { 5, MOUSE_BUTTON_WHEEL_DOWN },

    /* */
    { -1, -1 }
};

/**
 * Proccess pending event
 */
static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    struct caca_event ev;
    while (caca_get_event(sys->dp, CACA_EVENT_ANY, &ev, 0) > 0) {
        switch (caca_get_event_type(&ev)) {
        case CACA_EVENT_KEY_PRESS: {
            const int caca = caca_get_event_key_ch(&ev);

            for (int i = 0; keys[i].caca != -1; i++) {
                if (keys[i].caca == caca) {
                    const int vlc = keys[i].vlc;

                    if (vlc >= 0)
                        vout_display_SendEventKey(vd, vlc);
                    return;
                }
            }
            if (caca >= 0x20 && caca <= 0x7f) {
                vout_display_SendEventKey(vd, caca);
                return;
            }
            break;
        }
        case CACA_EVENT_RESIZE:
            vout_display_SendEventDisplaySize(vd, caca_get_event_resize_width(&ev),
                                                  caca_get_event_resize_height(&ev), false);
            break;
        case CACA_EVENT_MOUSE_MOTION: {
            vout_display_place_t place;
            Place(vd, &place);

            const unsigned x = vd->source.i_x_offset +
                               (int64_t)(caca_get_event_mouse_x(&ev) - place.x) *
                                    vd->source.i_visible_width / place.width;
            const unsigned y = vd->source.i_y_offset +
                               (int64_t)(caca_get_event_mouse_y(&ev) - place.y) *
                                    vd->source.i_visible_height / place.height;

            caca_set_mouse(sys->dp, 1);
            vout_display_SendEventMouseMoved(vd, x, y);
            break;
        }
        case CACA_EVENT_MOUSE_PRESS:
        case CACA_EVENT_MOUSE_RELEASE: {
            caca_set_mouse(sys->dp, 1);
            const int caca = caca_get_event_mouse_button(&ev);
            for (int i = 0; mouses[i].caca != -1; i++) {
                if (mouses[i].caca == caca) {
                    if (caca_get_event_type(&ev) == CACA_EVENT_MOUSE_PRESS)
                        vout_display_SendEventMousePressed(vd, mouses[i].vlc);
                    else
                        vout_display_SendEventMouseReleased(vd, mouses[i].vlc);
                    return;
                }
            }
            break;
        }
        case CACA_EVENT_QUIT:
            vout_display_SendEventClose(vd);
            break;
        default:
            break;
        }
    }
}

