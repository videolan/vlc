/*****************************************************************************
 * vlc_window.h: vlc_window definitions
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
 * Copyright (C) 2009 Laurent Aimar
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
 * Window management
 *
 * Window management provides a partial abstraction for windowing systems and
 * rendering targets (i.e. "windows"). See \ref vlc_window_t.
 *
 * @{
 * \file
 * Window modules interface
 */

struct vlc_window;
struct wl_display;
struct wl_surface;
typedef struct vlc_icc_profile_t vlc_icc_profile_t;

/**
 * Window handle type.
 *
 * The window handle type specifies the window system protocol that the
 * window was created with. It determines which members of the
 * vlc_window_t::handle and vlc_window_t::display unions are defined
 * for the given window.
 *
 * It also establishes some protocol-dependent semantics such as the exact
 * interpretation of the window state (\ref vlc_window_state)
 * and the window size.
 */
enum vlc_window_type {
    VLC_WINDOW_TYPE_DUMMY /**< Dummy window (not an actual window) */,
    VLC_WINDOW_TYPE_XID /**< X11 window */,
    VLC_WINDOW_TYPE_HWND /**< Win32 or OS/2 window */,
    VLC_WINDOW_TYPE_NSOBJECT /**< macOS/iOS view */,
    VLC_WINDOW_TYPE_ANDROID_NATIVE /**< Android native window */,
    VLC_WINDOW_TYPE_WAYLAND /**< Wayland surface */,
    VLC_WINDOW_TYPE_DCOMP /**< Win32 DirectComposition */,
    VLC_WINDOW_TYPE_KMS /**< DRM KMS CRTC */,
};

/**
 * Window states.
 *
 * Currently, this only handles different window stacking orders.
 * See also \ref vlc_window_SetState().
 */
enum vlc_window_state {
    VLC_WINDOW_STATE_NORMAL /**< Normal stacking */,
    VLC_WINDOW_STATE_ABOVE /**< Stacking above (a.k.a. always on top) */,
    VLC_WINDOW_STATE_BELOW /**< Stacking below (a.k.a. wall paper mode) */,
};

/**
 * Window mouse event types.
 *
 * This enumeration defines the possible event types
 * vlc_window_mouse_event_t::type.
 */
enum vlc_window_mouse_event_type {
    VLC_WINDOW_MOUSE_MOVED /**< Pointer position change */,
    VLC_WINDOW_MOUSE_PRESSED /**< Pointer button press or single click */,
    VLC_WINDOW_MOUSE_RELEASED /**< Pointer button release */,
    VLC_WINDOW_MOUSE_DOUBLE_CLICK /**< Double click */,
};

/**
 * Window mouse event.
 *
 * This structure describes a pointer input event on a window.
 */
typedef struct vlc_window_mouse_event
{
    enum vlc_window_mouse_event_type type; /**< Event type. */

    /**
     * Pointer abscissa.
     *
     * The pointer abscissa is relative to the window and expressed in pixels.
     * Abscissa goes from left to right, such that the left-most column is at 0
     * and the right-most column is at width minus one.
     *
     * A negative abscissa refers to pixels to the left of the window, and
     * an abscissa of width or larger refers to pixels to the right.
     *
     * This is only set if @c event equals \ref VLC_WINDOW_MOUSE_MOVED.
     */
    int x;

    /**
     * Pointer ordinate.
     *
     * The pointer ordinate is relative to the window and expressed in pixels.
     * Ordinate goes from top to bottom, such that the top-most row is at 0
     * and the bottom-most column is at height minus one.
     *
     * A negative ordinate refers to pixels above the window, and
     * an ordinate of height or larger refers to pixels below the window.
     *
     * This is only set if @c event equals \ref VLC_WINDOW_MOUSE_MOVED.
     */
    int y;

    /**
     * Pressed button.
     *
     * See \ref vlc_mouse_button for possible values.
     *
     * This is set if @c event does not equal \ref VLC_WINDOW_MOUSE_MOVED.
     */
    int button_mask;
} vlc_window_mouse_event_t;

/**
 * Window (desired) configuration.
 *
 * This structure describes the intended initial configuration
 * of a \ref vlc_window_t.
 */
