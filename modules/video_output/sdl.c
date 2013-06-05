/*****************************************************************************
 * sdl.c: SDL video output display method
 *****************************************************************************
 * Copyright (C) 1998-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Pierre Baillet <oct@zoy.org>
 *          Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
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

#include <assert.h>

#include <SDL.h>

#if !defined(_WIN32) && !defined(__OS2__)
# ifdef X_DISPLAY_MISSING
#  error Xlib required due to XInitThreads
# endif
# include <vlc_xlib.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define CHROMA_TEXT N_("SDL chroma format")
#define CHROMA_LONGTEXT N_(\
    "Force the SDL renderer to use a specific chroma format instead of " \
    "trying to improve performances by using the most efficient one.")

vlc_module_begin()
    set_shortname("SDL")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description(N_("Simple DirectMedia Layer video output"))
    set_capability("vout display", 70)
    add_shortcut("sdl")
    add_string("sdl-chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true)
    add_obsolete_string("sdl-video-driver") /* obsolete since 1.1.0 */
    set_callbacks(Open, Close)
#if defined(__i386__) || defined(__x86_64__)
    /* On i386, SDL is linked against svgalib */
    cannot_unload_broken_library()
#endif
vlc_module_end()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           PictureDisplay(vout_display_t *, picture_t *, subpicture_t *);
static int            Control(vout_display_t *, int, va_list);
static void           Manage(vout_display_t *);

/* */
static int ConvertKey(SDLKey);

/* */
static vlc_mutex_t sdl_lock = VLC_STATIC_MUTEX;

/* */
struct vout_display_sys_t {
    vout_display_place_t place;

    SDL_Surface          *display;
    int                  display_bpp;
    uint32_t             display_flags;

    unsigned int         desktop_width;
    unsigned int         desktop_height;

    /* For YUV output */
    SDL_Overlay          *overlay;
    bool                 is_uv_swapped;

    /* */
    picture_pool_t       *pool;
};

/**
 * This function initializes SDL vout method.
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

#if !defined(_WIN32) && !defined(__OS2__)
    if (!vlc_xlib_init (object))
        return VLC_EGENERIC;
#endif

    /* XXX: check for conflicts with the SDL audio output */
    vlc_mutex_lock(&sdl_lock);

    /* Check if SDL video module has been initialized */
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        vlc_mutex_unlock(&sdl_lock);
        return VLC_EGENERIC;
    }

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys) {
        vlc_mutex_unlock(&sdl_lock);
        return VLC_ENOMEM;
    }

    /* */
    int sdl_flags = SDL_INIT_VIDEO;
#ifndef _WIN32
    /* Win32 SDL implementation doesn't support SDL_INIT_EVENTTHREAD yet*/
    sdl_flags |= SDL_INIT_EVENTTHREAD;
