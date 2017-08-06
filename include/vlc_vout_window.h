/*****************************************************************************
 * vlc_vout_window.h: vout_window_t definitions
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
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

#ifndef VLC_VOUT_WINDOW_H
#define VLC_VOUT_WINDOW_H 1

#include <stdarg.h>
#include <vlc_common.h>

/**
 * \defgroup video_window Video window
 * \ingroup video_output
 * Video output window management
 * @{
 * \file
 * Video output window modules interface
 */

typedef struct vout_window_t vout_window_t;
typedef struct vout_window_sys_t vout_window_sys_t;

struct wl_display;
struct wl_surface;

/**
 * Window handle type
 */
enum vout_window_type {
    VOUT_WINDOW_TYPE_INVALID=0 /**< Invalid or unspecified window type */,
    VOUT_WINDOW_TYPE_XID /**< X11 window */,
    VOUT_WINDOW_TYPE_HWND /**< Win32 or OS/2 window */,
    VOUT_WINDOW_TYPE_NSOBJECT /**< MacOS X view */,
    VOUT_WINDOW_TYPE_ANDROID_NATIVE /**< Android native window */,
    VOUT_WINDOW_TYPE_WAYLAND /**< Wayland surface */,
};

/**
 * Control query for vout_window_t
 */
enum vout_window_control {
    VOUT_WINDOW_SET_STATE, /* unsigned state */
    VOUT_WINDOW_SET_SIZE,   /* unsigned i_width, unsigned i_height */
    VOUT_WINDOW_SET_FULLSCREEN, /* int b_fullscreen */
    VOUT_WINDOW_HIDE_MOUSE, /* int b_hide */
};

/**
 * Window mouse event type for vout_window_mouse_event_t
 */
enum vout_window_mouse_event_type {
    VOUT_WINDOW_MOUSE_STATE,
    VOUT_WINDOW_MOUSE_MOVED,
    VOUT_WINDOW_MOUSE_PRESSED,
    VOUT_WINDOW_MOUSE_RELEASED,
    VOUT_WINDOW_MOUSE_DOUBLE_CLICK,
};

/**
 * Window mouse event
 */
typedef struct vout_window_mouse_event_t
{
    enum vout_window_mouse_event_type type;
    int x;
    int y;
    int button_mask;
} vout_window_mouse_event_t;

typedef struct vout_window_cfg_t {
    /* Window handle type */
    unsigned type;

    /* If true, a standalone window is requested */
    bool is_standalone;
    bool is_fullscreen;

#ifdef __APPLE__
    /* Window position hint */
    int x;
    int y;
#endif

    /* Windows size hint */
    unsigned width;
    unsigned height;

} vout_window_cfg_t;

typedef struct vout_window_owner {
    void *sys;
    void (*resized)(vout_window_t *, unsigned width, unsigned height);
    void (*closed)(vout_window_t *);
    void (*mouse_event)(vout_window_t *, const vout_window_mouse_event_t *mouse);
} vout_window_owner_t;

/**
 * Graphical window
 *
 * This structure is an abstract interface to the windowing system.
 * The window is normally used to draw video (and subpictures) into, but it
 * can also be used for other purpose (e.g. OpenGL visualization).
 *
 * The window is responsible for providing a window handle, whose exact
 * meaning depends on the windowing system. It also must report some events
 * such as user input (keyboard, mouse) and window resize.
 *
 * Finally, it must support some control requests such as for fullscreen mode.
 */
struct vout_window_t {
    VLC_COMMON_MEMBERS

     /**
      * Window handle type
      *
      * This identified the windowing system and protocol that the window
      * needs to use. This also selects which member of the \ref handle union
      * and the \ref display union are to be set.
      *
      * The possible values are defined in \ref vout_window_type.
      *
      * VOUT_WINDOW_TYPE_INVALID is a special placeholder type. It means that
      * any windowing system is acceptable. In that case, the plugin must set
      * its actual type during activation.
      */
    unsigned type;

    /**
     * Window handle (mandatory)
     *
     * This must be filled by the plugin upon activation.
     *
     * Depending on the \ref type above, a different member of this union is
     * used.
     */
    union {
        void     *hwnd;          /**< Win32 window handle */
        uint32_t xid;            /**< X11 windows ID */
        void     *nsobject;      /**< Mac OSX view object */
        void     *anativewindow; /**< Android native window */
        struct wl_surface *wl;   /**< Wayland surface (client pointer) */
    } handle;

    /** Display server (mandatory)
     *
     * This must be filled by the plugin upon activation.
     *
     * The window handle is relative to the display server. The exact meaning
     * of the display server depends on the window handle type. Not all window
     * handle type provide a display server field.
     */
    union {
        char     *x11; /**< X11 display string (NULL = use default) */
        struct wl_display *wl; /**< Wayland display (client pointer) */
    } display;

    /**
     * Control callback (mandatory)
     *
     * This callback handles some control request regarding the window.
     * See \ref vout_window_control.
     *
     * This field should not be used directly when manipulating a window.
     * vout_window_Control() should be used instead.
     */
    int (*control)(vout_window_t *, int query, va_list);

    struct {
        bool has_double_click; /**< Whether double click events are sent,
                                    or need to be emulated */
    } info;