typedef struct vlc_window_cfg {
    /**
     * Whether the window should be in full screen mode or not.
     */
    bool is_fullscreen;

    /**
     * Whether the window should have decorations or not.
     */
    bool is_decorated;

#if defined(__APPLE__) || defined(_WIN32)
    /* Window position hint */
    int x;
    int y;
#endif

    /**
     * Intended pixel width of the window.
     */
    unsigned width;

    /**
     * Intended pixel height of the window.
     */
    unsigned height;

} vlc_window_cfg_t;

/**
 * Callback prototype for window event acknowledgement.
 *
 * @param width pixel width as supplied to vlc_window_callbacks::resized
 * @param height pixel height as supplied to vlc_window_callbacks::resized
 * @param data opaque pointer as supplied to vlc_window_callbacks::resized
 */
typedef void (*vlc_window_ack_cb)(struct vlc_window *, unsigned width,
                                   unsigned height, void *data);

/**
 * Window event callbacks structure.
 *
 * This structure provided to vlc_window_New() conveys callbacks to handle
 * window events.
 *
 * As a general rule, the events can occur synchronously or asynchronously from
 * the time that the window is (successfully) being created by vlc_window_New()
 * until the time that the window has been deleted by vlc_window_Delete().
 *
 * \warning
 * Also, a window object functions are not reentrant, so the callbacks must not
 * invoke the window object functions.
 * Otherwise a deadlock or infinite recursion may occur.
 */
struct vlc_window_callbacks {
    /**
     * Callback for window size changes.
     *
     * This callback function is invoked when the windowing
     * system changes the window size.
     *
     * This event may occur synchronously when the window is created or a size
     * change is requested. It may also occur asynchronously as a consequence
     * of external events from the windowing system, or deferred processing of
     * a size change request.
     *
     * If a non-NULL acknowledgement callback is specified, it is called
     * synchronously after the consumer of the window has been notified of the
     * size change, and before any further processing by the consumer. In other
     * words, the callback invocation is after all rendering operations using
     * the previous old window size, and before all rendering operations using
     * the new window size.
     *
     * \param cb optional acknowledgement callback function (NULL to ignore)
     * \param opaque opaque data pointer for the acknowledgement callback
     */
    void (*resized)(struct vlc_window *, unsigned width, unsigned height,
                    vlc_window_ack_cb cb, void *opaque);

    /**
     * Callback for window closing.
     *
     * This callback function (if non-NULL) is invoked upon an external request
     * to close the window. Not all windowing systems support this.
     *
     * Soon after this callback, the window should be disabled with
     * vlc_window_Disable().
     *
     * \warning Do not disable the window within the callback.
     * That could lead to a dead lock.
     */
    void (*closed)(struct vlc_window *);

    /**
     * Callback for window state change.
     *
     * This callback function (if non-NULL) is invoked when the window state
     * as changed, either as a consequence of vlc_window_SetSate() or external
     * events.
     *
     * \bug Many window back-ends fail to invoke this callback when due.
     *
     * \param state new window state (see \ref vlc_window_state).
     */
    void (*state_changed)(struct vlc_window *, unsigned state);

    /**
     * Callback for windowed mode.
     *
     * This callback function (if non-NULL) is invoked when the window becomes
     * windowed. It might also occur spuriously if the window remains windowed.
     *
     * \bug Many window back-ends fail to invoke this callback when due.
     */
    void (*windowed)(struct vlc_window *);

    /**
     * Callback for fullscreen mode.
     *
     * This callback function (if non-NULL) is invoked when the window becomes
     * fullscreen, when it changes to a different fullscreen output, or
     * spuriously when the window remains in fullscreen mode.
     *
     * \bug Many window back-ends fail to invoke this callback when due.
     *
     * \param id fullscreen output identifier (NULL if unspecified)
     */
    void (*fullscreened)(struct vlc_window *, const char *id);

    /**
     * Callback for pointer input events.
     *
     * This callback function (if non-NULL) is invoked upon any pointer input
     * event on the window. See \ref vlc_window_mouse_event_t.
     *
     * \param mouse pointer to the input event.
     */
    void (*mouse_event)(struct vlc_window *,
                        const vlc_window_mouse_event_t *mouse);

    /**
     * Callback for keyboard input events.
     *
     * This callback function (if non-NULL) is invoked upon any keyboard key
     * press event, or repetition event, on the window.
     *
     * \note No events are delivered for keyboard key releases.
     *
     * \param key VLC key code
     */
    void (*keyboard_event)(struct vlc_window *, unsigned key);