#endif
    /* In debug mode you may want vlc to dump a core instead of staying stuck */
    sdl_flags |= SDL_INIT_NOPARACHUTE;

    /* Initialize library */
    if (SDL_Init(sdl_flags) < 0) {
        vlc_mutex_unlock(&sdl_lock);

        msg_Err(vd, "cannot initialize SDL (%s)", SDL_GetError());
        free(sys);
        return VLC_EGENERIC;
    }
    vlc_mutex_unlock(&sdl_lock);

    /* Translate keys into unicode */
    SDL_EnableUNICODE(1);

    /* Get the desktop resolution */
    /* FIXME: SDL has a problem with virtual desktop */
    sys->desktop_width  = SDL_GetVideoInfo()->current_w;
    sys->desktop_height = SDL_GetVideoInfo()->current_h;

    /* */
    video_format_t fmt = vd->fmt;

    /* */
    vout_display_info_t info = vd->info;

    /* Set main window's size */
    int display_width;
    int display_height;
    if (vd->cfg->is_fullscreen) {
        display_width  = sys->desktop_width;
        display_height = sys->desktop_height;
    } else {
        display_width  = vd->cfg->display.width;
        display_height = vd->cfg->display.height;
    }

    /* Initialize flags and cursor */
    sys->display_flags = SDL_ANYFORMAT | SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF;
    sys->display_flags |= vd->cfg->is_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;

    sys->display_bpp = SDL_VideoModeOK(display_width, display_height,
                                       16, sys->display_flags);
    if (sys->display_bpp == 0) {
        msg_Err(vd, "no video mode available");
        goto error;
    }
    vout_display_DeleteWindow(vd, NULL);

    sys->display = SDL_SetVideoMode(display_width, display_height,
                                    sys->display_bpp, sys->display_flags);
    if (!sys->display) {
        msg_Err(vd, "cannot set video mode");
        goto error;
    }

    /* We keep the surface locked forever */
    SDL_LockSurface(sys->display);

    /* */
    vlc_fourcc_t forced_chroma = 0;
    char *psz_chroma = var_InheritString(vd, "sdl-chroma");
    if (psz_chroma) {
        forced_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, psz_chroma);
        if (forced_chroma)
            msg_Dbg(vd, "Forcing chroma to 0x%.8x (%4.4s)",
                    forced_chroma, (const char*)&forced_chroma);
        free(psz_chroma);
    }

    /* Try to open an overlay if requested */
    sys->overlay = NULL;
    const bool is_overlay = var_InheritBool(vd, "overlay");
    if (is_overlay) {
        static const struct
        {
            vlc_fourcc_t vlc;
            uint32_t     sdl;
        } vlc_to_sdl[] = {
            { VLC_CODEC_YV12, SDL_YV12_OVERLAY },
            { VLC_CODEC_I420, SDL_IYUV_OVERLAY },
            { VLC_CODEC_YUYV, SDL_YUY2_OVERLAY },
            { VLC_CODEC_UYVY, SDL_UYVY_OVERLAY },
            { VLC_CODEC_YVYU, SDL_YVYU_OVERLAY },

            { 0, 0 }
        };
        const vlc_fourcc_t forced_chromas[] = {
            forced_chroma, 0
        };
        const vlc_fourcc_t *fallback_chromas =
            vlc_fourcc_GetYUVFallback(fmt.i_chroma);
        const vlc_fourcc_t *chromas = forced_chroma ? forced_chromas : fallback_chromas;

        for (int pass = forced_chroma ? 1 : 0; pass < 2 && !sys->overlay; pass++) {
            for (int i = 0; chromas[i] != 0; i++) {
                const vlc_fourcc_t vlc = chromas[i];

                uint32_t sdl = 0;
                for (int j = 0; vlc_to_sdl[j].vlc != 0 && !sdl; j++) {
                    if (vlc_to_sdl[j].vlc == vlc)
                        sdl = vlc_to_sdl[j].sdl;
                }
                if (!sdl)
                    continue;

                sys->overlay = SDL_CreateYUVOverlay(fmt.i_width, fmt.i_height,
                                                    sdl, sys->display);
                if (sys->overlay && !sys->overlay->hw_overlay && pass == 0) {
                    /* Ignore non hardware overlay surface in first pass */
                    SDL_FreeYUVOverlay(sys->overlay);
                    sys->overlay = NULL;
                }
                if (sys->overlay) {
                    /* We keep the surface locked forever */
                    SDL_LockYUVOverlay(sys->overlay);

                    fmt.i_chroma = vlc;
                    sys->is_uv_swapped = vlc_fourcc_AreUVPlanesSwapped(fmt.i_chroma,
                                                                       vd->fmt.i_chroma);
                    if (sys->is_uv_swapped)
                        fmt.i_chroma = vd->fmt.i_chroma;
                    break;
                }
            }
        }
    } else {
        msg_Warn(vd, "SDL overlay disabled by the user");
    }

    /* */
    vout_display_cfg_t place_cfg = *vd->cfg;
    place_cfg.display.width  = display_width;
    place_cfg.display.height = display_height;
    vout_display_PlacePicture(&sys->place, &vd->source, &place_cfg, !sys->overlay);

    /* If no overlay, fallback to software output */
    if (!sys->overlay) {
        /* */
        switch (sys->display->format->BitsPerPixel) {
        case 8:
            fmt.i_chroma = VLC_CODEC_RGB8;
            break;
        case 15:
            fmt.i_chroma = VLC_CODEC_RGB15;
            break;
        case 16:
            fmt.i_chroma = VLC_CODEC_RGB16;
            break;
        case 24:
            fmt.i_chroma = VLC_CODEC_RGB24;
            break;
        case 32:
            fmt.i_chroma = VLC_CODEC_RGB32;
            break;
        default:
            msg_Err(vd, "unknown screen depth %i",
                    sys->display->format->BitsPerPixel);
            goto error;
        }

        /* All we have is an RGB image with square pixels */
        fmt.i_width  = display_width;
        fmt.i_height = display_height;
        fmt.i_rmask = sys->display->format->Rmask;
        fmt.i_gmask = sys->display->format->Gmask;
        fmt.i_bmask = sys->display->format->Bmask;

        info.has_pictures_invalid = true;
    }

    if (vd->cfg->display.title)
        SDL_WM_SetCaption(vd->cfg->display.title,
                          vd->cfg->display.title);
    else if (!sys->overlay)
        SDL_WM_SetCaption(VOUT_TITLE " (software RGB SDL output)",
                          VOUT_TITLE " (software RGB SDL output)");
    else if (sys->overlay->hw_overlay)
        SDL_WM_SetCaption(VOUT_TITLE " (hardware YUV SDL output)",
                          VOUT_TITLE " (hardware YUV SDL output)");
    else
        SDL_WM_SetCaption(VOUT_TITLE " (software YUV SDL output)",
                          VOUT_TITLE " (software YUV SDL output)");

    /* Setup events */
    SDL_EventState(SDL_KEYUP, SDL_IGNORE);               /* ignore keys up */

    /* Setup vout_display now that everything is fine */
    vd->fmt = fmt;
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = PictureDisplay;
    vd->control = Control;
    vd->manage  = Manage;

    /* */
    vout_display_SendEventDisplaySize(vd, display_width, display_height, vd->cfg->is_fullscreen);
    return VLC_SUCCESS;

