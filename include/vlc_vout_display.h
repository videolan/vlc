/*****************************************************************************
 * vlc_vout_display.h: vout_display_t definitions
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_VOUT_DISPLAY_H
#define VLC_VOUT_DISPLAY_H 1

/**
 * \file
 * This file defines vout display structures and functions in vlc
 */

#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>
#include <vlc_subpicture.h>
#include <vlc_keys.h>
#include <vlc_mouse.h>
#include <vlc_vout_window.h>

/* XXX
 * Do NOT use video_format_t::i_aspect but i_sar_num/den everywhere. i_aspect
 * will be removed as soon as possible.
 *
 */
typedef struct vout_display_t vout_display_t;
typedef struct vout_display_sys_t vout_display_sys_t;
typedef struct vout_display_owner_t vout_display_owner_t;
typedef struct vout_display_owner_sys_t vout_display_owner_sys_t;

/**
 * Possible alignments for vout_display.
 */
typedef enum
{
    VOUT_DISPLAY_ALIGN_CENTER,
    /* */
    VOUT_DISPLAY_ALIGN_LEFT,
    VOUT_DISPLAY_ALIGN_RIGHT,
    /* */
    VOUT_DISPLAY_ALIGN_TOP,
    VOUT_DISPLAY_ALIGN_BOTTOM,
} vout_display_align_t;

/**
 * Window management state.
 */
enum {
    VOUT_WINDOW_STATE_NORMAL=0,
    VOUT_WINDOW_STATE_ABOVE=1,
    VOUT_WINDOW_STATE_BELOW=2,
    VOUT_WINDOW_STACK_MASK=3,
};

/**
 * Initial/Current configuration for a vout_display_t
 */
typedef struct {
    bool is_fullscreen;  /* Is the display fullscreen */

    /* Display properties */
    struct {
        /* Window title (may be NULL) */
        const char *title;

        /* Display size */
        unsigned  width;
        unsigned  height;

        /* Display SAR */
        struct {
            unsigned num;
            unsigned den;
        } sar;
    } display;

    /* Alignment of the picture inside the display */
    struct {
        int horizontal;
        int vertical;
    } align;

    /* Do we fill up the display with the video */
    bool is_display_filled;

    /* Zoom to use
     * It will be applied to the whole display if b_display_filled is set, otherwise
     * only on the video source */
    struct {
        int num;
        int den;
    } zoom;

} vout_display_cfg_t;

/**
 * Information from a vout_display_t to configure
 * the core behaviour.
 *
 * By default they are all false or NULL.
 *
 */
typedef struct {
    bool is_slow;                           /* The picture memory has slow read/write */
    bool has_double_click;                  /* Is double-click generated */
    bool has_hide_mouse;                    /* Is mouse automatically hidden */
    bool has_pictures_invalid;              /* Will VOUT_DISPLAY_EVENT_PICTURES_INVALID be used */
    bool has_event_thread;                  /* Will events (key at least) be emitted using an independent thread */
    const vlc_fourcc_t *subpicture_chromas; /* List of supported chromas for subpicture rendering. */
} vout_display_info_t;

/**
 * Control query for vout_display_t
 */
enum {
    /* Hide the mouse. It will be sent when
     * vout_display_t::info.b_hide_mouse is false */
    VOUT_DISPLAY_HIDE_MOUSE,

    /* Ask to reset the internal buffers after a VOUT_DISPLAY_EVENT_PICTURES_INVALID
     * request.
     */
    VOUT_DISPLAY_RESET_PICTURES,

    /* Ask the module to acknowledge/refuse the fullscreen state change after
     * being requested (externally or by VOUT_DISPLAY_EVENT_FULLSCREEN */
    VOUT_DISPLAY_CHANGE_FULLSCREEN,     /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse the window management state change
     * after being requested externally or by VOUT_DISPLAY_WINDOW_STATE */
    VOUT_DISPLAY_CHANGE_WINDOW_STATE,         /* unsigned state */

    /* Ask the module to acknowledge/refuse the display size change requested
     * (externally or by VOUT_DISPLAY_EVENT_DISPLAY_SIZE) */
    VOUT_DISPLAY_CHANGE_DISPLAY_SIZE,   /* const vout_display_cfg_t *p_cfg, int is_forced */

    /* Ask the module to acknowledge/refuse fill display state change after
     * being requested externally */
    VOUT_DISPLAY_CHANGE_DISPLAY_FILLED, /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse zoom change after being requested
     * externally */
    VOUT_DISPLAY_CHANGE_ZOOM, /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse source aspect ratio after being
     * requested externally */
    VOUT_DISPLAY_CHANGE_SOURCE_ASPECT, /* const video_format_t *p_source */