    /**
     * Callback for fullscreen output enumeration.
     *
     * This callback function (if non-NULL) indicates that a fullscreen output
     * becomes available, changes human-readable description, or becomes
     * unavailable.
     *
     * \param id nul-terminated id fullscreen output identifier
     *           (cannot be NULL)
     * \param desc nul-terminated human-readable description,
     *             or NULL if the output has become unavailable
     */
    void (*output_event)(struct vlc_window *,
                         const char *id, const char *desc);

    /**
     * Callback for ICC profile update.
     *
     * This can happen either because of the window being moved to a different
     * display, or because the ICC profile associated with a display is
     * updated. Memory transfers to the callee.
     *
     * \param profile ICC profile associated with the window, or NULL to
     *                indicate absence of an ICC profile
     */
    void (*icc_event)(struct vlc_window *, vlc_icc_profile_t *profile);
};

/**
 * Window callbacks and opaque data.
 */
typedef struct vlc_window_owner {
    const struct vlc_window_callbacks *cbs; /**< Callbacks */
    void *sys; /**< Opaque data / private pointer for callbacks */
} vlc_window_owner_t;

/**
 * Window implementation callbacks.
 */
struct vlc_window_operations {
    int (*enable)(struct vlc_window *, const vlc_window_cfg_t *);
    void (*disable)(struct vlc_window *);
    void (*resize)(struct vlc_window *, unsigned width, unsigned height);

    /**
     * Destroy the window.
     *
     * Destroys the window and releases all associated resources.
     */
    void (*destroy)(struct vlc_window *);

    void (*set_state)(struct vlc_window *, unsigned state);
    void (*unset_fullscreen)(struct vlc_window *);
    void (*set_fullscreen)(struct vlc_window *, const char *id);
    void (*set_title)(struct vlc_window *, const char *id);
};

/**
 * Window object.
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
typedef struct vlc_window {
    struct vlc_object_t obj;

     /**
      * Window handle type
      *
      * This identified the windowing system and protocol that the window
      * needs to use. This also selects which member of the \ref handle union
      * and the \ref display union are to be set.
      *
      * The possible values are defined in \ref vlc_window_type.
      */
    unsigned type;

    /**
     * Window handle (mandatory)
     *
     * This must be filled by the plugin upon successful vlc_window_Enable().
     *
     * Depending on the \ref type above, a different member of this union is
     * used.
     */
    union {
        void     *hwnd;          /**< Win32 window handle */
        uint32_t xid;            /**< X11 windows ID */
        void     *nsobject;      /**< macOS/iOS view object */
        void     *anativewindow; /**< Android native window */
        struct wl_surface *wl;   /**< Wayland surface (client pointer) */
        void     *dcomp_visual;  /**<  Win32 direct composition visual */
        uint32_t crtc;           /**< KMS CRTC identifier */
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
        void* dcomp_device; /**< DirectComposition device */
        int      drm_fd; /**< KMS DRM device */
    } display;

    const struct vlc_window_operations *ops; /**< operations handled by the
                             window. Once this is set it MUST NOT be changed */

    struct {
        bool has_double_click; /**< Whether double click events are sent,
                                    or need to be emulated */
    } info;

    /* Private place holder for the vlc_window_t module (optional)
     *
     * A module is free to use it as it wishes.
     */
    void *sys;

    vlc_window_owner_t owner;
} vlc_window_t;

/**
 * Creates a new window.
 *
 * This function creates a window, or some other kind of rectangle render
 * target.
 *
 * \param obj parent VLC object
 * \param module plugin name, NULL for default
 * \param owner callbacks and private data
 * \param cfg initial window configuration, NULL for defaults
 * \return a new window, or NULL on error.
 */
VLC_API vlc_window_t *vlc_window_New(vlc_object_t *obj,
                                       const char *module,
                                       const vlc_window_owner_t *owner,
                                       const vlc_window_cfg_t *cfg);

/**
 * Deletes a window.
 *
 * This deletes a window created by vlc_window_New().
 *
 * \param window window object to delete
 */
VLC_API void vlc_window_Delete(vlc_window_t *window);

/**
 * Inhibits or deinhibits the screensaver.
 *
 * \param window window in respect to which the screensaver should be inhibited
 *               or deinhibited
 * \param enabled true to inhibit, false to deinhibit
 */
void vlc_window_SetInhibition(vlc_window_t *window, bool enabled);