    /* Private place holder for the vout_window_t module (optional)
     *
     * A module is free to use it as it wishes.
     */
    vout_window_sys_t *sys;

    vout_window_owner_t owner;
};

/**
 * Creates a new window.
 *
 * @param module plugin name (usually "$window")
 * @note If you are inside a "vout display", you must use
 / vout_display_NewWindow() and vout_display_DeleteWindow() instead.
 * This enables recycling windows.
 */
VLC_API vout_window_t * vout_window_New(vlc_object_t *, const char *module, const vout_window_cfg_t *, const vout_window_owner_t *);

/**
 * Deletes a window created by vout_window_New().
 *
 * @note See vout_window_New() about window recycling.
 */
VLC_API void vout_window_Delete(vout_window_t *);

void vout_window_SetInhibition(vout_window_t *window, bool enabled);

static inline int vout_window_vaControl(vout_window_t *window, int query,
                                        va_list ap)
{
    return window->control(window, query, ap);
}

/**
 * Reconfigures a window.
 *
 * @note The vout_window_* wrappers should be used instead of this function.
 *
 * @warning The caller must own the window, as vout_window_t is not thread safe.
 */
static inline int vout_window_Control(vout_window_t *window, int query, ...)
{
    va_list ap;
    int ret;

    va_start(ap, query);
    ret = vout_window_vaControl(window, query, ap);
    va_end(ap);
    return ret;
}

/**
 * Configures the window manager state for this window.
 */
static inline int vout_window_SetState(vout_window_t *window, unsigned state)
{
    return vout_window_Control(window, VOUT_WINDOW_SET_STATE, state);
}

/**
 * Configures the window display (i.e. inner/useful) size.
 */
static inline int vout_window_SetSize(vout_window_t *window,
                                      unsigned width, unsigned height)
{
    return vout_window_Control(window, VOUT_WINDOW_SET_SIZE, width, height);
}

/**
 * Sets fullscreen mode.
 */
static inline int vout_window_SetFullScreen(vout_window_t *window, bool full)
{
    return vout_window_Control(window, VOUT_WINDOW_SET_FULLSCREEN, full);
}

/**
 * Hide the mouse cursor
 */
static inline int vout_window_HideMouse(vout_window_t *window, bool hide)
{
    return vout_window_Control(window, VOUT_WINDOW_HIDE_MOUSE, hide);
}

/**
 * Report current window size
 *
 * This notifies the user of the window what the pixel dimensions of the
 * window are (or should be, depending on the windowing system).
 *
 * \note This function is thread-safe. In case of concurrent call, it is
 * undefined which one is taken into account (but at least one is).
 */
static inline void vout_window_ReportSize(vout_window_t *window,
                                          unsigned width, unsigned height)
{
    if (window->owner.resized != NULL)
        window->owner.resized(window, width, height);
}

static inline void vout_window_ReportClose(vout_window_t *window)
{
    if (window->owner.closed != NULL)
        window->owner.closed(window);
}

static inline void vout_window_SendMouseEvent(vout_window_t *window,
                                              const vout_window_mouse_event_t *mouse)
{
    if (window->owner.mouse_event != NULL)
        window->owner.mouse_event(window, mouse);
}

/**
 * Send a full mouse state
 *
 * The mouse position must be expressed against window unit. You can use this
 * function of others vout_window_ReportMouse*() functions.
 */
static inline void vout_window_ReportMouseState(vout_window_t *window,
                                                int x, int y, int button_mask)
{
    const vout_window_mouse_event_t mouse = {
        VOUT_WINDOW_MOUSE_STATE, x, y, button_mask
    };
    vout_window_SendMouseEvent(window, &mouse);
}

/**
 * Send a mouse movement
 *
 * The mouse position must be expressed against window unit.
 */
static inline void vout_window_ReportMouseMoved(vout_window_t *window,
                                                int x, int y)
{
    const vout_window_mouse_event_t mouse = {
        VOUT_WINDOW_MOUSE_MOVED, x, y, 0
    };
    vout_window_SendMouseEvent(window, &mouse);
}

/**
 * Send a mouse pressed event
 */
static inline void vout_window_ReportMousePressed(vout_window_t *window,
                                                  int button)
{
    const vout_window_mouse_event_t mouse = {
        VOUT_WINDOW_MOUSE_PRESSED, 0, 0, button,
    };
    vout_window_SendMouseEvent(window, &mouse);
}

/**
 * Send a mouse released event
 */
static inline void vout_window_ReportMouseReleased(vout_window_t *window,
                                                  int button)
{
    const vout_window_mouse_event_t mouse = {
        VOUT_WINDOW_MOUSE_RELEASED, 0, 0, button,
    };
    vout_window_SendMouseEvent(window, &mouse);
}

/**
 * Send a mouse double click event
 */
static inline void vout_window_ReportMouseDoubleClick(vout_window_t *window,
                                                      int button)
{
    const vout_window_mouse_event_t mouse = {
        VOUT_WINDOW_MOUSE_DOUBLE_CLICK, 0, 0, button,
    };
    vout_window_SendMouseEvent(window, &mouse);
}

/** @} */
#endif /* VLC_VOUT_WINDOW_H */