error:
    msg_Err(vd, "cannot set up SDL (%s)", SDL_GetError());

    if (sys->display) {
        SDL_UnlockSurface(sys->display);
        SDL_FreeSurface(sys->display);
    }

    vlc_mutex_lock(&sdl_lock);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    vlc_mutex_unlock(&sdl_lock);

    free(sys);
    return VLC_EGENERIC;
}

/**
 * Close a SDL video output
 */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);

    if (sys->overlay) {
        SDL_LockYUVOverlay(sys->overlay);
        SDL_FreeYUVOverlay(sys->overlay);
    }
    SDL_UnlockSurface (sys->display);
    SDL_FreeSurface(sys->display);

    vlc_mutex_lock(&sdl_lock);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    vlc_mutex_unlock(&sdl_lock);

    free(sys);
}

/**
 * Return a pool of direct buffers
 */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(count);

    if (!sys->pool) {
        picture_resource_t rsc;

        memset(&rsc, 0, sizeof(rsc));

        if (sys->overlay) {
            SDL_Overlay *ol = sys->overlay;

            for (int i = 0; i < ol->planes; i++) {
                rsc.p[i].p_pixels = ol->pixels[ i > 0 && sys->is_uv_swapped ? (3-i) : i];
                rsc.p[i].i_pitch  = ol->pitches[i > 0 && sys->is_uv_swapped ? (3-i) : i];
                rsc.p[i].i_lines  = ol->h;
                if (ol->format == SDL_YV12_OVERLAY ||
                    ol->format == SDL_IYUV_OVERLAY)
                    rsc.p[i].i_lines /= 2;

            }
        } else {
            const int x = sys->place.x;
            const int y = sys->place.y;

            SDL_Surface *sf = sys->display;
            SDL_FillRect(sf, NULL, 0);

            assert(x >= 0 && y >= 0);
            rsc.p[0].p_pixels = (uint8_t*)sf->pixels + y * sf->pitch + x * ((sf->format->BitsPerPixel + 7) / 8);
            rsc.p[0].i_pitch  = sf->pitch;
            rsc.p[0].i_lines  = vd->fmt.i_height;
        }

        picture_t *picture = picture_NewFromResource(&vd->fmt, &rsc);;
        if (!picture)
            return NULL;

        sys->pool = picture_pool_New(1, &picture);
    }

    return sys->pool;
}

/**
 * Display a picture
 */