/**
 * Requests a new window state.
 *
 * This requests a change of the state of a window from the windowing system.
 * See \ref vlc_window_state for possible states.
 *
 * @param window window whose state to change
 * @param state requested state
 */
static inline void vlc_window_SetState(vlc_window_t *window, unsigned state)
{
    if (window->ops->set_state != NULL)
        window->ops->set_state(window, state);
}

/**
 * Requests a new window size.
 *
 * This requests a change of the window size. In general and unless otherwise
 * stated, the size is expressed in pixels. However, the exact interpretation
 * of the window size depends on the windowing system.
 *
 * There is no return value as the request may be processed asynchronously,
 * ignored and/or modified by the window system. The actual size of the window
 * is determined by the vlc_window_callbacks::resized callback function that
 * was supplied to vlc_window_New().
 *
 * \note The size is expressed in terms of the "useful" area,
 * i.e. it excludes any side decoration added by the windowing system.
 *
 * \param window window whom a size change is requested for
 * \param width pixel width
 * \param height height width
 */
VLC_API void vlc_window_SetSize(vlc_window_t *window,
                                 unsigned width, unsigned height);

/**
 * Requests fullscreen mode.
 *
 * \param window window to be brought to fullscreen mode.
 * \param id nul-terminated output identifier, NULL for default
 */
VLC_API void vlc_window_SetFullScreen(vlc_window_t *window, const char *id);

/**
 * Requests windowed mode.
 *
 * \param window window to be brought into windowed mode.
 */
VLC_API void vlc_window_UnsetFullScreen(vlc_window_t *window);

/**
 * Request a new window title.
 *
 * \param window window to change the title.
 * \param title window title to use.
 */
static inline void vlc_window_SetTitle(vlc_window_t *window, const char *title)
{
    if (window->ops->set_title != NULL)
        window->ops->set_title(window, title);
}

/**
 * Enables a window.
 *
 * This informs the window provider that the window is about to be taken into
 * active use. A window is always initially disabled. This is so that the
 * window provider can provide a persistent connection to the display server,
 * and track any useful events, such as monitors hotplug.
 *
 * The window handle (vlc_window_t.handle) must remain valid and constant
 * while the window is enabled.
 *
 * \param window window to enable
 * \param cfg initial configuration for the window
 */
VLC_API
int vlc_window_Enable(vlc_window_t *window);

/**
 * Disables a window.
 *
 * This informs the window provider that the window is no longer needed.
 *
 * \note
 * The window may be re-enabled later by a call to vlc_window_Enable().
 *
 * \param window window to disable
 */
VLC_API
void vlc_window_Disable(vlc_window_t *window);

/**
 * \defgroup video_window_reporting Window event reporting
 * Window provider event reporting
 *
 * The Window provider has to report some events to the core
 * so that it can react appropriately to these events, for
 * this the window provider calls the functions in this section
 * when appropriate.
 *
 * \note These functions may only be called by the window provider
 *       implementation.
 * @{
 */

/**
 * Reports the current window size.
 *
 * This function is called by the window implementation and notifies the owner
 * of the window what the pixel dimensions of the window are (or should be,
 * depending on the windowing system).
 *
 * \note This function is thread-safe. In case of concurrent call, it is
 * undefined which one is taken into account (but at least one is).
 *
 * \param window window implementation that reports the event
 * \param width width of the usable window area in pixels
 * \param height height of the usable window area in pixels
 */
static inline void vlc_window_ReportSize(vlc_window_t *window,
                                          unsigned width, unsigned height)
{
    window->owner.cbs->resized(window, width, height, NULL, NULL);
}

/**
 * Reports a request to close the window.
 *
 * This function is called by the window implementation to advise that the
 * window is being closed externally, and should be disabled by the owner.
 *
 * \param window window implementation that reports the event
 */
static inline void vlc_window_ReportClose(vlc_window_t *window)
{
    if (window->owner.cbs->closed != NULL)
        window->owner.cbs->closed(window);
}

/**
 * Reports the current window state.
 *
 * This function is called by the window implementation to notify the owner of
 * the window that the state of the window changed.
 *
 * \param window the window reporting the state change
 * \param state \see vlc_window_state
 */
static inline void vlc_window_ReportState(vlc_window_t *window,
                                           unsigned state)
{
    if (window->owner.cbs->state_changed != NULL)
        window->owner.cbs->state_changed(window, state);
}