    /* Ask the module to acknowledge/refuse source crop change after being
     * requested externally.
     * The cropping requested is stored by video_format_t::i_x/y_offset and
     * video_format_t::i_visible_width/height */
    VOUT_DISPLAY_CHANGE_SOURCE_CROP,   /* const video_format_t *p_source */

    /* Ask an opengl interface if available. */
    VOUT_DISPLAY_GET_OPENGL,           /* vlc_gl_t ** */
};

/**
 * Event from vout_display_t
 *
 * Events modifiying the state may be sent multiple times.
 * Only the transition will be retained and acted upon.
 */
enum {
    /* TODO:
     * ZOOM ? DISPLAY_FILLED ? ON_TOP ?
     */
    /* */
    VOUT_DISPLAY_EVENT_PICTURES_INVALID,    /* The buffer are now invalid and need to be changed */

    VOUT_DISPLAY_EVENT_FULLSCREEN,
    VOUT_DISPLAY_EVENT_WINDOW_STATE,

    VOUT_DISPLAY_EVENT_DISPLAY_SIZE,        /* The display size need to change : int i_width, int i_height, bool is_fullscreen */

    /* */
    VOUT_DISPLAY_EVENT_CLOSE,
    VOUT_DISPLAY_EVENT_KEY,

    /* Full mouse state.
     * You can use it OR use the other mouse events. The core will do
     * the conversion.
     */
    VOUT_DISPLAY_EVENT_MOUSE_STATE,

    /* Mouse event */
    VOUT_DISPLAY_EVENT_MOUSE_MOVED,
    VOUT_DISPLAY_EVENT_MOUSE_PRESSED,
    VOUT_DISPLAY_EVENT_MOUSE_RELEASED,
    VOUT_DISPLAY_EVENT_MOUSE_DOUBLE_CLICK,
};

/**
 * Vout owner structures
 */
struct vout_display_owner_t {
    /* Private place holder for the vout_display_t creator
     */
    vout_display_owner_sys_t *sys;

    /* Event coming from the module
     *
     * This function is set prior to the module instantiation and must not
     * be overwritten nor used directly (use the vout_display_SendEvent*
     * wrapper.
     *
     * You can send it at any time i.e. from any vout_display_t functions or
     * from another thread.
     * Be careful, it does not ensure correct serialization if it is used
     * from multiple threads.
     */
    void            (*event)(vout_display_t *, int, va_list);

    /* Window management
     *
     * These functions are set prior to the module instantiation and must not
     * be overwritten nor used directly (use the vout_display_*Window
     * wrapper */
    vout_window_t *(*window_new)(vout_display_t *, const vout_window_cfg_t *);
    void           (*window_del)(vout_display_t *, vout_window_t *);
};

struct vout_display_t {
    VLC_COMMON_MEMBERS

    /* Module */
    module_t *module;

    /* Initial and current configuration.
     * You cannot modify it directly, you must use the appropriate events.
     *
     * It reflects the current values, i.e. after the event has been accepted
     * and applied/configured if needed.
     */
    const vout_display_cfg_t *cfg;

    /* video source format.
     *
     * Cropping is not requested while in the open function.
     * You cannot change it.
     */
    video_format_t source;

    /* picture_t format.
     *
     * You can only change it inside the module open function to
     * match what you want, and when a VOUT_DISPLAY_RESET_PICTURES control
     * request is made and succeeds.
     *
     * By default, it is equal to ::source except for the aspect ratio
     * which is undefined(0) and is ignored.
     */
    video_format_t fmt;

    /* Information
     *
     * You can only set them in the open function.
     */
    vout_display_info_t info;

    /* Return a pointer over the current picture_pool_t* (mandatory).
     *
     * For performance reasons, it is best to provide at least count
     * pictures but it is not mandatory.
     * You can return NULL when you cannot/do not want to allocate
     * pictures.
     * The vout display module keeps the ownership of the pool and can
     * destroy it only when closing or on invalid pictures control.
     */
    picture_pool_t *(*pool)(vout_display_t *, unsigned count);

    /* Prepare a picture and an optional subpicture for display (optional).
     *
     * It is called before the next pf_display call to provide as much
     * time as possible to prepare the given picture and the subpicture
     * for display.
     * You are guaranted that pf_display will always be called and using
     * the exact same picture_t and subpicture_t.
     * You cannot change the pixel content of the picture_t or of the
     * subpicture_t.
     */
    void       (*prepare)(vout_display_t *, picture_t *, subpicture_t *);

