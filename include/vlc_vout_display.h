/*****************************************************************************
 * vlc_vout_display.h: vout_display_t definitions
 *****************************************************************************
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

#ifndef VLC_VOUT_DISPLAY_H
#define VLC_VOUT_DISPLAY_H 1

#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>
#include <vlc_subpicture.h>
#include <vlc_actions.h>
#include <vlc_mouse.h>
#include <vlc_vout.h>
#include <vlc_vout_window.h>
#include <vlc_viewpoint.h>

/**
 * \defgroup video_display Video output display
 * Video output display: output buffers and rendering
 *
 * \ingroup video_output
 * @{
 * \file
 * Video output display modules interface
 */

typedef struct vout_display_t vout_display_t;
typedef struct vout_display_sys_t vout_display_sys_t;
typedef struct vout_display_owner_t vout_display_owner_t;

/**
 * Possible alignments for vout_display.
 */
#define VLC_VIDEO_ALIGN_CENTER 0
#define VLC_VIDEO_ALIGN_LEFT   1
#define VLC_VIDEO_ALIGN_RIGHT  2
#define VLC_VIDEO_ALIGN_TOP    1
#define VLC_VIDEO_ALIGN_BOTTOM 2

typedef struct vlc_video_align {
    char horizontal;
    char vertical;
} vlc_video_align_t;

/**
 * Initial/Current configuration for a vout_display_t
 */
typedef struct {
    struct vout_window_t *window; /**< Window */
#if defined(_WIN32) || defined(__OS2__)
    bool is_fullscreen VLC_DEPRECATED;  /* Is the display fullscreen */
#endif

    /* Display properties */
    struct {
        /* Display size */
        unsigned  width;
        unsigned  height;

        /* Display SAR */
        vlc_rational_t sar;
    } display;

    /* Alignment of the picture inside the display */
    vlc_video_align_t align;

    /* Do we fill up the display with the video */
    bool is_display_filled;

    /* Zoom to use
     * It will be applied to the whole display if b_display_filled is set, otherwise
     * only on the video source */
    struct {
        unsigned num;
        unsigned den;
    } zoom;

    vlc_viewpoint_t viewpoint;
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
    bool has_pictures_invalid;              /* Can handle VOUT_DISPLAY_RESET_PICTURES */
    bool can_scale_spu;                     /* Handles subpictures with a non default zoom factor */
    const vlc_fourcc_t *subpicture_chromas; /* List of supported chromas for subpicture rendering. */
} vout_display_info_t;

/**
 * Control query for vout_display_t
 */
enum {
    /* Ask to reset the internal buffers after a
     * \ref VOUT_DISPLAY_CHANGE_DISPLAY_SIZE,
     * \ref VOUT_DISPLAY_CHANGE_DISPLAY_FILLED,
     * \ref VOUT_DISPLAY_CHANGE_ZOOM,
     * \ref VOUT_DISPLAY_CHANGE_SOURCE_ASPECT or
     * \ref VOUT_DISPLAY_CHANGE_SOURCE_CROP
     * control query returns an error.
     */
    VOUT_DISPLAY_RESET_PICTURES, /* const vout_display_cfg_t *, es_format_t * */

#if defined(_WIN32) || defined(__OS2__)
    /* Ask the module to acknowledge/refuse the fullscreen state change after
     * being requested (externally or by VOUT_DISPLAY_EVENT_FULLSCREEN */
    VOUT_DISPLAY_CHANGE_FULLSCREEN VLC_DEPRECATED_ENUM,     /* bool fs */
    /* Ask the module to acknowledge/refuse the window management state change
     * after being requested externally or by VOUT_DISPLAY_WINDOW_STATE */
    VOUT_DISPLAY_CHANGE_WINDOW_STATE VLC_DEPRECATED_ENUM,   /* unsigned state */
#endif
    /* Ask the module to acknowledge the display size change */
    VOUT_DISPLAY_CHANGE_DISPLAY_SIZE,   /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse fill display state change after
     * being requested externally */
    VOUT_DISPLAY_CHANGE_DISPLAY_FILLED, /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse zoom change after being requested
     * externally */
    VOUT_DISPLAY_CHANGE_ZOOM, /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse source aspect ratio after being
     * requested externally */
    VOUT_DISPLAY_CHANGE_SOURCE_ASPECT, /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse source crop change after being
     * requested externally.
     * The cropping requested is stored by source video_format_t::i_x/y_offset
     * and video_format_t::i_visible_width/height */
    VOUT_DISPLAY_CHANGE_SOURCE_CROP, /* const vout_display_cfg_t *p_cfg */

    /* Ask the module to acknowledge/refuse VR/360° viewing direction after
     * being requested externally */
    VOUT_DISPLAY_CHANGE_VIEWPOINT,   /* const vout_display_cfg_t *p_cfg */
};

/**
 * Event from vout_display_t
 *
 * Events modifiying the state may be sent multiple times.
 * Only the transition will be retained and acted upon.
 */
enum {
    /* VR navigation */
    VOUT_DISPLAY_EVENT_VIEWPOINT_MOVED,
};

/**
 * Vout owner structures
 */
struct vout_display_owner_t {
    /* Private place holder for the vout_display_t creator
     */
    void *sys;

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
};

/**
 * "vout display" open callback
 *
 * @param vd vout display context
 * @param cfg Initial and current configuration.
 * @param fmtp By default, it is equal to vd->source except for the aspect
 * ratio which is undefined(0) and is ignored. It can be changed by the module
 * to request a different format.
 * @param context XXX: to be defined.
 * @return VLC_SUCCESS or a VLC error code
 */