static void PictureDisplay(vout_display_t *vd, picture_t *p_pic, subpicture_t *p_subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->overlay) {
        SDL_Rect disp;
        disp.x = sys->place.x;
        disp.y = sys->place.y;
        disp.w = sys->place.width;
        disp.h = sys->place.height;

        SDL_UnlockYUVOverlay(sys->overlay);
        SDL_DisplayYUVOverlay(sys->overlay , &disp);
        SDL_LockYUVOverlay(sys->overlay);
    } else {
        SDL_Flip(sys->display);
    }

    picture_Release(p_pic);
    VLC_UNUSED(p_subpicture);
}


/**
 * Control for vout display
 */
static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_HIDE_MOUSE:
        SDL_ShowCursor(0);
        return VLC_SUCCESS;

    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE: {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);

        /* */
        sys->display = SDL_SetVideoMode(cfg->display.width,
                                        cfg->display.height,
                                        sys->display_bpp, sys->display_flags);
        if (!sys->display) {
            sys->display = SDL_SetVideoMode(vd->cfg->display.width,
                                            vd->cfg->display.height,
                                            sys->display_bpp, sys->display_flags);
            return VLC_EGENERIC;
        }
        if (sys->overlay)
            vout_display_PlacePicture(&sys->place, &vd->source, cfg, !sys->overlay);
        else
            vout_display_SendEventPicturesInvalid(vd);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_FULLSCREEN: {
        vout_display_cfg_t cfg = *va_arg(args, const vout_display_cfg_t *);

        /* Fix flags */
        sys->display_flags &= ~(SDL_FULLSCREEN | SDL_RESIZABLE);
        sys->display_flags |= cfg.is_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;

        if (cfg.is_fullscreen) {
            cfg.display.width = sys->desktop_width;
            cfg.display.height = sys->desktop_height;
        }

        if (sys->overlay) {
            sys->display = SDL_SetVideoMode(cfg.display.width, cfg.display.height,
                                            sys->display_bpp, sys->display_flags);

            vout_display_PlacePicture(&sys->place, &vd->source, &cfg, !sys->overlay);
        }
        vout_display_SendEventDisplaySize(vd, cfg.display.width, cfg.display.height, cfg.is_fullscreen);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT: {
        const vout_display_cfg_t *cfg;
        const video_format_t *source;

        if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT) {
            source = va_arg(args, const video_format_t *);
            cfg = vd->cfg;
        } else {
            source = &vd->source;
            cfg = va_arg(args, const vout_display_cfg_t *);
        }
        if (sys->overlay) {
            sys->display = SDL_SetVideoMode(cfg->display.width, cfg->display.height,
                                            sys->display_bpp, sys->display_flags);

            vout_display_PlacePicture(&sys->place, source, cfg, !sys->overlay);
        } else {
            vout_display_SendEventPicturesInvalid(vd);
        }
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_RESET_PICTURES: {
        /* */
        assert(!sys->overlay);

        /* */
        if (sys->pool)
            picture_pool_Delete(sys->pool);
        sys->pool = NULL;

        vout_display_PlacePicture(&sys->place, &vd->source, vd->cfg, !sys->overlay);

        /* */
        vd->fmt.i_width  = sys->place.width;
        vd->fmt.i_height = sys->place.height;
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        /* I don't think it is possible to support with SDL:
         * - crop
         * - on top
         */
        return VLC_EGENERIC;

    default:
        msg_Err(vd, "Unsupported query in vout display SDL");
        return VLC_EGENERIC;
    }
}

/**
 * Proccess pending event
 */