    /* Display a picture and an optional subpicture (mandatory).
     *
     * The picture and the optional subpicture must be displayed as soon as
     * possible.
     * You cannot change the pixel content of the picture_t or of the
     * subpicture_t.
     *
     * This function gives away the ownership of the picture and of the
     * subpicture, so you must release them as soon as possible.
     */
    void       (*display)(vout_display_t *, picture_t *, subpicture_t *);

    /* Control on the module (mandatory) */
    int        (*control)(vout_display_t *, int, va_list);

    /* Manage pending event (optional) */
    void       (*manage)(vout_display_t *);

    /* Private place holder for the vout_display_t module (optional)
     *
     * A module is free to use it as it wishes.
     */
    vout_display_sys_t *sys;

    /* Reserved for the vout_display_t owner.
     *
     * It must not be overwritten nor used directly by a module.
     */
    vout_display_owner_t owner;
};

static inline void vout_display_SendEvent(vout_display_t *vd, int query, ...)
{
    va_list args;
    va_start(args, query);
    vd->owner.event(vd, query, args);
    va_end(args);
}

static inline void vout_display_SendEventDisplaySize(vout_display_t *vd, int width, int height, bool is_fullscreen)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_DISPLAY_SIZE, width, height, is_fullscreen);
}
static inline void vout_display_SendEventPicturesInvalid(vout_display_t *vd)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_PICTURES_INVALID);
}
static inline void vout_display_SendEventClose(vout_display_t *vd)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_CLOSE);
}
static inline void vout_display_SendEventKey(vout_display_t *vd, int key)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_KEY, key);
}
static inline void vout_display_SendEventFullscreen(vout_display_t *vd, bool is_fullscreen)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_FULLSCREEN, is_fullscreen);
}
static inline void vout_display_SendWindowState(vout_display_t *vd, unsigned state)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_WINDOW_STATE, state);
}
/* The mouse position (State and Moved event) must be expressed against vout_display_t::source unit */
static inline void vout_display_SendEventMouseState(vout_display_t *vd, int x, int y, int button_mask)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_MOUSE_STATE, x, y, button_mask);
}
static inline void vout_display_SendEventMouseMoved(vout_display_t *vd, int x, int y)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_MOUSE_MOVED, x, y);
}
static inline void vout_display_SendEventMousePressed(vout_display_t *vd, int button)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_MOUSE_PRESSED, button);
}
static inline void vout_display_SendEventMouseReleased(vout_display_t *vd, int button)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_MOUSE_RELEASED, button);
}
static inline void vout_display_SendEventMouseDoubleClick(vout_display_t *vd)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_MOUSE_DOUBLE_CLICK);
}

/**
 * Asks for a new window with the given configuration as hint.
 *
 * b_standalone/i_x/i_y may be overwritten by the core
 */
static inline vout_window_t *vout_display_NewWindow(vout_display_t *vd, const vout_window_cfg_t *cfg)
{
    return vd->owner.window_new(vd, cfg);
}
/**
 * Deletes a window created by vout_display_NewWindow if window is non NULL
 * or any unused windows otherwise.
 */
static inline void vout_display_DeleteWindow(vout_display_t *vd,
                                             vout_window_t *window)
{
    vd->owner.window_del(vd, window);
}

/**
 * Computes the default display size given the source and
 * the display configuration.
 *
 * This asssumes that the picture is already cropped.
 */
VLC_API void vout_display_GetDefaultDisplaySize(unsigned *width, unsigned *height, const video_format_t *source, const vout_display_cfg_t *);


/**
 * Structure used to store the result of a vout_display_PlacePicture.
 */
typedef struct {
    int x;
    int y;
    unsigned width;
    unsigned height;
} vout_display_place_t;

/**
 * Computes how to place a picture inside the display to respect
 * the given parameters.
 * This assumes that cropping is done by an external mean.
 *
 * \param p_place Place inside the window (window pixel unit)
 * \param p_source Video source format
 * \param p_cfg Display configuration
 * \param b_clip If true, prevent the video to go outside the display (break zoom).
 */
VLC_API void vout_display_PlacePicture(vout_display_place_t *place, const video_format_t *source, const vout_display_cfg_t *cfg, bool do_clipping);

#endif /* VLC_VOUT_DISPLAY_H */