typedef int (*vout_display_open_cb)(vout_display_t *vd,
                                    const vout_display_cfg_t *cfg,
                                    video_format_t *fmtp,
                                    vlc_video_context *context);

/**
 * "vout display" close callback
 *
 * @param vd vout display context
 */
typedef int (*vout_display_close_cb)(vout_display_t *vd);

struct vout_display_t {
    struct vlc_common_members obj;

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
    void       (*prepare)(vout_display_t *, picture_t *, subpicture_t *,
                          vlc_tick_t date);

    /* Display a picture.
     *
     * The picture must be displayed as soon as possible.
     * You cannot change the pixel content of the picture_t.
     */
    void       (*display)(vout_display_t *, picture_t *);

    /* Control on the module (mandatory) */
    int        (*control)(vout_display_t *, int, va_list);

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

/**
 * Creates video output display.
 */
VLC_API
vout_display_t *vout_display_New(vlc_object_t *, const video_format_t *,
    const vout_display_cfg_t *, const char *module,
    const vout_display_owner_t *);

/**
 * Destroys a video output display.
 */
VLC_API void vout_display_Delete(vout_display_t *);

/**
 * Prepares a picture for display.
 *
 * This renders a picture for subsequent display, with vout_display_Display().
 *
 * \note A reference to the input picture is consumed by the function, which
 * returns a reference to an output picture for display. The input and output
 * picture may or may not be equal depending on the underlying display setup.
 *
 * \bug Currently, only one picture can be prepared at a time. It must be
 * displayed with vout_display_Display() before any picture is prepared or
 * before the display is destroyd with vout_display_Delete().
 *
 \ bug Rendering subpictures is not supported with this function yet.
 * \c subpic must be @c NULL .
 *
 * \param vd display to prepare the picture for
 * \param picture picure to be prepared
 * \param subpic reserved, must be NULL
 * \param date intended time to show the picture
 * \return The prepared picture is returned, NULL on error.
 */
VLC_API picture_t *vout_display_Prepare(vout_display_t *vd, picture_t *picture,
                                        subpicture_t *subpic, vlc_tick_t date);

/**
 * Displays a picture.
 */
static inline void vout_display_Display(vout_display_t *vd, picture_t *picture)
{
    if (vd->display != NULL)
        vd->display(vd, picture);
    picture_Release(picture);
}

VLC_API
void vout_display_SetSize(vout_display_t *vd, unsigned width, unsigned height);

static inline int vout_display_Control(vout_display_t *vd, int query, ...)
{
    va_list ap;
    int ret;

    va_start(ap, query);
    ret = vd->control(vd, query, ap);
    va_end(ap);
    return ret;
}

static inline void vout_display_SendEvent(vout_display_t *vd, int query, ...)
{
    va_list args;
    va_start(args, query);
    vd->owner.event(vd, query, args);
    va_end(args);
}

VLC_API void vout_display_SendEventPicturesInvalid(vout_display_t *vd);

#if defined(_WIN32) || defined(__OS2__)
VLC_DEPRECATED
static inline void vout_display_SendEventFullscreen(vout_display_t *vd, bool is_fullscreen)
{
    if (vout_display_Control(vd, VOUT_DISPLAY_CHANGE_FULLSCREEN,
                             is_fullscreen) == VLC_SUCCESS)
        ((vout_display_cfg_t *)vd->cfg)->is_fullscreen = is_fullscreen;
}

VLC_DEPRECATED
static inline void vout_display_SendWindowState(vout_display_t *vd, unsigned state)
{
    vout_display_Control(vd, VOUT_DISPLAY_CHANGE_WINDOW_STATE, state);
}
#endif
static inline void vout_display_SendEventMousePressed(vout_display_t *vd, int button)
{
    vout_window_ReportMousePressed(vd->cfg->window, button);
}
static inline void vout_display_SendEventMouseReleased(vout_display_t *vd, int button)
{
    vout_window_ReportMouseReleased(vd->cfg->window, button);
}
static inline void vout_display_SendEventMouseDoubleClick(vout_display_t *vd)
{
    vout_window_ReportMouseDoubleClick(vd->cfg->window, MOUSE_BUTTON_LEFT);
}
static inline void vout_display_SendEventViewpointMoved(vout_display_t *vd,
                                                        const vlc_viewpoint_t *vp)
{
    vout_display_SendEvent(vd, VOUT_DISPLAY_EVENT_VIEWPOINT_MOVED, vp);
}

static inline bool vout_display_cfg_IsWindowed(const vout_display_cfg_t *cfg)
{
    return cfg->window->type != VOUT_WINDOW_TYPE_DUMMY;
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
 */
VLC_API void vout_display_PlacePicture(vout_display_place_t *place, const video_format_t *source, const vout_display_cfg_t *cfg);

/**
 * Translates mouse state.
 *
 * This translates the mouse (pointer) state from window coordinates to
 * video coordinates.
 * @note @c video and @c window pointers may alias.
 */
void vout_display_TranslateMouseState(vout_display_t *vd, vlc_mouse_t *video,
                                      const vlc_mouse_t *window);

/**
 * Helper function that applies the necessary transforms to the mouse position
 * and then calls vout_display_SendEventMouseMoved.
 *
 * \param vd vout_display_t.
 * \param m_x Mouse x position (relative to place, origin is top left).
 * \param m_y Mouse y position (relative to place, origin is top left).
 */
VLC_API void vout_display_SendMouseMovedDisplayCoordinates(vout_display_t *vd, int m_x, int m_y);

/** @} */
#endif /* VLC_VOUT_DISPLAY_H */