static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    SDL_Event event;

    /* */
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            vout_display_SendEventClose(vd);
            break;

        case SDL_KEYDOWN: {
            /* convert the key if possible */
            int key = ConvertKey(event.key.keysym.sym);

            if (!key) {
                /* Find the right caracter */
                if ((event.key.keysym.unicode & 0xff80) == 0) {
                    key = event.key.keysym.unicode & 0x7f;
                    /* FIXME: find a better solution than this
                              hack to find the right caracter */
                    if (key >= 1 && key <= 26)
                        key += 96;
                    else if (key >= 65 && key <= 90)
                        key += 32;
                }
            }
            if (!key)
                break;

            if (event.key.keysym.mod & KMOD_SHIFT)
                key |= KEY_MODIFIER_SHIFT;
            if (event.key.keysym.mod & KMOD_CTRL)
                key |= KEY_MODIFIER_CTRL;
            if (event.key.keysym.mod & KMOD_ALT)
                key |= KEY_MODIFIER_ALT;
            vout_display_SendEventKey(vd, key);
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            static const struct { int sdl; int vlc; } buttons[] = {
                { SDL_BUTTON_LEFT,      MOUSE_BUTTON_LEFT },
                { SDL_BUTTON_MIDDLE,    MOUSE_BUTTON_CENTER },
                { SDL_BUTTON_RIGHT,     MOUSE_BUTTON_RIGHT },
                { SDL_BUTTON_WHEELUP,   MOUSE_BUTTON_WHEEL_UP },
                { SDL_BUTTON_WHEELDOWN, MOUSE_BUTTON_WHEEL_DOWN },
                { -1, -1 },
            };

            SDL_ShowCursor(1);
            for (int i = 0; buttons[i].sdl != -1; i++) {
                if (buttons[i].sdl == event.button.button) {
                    if (event.type == SDL_MOUSEBUTTONDOWN)
                        vout_display_SendEventMousePressed(vd, buttons[i].vlc);
                    else
                        vout_display_SendEventMouseReleased(vd, buttons[i].vlc);
                }
            }
            break;
        }

        case SDL_MOUSEMOTION: {
            if (sys->place.width <= 0 || sys->place.height <= 0)
                break;

            const int x = (int64_t)(event.motion.x - sys->place.x) * vd->source.i_width  / sys->place.width;
            const int y = (int64_t)(event.motion.y - sys->place.y) * vd->source.i_height / sys->place.height;

            SDL_ShowCursor(1);
            vout_display_SendEventMouseMoved(vd, x, y);
            break;
        }

        case SDL_VIDEORESIZE:
            vout_display_SendEventDisplaySize(vd, event.resize.w, event.resize.h, vd->cfg->is_fullscreen);
            break;

        default:
            break;
        }
    }

}

static const struct {
    SDLKey sdl_key;
    int    vlckey;

} sdlkeys_to_vlckeys[] = {
    { SDLK_F1,  KEY_F1 },
    { SDLK_F2,  KEY_F2 },
    { SDLK_F3,  KEY_F3 },
    { SDLK_F4,  KEY_F4 },
    { SDLK_F5,  KEY_F5 },
    { SDLK_F6,  KEY_F6 },
    { SDLK_F7,  KEY_F7 },
    { SDLK_F8,  KEY_F8 },
    { SDLK_F9,  KEY_F9 },
    { SDLK_F10, KEY_F10 },
    { SDLK_F11, KEY_F11 },
    { SDLK_F12, KEY_F12 },

    { SDLK_RETURN, KEY_ENTER },
    { SDLK_KP_ENTER, KEY_ENTER },
    { SDLK_SPACE, ' ' },
    { SDLK_ESCAPE, KEY_ESC },

    { SDLK_MENU, KEY_MENU },
    { SDLK_LEFT, KEY_LEFT },
    { SDLK_RIGHT, KEY_RIGHT },
    { SDLK_UP, KEY_UP },
    { SDLK_DOWN, KEY_DOWN },

    { SDLK_HOME, KEY_HOME },
    { SDLK_END, KEY_END },
    { SDLK_PAGEUP, KEY_PAGEUP },
    { SDLK_PAGEDOWN,  KEY_PAGEDOWN },

    { SDLK_INSERT, KEY_INSERT },
    { SDLK_DELETE, KEY_DELETE },
    /*TODO: find a equivalent for SDL 
    { , KEY_MEDIA_NEXT_TRACK }
    { , KEY_MEDIA_PREV_TRACK }
    { , KEY_VOLUME_MUTE }
    { , KEY_VOLUME_DOWN }
    { , KEY_VOLUME_UP }
    { , KEY_MEDIA_PLAY_PAUSE }
    { , KEY_MEDIA_PLAY_PAUSE }*/

    { 0, 0 }
};

static int ConvertKey(SDLKey sdl_key)
{
    for (int i = 0; sdlkeys_to_vlckeys[i].sdl_key != 0; i++) {
        if (sdlkeys_to_vlckeys[i].sdl_key == sdl_key)
            return sdlkeys_to_vlckeys[i].vlckey;
    }
    return 0;
}