/**
 * Reports that the window is not in full screen.
 *
 * This notifies the owner of the window that the window is windowed, i.e. not
 * in full screen mode.
 *
 * \param wnd window implementation that reports the event
 */
VLC_API void vlc_window_ReportWindowed(vlc_window_t *wnd);

/**
 * Reports that the window is in full screen.
 *
 * \param wnd the window reporting the fullscreen state
 * \param id fullscreen output nul-terminated identifier, NULL for default
 */
VLC_API void vlc_window_ReportFullscreen(vlc_window_t *wnd, const char *id);

static inline void vlc_window_SendMouseEvent(vlc_window_t *window,
                                              const vlc_window_mouse_event_t *mouse)
{
    if (window->owner.cbs->mouse_event != NULL)
        window->owner.cbs->mouse_event(window, mouse);
}

/**
 * Reports a pointer movement.
 *
 * The mouse position must be expressed in window pixel units.
 * See also \ref vlc_window_mouse_event_t.
 *
 * \param window window in focus
 * \param x abscissa
 * \param y ordinate
 */
static inline void vlc_window_ReportMouseMoved(vlc_window_t *window,
                                                int x, int y)
{
    const vlc_window_mouse_event_t mouse = {
        VLC_WINDOW_MOUSE_MOVED, x, y, 0
    };
    vlc_window_SendMouseEvent(window, &mouse);
}

/**
 * Reports a mouse button press.
 *
 * \param window window in focus
 * \param button pressed button (see \ref vlc_mouse_button)
 */
static inline void vlc_window_ReportMousePressed(vlc_window_t *window,
                                                  int button)
{
    const vlc_window_mouse_event_t mouse = {
        VLC_WINDOW_MOUSE_PRESSED, 0, 0, button,
    };
    vlc_window_SendMouseEvent(window, &mouse);
}

/**
 * Reports a mouse button release.
 *
 * \param window window in focus
 * \param button released button (see \ref vlc_mouse_button)
 */
static inline void vlc_window_ReportMouseReleased(vlc_window_t *window,
                                                   int button)
{
    const vlc_window_mouse_event_t mouse = {
        VLC_WINDOW_MOUSE_RELEASED, 0, 0, button,
    };
    vlc_window_SendMouseEvent(window, &mouse);
}

/**
 * Reports a mouse double-click.
 *
 * \param window window in focus
 * \param button double-clicked button (see \ref vlc_mouse_button)
 */
static inline void vlc_window_ReportMouseDoubleClick(vlc_window_t *window,
                                                      int button)
{
    const vlc_window_mouse_event_t mouse = {
        VLC_WINDOW_MOUSE_DOUBLE_CLICK, 0, 0, button,
    };
    vlc_window_SendMouseEvent(window, &mouse);
}

/**
 * Reports a keyboard key press.
 *
 * \param window window in focus
 * \param key VLC key code
 */
static inline void vlc_window_ReportKeyPress(vlc_window_t *window, int key)
{
    if (window->owner.cbs->keyboard_event != NULL)
        window->owner.cbs->keyboard_event(window, key);
}

/**
 * Adds/removes a fullscreen output.
 *
 * This notifies the owner of the window that a usable fullscreen output has
 * been added, changed or removed.
 *
 * If an output with the same identifier is already known, its name will be
 * updated. Otherwise it will be added.
 * If the name parameter is NULL, the output will be removed.
 *
 * \param window the window reporting the output device
 * \param id unique nul-terminated identifier for the output
 * \param name human-readable name
 */
static inline void vlc_window_ReportOutputDevice(vlc_window_t *window,
                                                  const char *id,
                                                  const char *name)
{
    if (window->owner.cbs->output_event != NULL)
        window->owner.cbs->output_event(window, id, name);
}

/**
 * Reports a change to the currently active ICC profile.
 *
 * \param window the window reporting the ICC profile
 * \param prof the profile data, or NULL. Ownership transfers to callee
 */
static inline void vlc_window_ReportICCProfile(vlc_window_t *window,
                                               vlc_icc_profile_t *prof)
{
    if (window->owner.cbs->icc_event != NULL) {
        window->owner.cbs->icc_event(window, prof);
    } else {
        free(prof);
    }
}

/** @} */
/** @} */
#endif /* VLC_WINDOW_H */
