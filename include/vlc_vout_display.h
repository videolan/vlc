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
#include <vlc_subpicture.h>
#include <vlc_actions.h>
#include <vlc_mouse.h>
#include <vlc_vout.h>
#include <vlc_window.h>
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
typedef struct vout_display_owner_t vout_display_owner_t;

/**
 * \defgroup video_align Video alignment
 * @{
 */
#define VLC_VIDEO_ALIGN_CENTER 0
#define VLC_VIDEO_ALIGN_LEFT   1
#define VLC_VIDEO_ALIGN_RIGHT  2
#define VLC_VIDEO_ALIGN_TOP    1
#define VLC_VIDEO_ALIGN_BOTTOM 2

/**
 * Video alignment within the display.
 */
typedef struct vlc_video_align {
    /**
     * Horizontal alignment.
     *
     * This must be one of \ref VLC_VIDEO_ALIGN_CENTER,
     * \ref VLC_VIDEO_ALIGN_LEFT or \ref VLC_VIDEO_ALIGN_RIGHT.
     */
    char horizontal;

    /**
     * Vectical alignment.
     *
     * This must be one of \ref VLC_VIDEO_ALIGN_CENTER,
     * \ref VLC_VIDEO_ALIGN_TOP or \ref VLC_VIDEO_ALIGN_BOTTOM.
     */
    char vertical;
} vlc_video_align_t;
/** @} */

/**
 * Video automatic scale fitting.
 */
enum vlc_video_fitting {
    VLC_VIDEO_FIT_NONE /**< No automatic scaling (use explicit zoom ratio) */,
    VLC_VIDEO_FIT_SMALLER /**< Fit inside / to smallest dimension */,
    VLC_VIDEO_FIT_LARGER /**< Fit outside / to largest dimension */,
    VLC_VIDEO_FIT_WIDTH /**< Fit to width */,
    VLC_VIDEO_FIT_HEIGHT /**< Fit to height */,
};

/**
 * Display placement and zoom configuration.
 */
struct vout_display_placement {
    unsigned width; /**< Requested display pixel width (0 by default). */
    unsigned height; /**< Requested display pixel height (0 by default). */
    vlc_rational_t sar; /**< Requested sample aspect ratio */

    vlc_video_align_t align; /**< Alignment within the window */
    enum vlc_video_fitting fitting; /**< Scaling/fitting mode */
    vlc_rational_t zoom; /**< Zoom ratio (if fitting is disabled) */
};

/**
 * User configuration for a video output display (\ref vout_display_t)
 *
 * This primarily controls the size of the display area within the video
 * window, as follows:
 * - If \ref vout_display_cfg::display::fitting is not disabled,
 *   the video size is fitted to the display size.
 * - If \ref vout_display_cfg::window "window" size is valid, the video size
 *   is set to the window size,
 * - Otherwise, the video size is determined from the original video format,
 *   multiplied by the zoom factor.
 */
typedef struct vout_display_cfg {
    struct vlc_window *window; /**< Window */
    struct vout_display_placement display; /**< Display placement properties */
    vlc_icc_profile_t *icc_profile; /**< Currently active ICC profile */
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
    bool can_scale_spu;                     /* Handles subpictures with a non default zoom factor */
    const vlc_fourcc_t *subpicture_chromas; /* List of supported chromas for subpicture rendering. */
} vout_display_info_t;

/**
 * Control query for vout_display_t
 */
enum vout_display_query {
    /**
     * Notifies a change in display size.
     *
     * \retval VLC_SUCCESS if the display handled the change
     * \retval VLC_EGENERIC if a \ref vlc_display_operations::reset_pictures
     *         request is necessary
     */
    VOUT_DISPLAY_CHANGE_DISPLAY_SIZE,

    /**
     * Notifies a change of the display fitting mode by the user.
     *
     * \retval VLC_SUCCESS if the display handled the change
     * \retval VLC_EGENERIC if a \ref vlc_display_operations::reset_pictures
     *         request is necessary
     */
    VOUT_DISPLAY_CHANGE_DISPLAY_FILLED,

    /**
     * Notifies a change of the user zoom factor.
     *
     * \retval VLC_SUCCESS if the display handled the change
     * \retval VLC_EGENERIC if a \ref vlc_display_operations::reset_pictures
     *         request is necessary
     */
    VOUT_DISPLAY_CHANGE_ZOOM,

    /**
     * Notifies a change of the sample aspect ratio.
     *
     * \retval VLC_SUCCESS if the display handled the change
     * \retval VLC_EGENERIC if a \ref vlc_display_operations::reset_pictures
     *         request is necessary
     */
    VOUT_DISPLAY_CHANGE_SOURCE_ASPECT,

    /**
     * Notifies a change of the source cropping.
     *
     * The cropping requested is stored by source \ref video_format_t `i_x`/`y_offset`
     * and `i_visible_width`/`height`
     *
     * \retval VLC_SUCCESS if the display handled the change
     * \retval VLC_EGENERIC if a \ref vlc_display_operations::reset_pictures
     *         request is necessary
     */
    VOUT_DISPLAY_CHANGE_SOURCE_CROP,
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
    void (*viewpoint_moved)(void *sys, const vlc_viewpoint_t *vp);
};

/**
 * "vout display" open callback
 *
 * @param vd vout display context
 * @param fmtp It can be changed by the module to request a different format.
 * @param context The video context to configure the display for.
 * @return VLC_SUCCESS or a VLC error code
 */
typedef int (*vout_display_open_cb)(vout_display_t *vd,
                                    video_format_t *fmtp,
                                    vlc_video_context *context);

#define set_callback_display(activate, priority) \
    { \
        vout_display_open_cb open__ = activate; \
        (void) open__; \
        set_callback(activate) \
    } \
    set_capability( "vout display", priority )

struct vlc_display_operations
{
    /**
     * Destroys the display.
     */
    void       (*close)(vout_display_t *);

    /**
     * Prepares a picture and an optional subpicture for display (optional).
     *
     * This callback is called once a picture buffer content is ready,
     * as far in advance as possible to the intended display time,
     * but only after the previous picture was displayed.
     *
     * The callback should perform any preprocessing operation that will not
     * actually cause the picture to be shown, such as blending the subpicture
     * or upload the picture to video memory. If supported, this can also
     * queue the picture to be shown asynchronously at the given date.
     *
     *
     * If \ref vlc_display_operations.prepare and
     * \ref vlc_display_operations.display are not \c NULL, there is an
     * implicit guarantee that display will be invoked with the exact same
     * picture afterwards:
     * prepare 1st picture, display 1st picture, prepare 2nd picture, display
     * 2nd picture, and so on.
     *
     * \note The picture buffers may have multiple references.
     * Therefore the pixel content of the picture or of the subpicture
     * must not be changed.
     *
     * \param pic picture
     * \param subpic subpicture to render over the picture
     * \param date time when the picture is intended to be shown
     */
    void       (*prepare)(vout_display_t *, picture_t *pic,
                          subpicture_t *subpic, vlc_tick_t date);

    /**
     * Displays a picture.
     *
     * This callback is invoked at the time when the picture should be shown.
     * The picture must be displayed as soon as possible.
     *
     * If NULL, prepare must be valid. In that case, the plugin can handle
     * asynchronous display at the time given by the prepare call.
     *
     * \note The picture buffers may have multiple references.
     * Therefore the pixel content of the picture or of the subpicture
     * must not be changed.
     */
    void       (*display)(vout_display_t *, picture_t *pic);

    /**
     * Performs a control request (mandatory).
     *
     * \param query request type
     *
     * See \ref vout_display_query for the list of request types.
     */
    int        (*control)(vout_display_t *, int query);

    /**
     * Reset the picture format handled by the module.
     * This occurs after a
     * \ref VOUT_DISPLAY_CHANGE_DISPLAY_SIZE,
     * \ref VOUT_DISPLAY_CHANGE_DISPLAY_FILLED,
     * \ref VOUT_DISPLAY_CHANGE_ZOOM,
     * \ref VOUT_DISPLAY_CHANGE_SOURCE_ASPECT or
     * \ref VOUT_DISPLAY_CHANGE_SOURCE_CROP
     * control query returns an error.
     *
     * \param ftmp video format that the module expects as input
     */
    int       (*reset_pictures)(vout_display_t *, video_format_t *fmtp);

    /**
     * Notifies a change of VR/360° viewpoint.
     *
     * May be NULL.
     *
     * \param vp viewpoint to use on the next render
     */
    int        (*set_viewpoint)(vout_display_t *, const vlc_viewpoint_t *vp);

    /**
     * Notifies a change in output ICC profile.
     *
     * May be NULL. Memory owned by the caller.
     *
     * \param prof new ICC profile associated with display, or NULL for none
     */
    void       (*set_icc_profile)(vout_display_t *, const vlc_icc_profile_t *prof);

    /**
     * Notifies a change in the input format.
     *
     * The format size is not expected to change.
     *
     * \param fmt the requested input format
     * \param ctx the video context
     * \return VLC_SUCCESS on success, another value on error
     */
    int (*update_format)(vout_display_t *, const video_format_t *fmt,
                         vlc_video_context *ctx);
};

struct vout_display_t {
    struct vlc_object_t obj;

    /**
     * User configuration.
     *
     * This cannot be modified directly. It reflects the current values.
     */
    const vout_display_cfg_t *cfg;

    /**
     * Source video format.
     *
     * This is the format of the video that is being displayed (after decoding
     * and filtering). It cannot be modified.
     *
     * \note
     * Cropping is not requested while in the open function.
     */
    const video_format_t *source;

    /**
     * Picture format.
     *
     * This is the format of the pictures that are supplied to the
     * \ref vlc_display_operations::prepare "prepare" and
     * \ref vlc_display_operations::display "display" callbacks.
     * Ideally, it should be identical or as close as possible as \ref source.
     *
     * This can only be changed from the display module activation callback,
     * or within a \ref vlc_display_operations::reset_pictures "reset_pictures"
     * request.
     *
     * By default, it is equal to \ref source except for the aspect ratio
     * which is undefined(0) and is ignored.
     */
    const video_format_t *fmt;

    /* Information
     *
     * You can only set them in the open function.
     */
    vout_display_info_t info;

    /* Reserved for the vout_display_t owner.
     *
     * It must not be overwritten nor used directly by a module.
     */
    vout_display_owner_t owner;

    /**
     * Private data for the display module.
     *
     * A module is free to use it as it wishes.
     */
    void *sys;

    /**
     * Callbacks the display module must set on Open.
     */
    const struct vlc_display_operations *ops;
};

/**
 * Creates video output display.
 */
VLC_API
vout_display_t *vout_display_New(vlc_object_t *,
    const video_format_t *, vlc_video_context *,
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
    if (vd->ops->display != NULL)
        vd->ops->display(vd, picture);
}

VLC_API
void vout_display_SetSize(vout_display_t *vd, unsigned width, unsigned height);

static inline void vout_display_SendEventMousePressed(vout_display_t *vd, int button)
{
    vlc_window_ReportMousePressed(vd->cfg->window, button);
}
static inline void vout_display_SendEventMouseReleased(vout_display_t *vd, int button)
{
    vlc_window_ReportMouseReleased(vd->cfg->window, button);
}
static inline void vout_display_SendEventViewpointMoved(vout_display_t *vd,
                                                        const vlc_viewpoint_t *vp)
{
    if (vd->owner.viewpoint_moved)
        vd->owner.viewpoint_moved(vd->owner.sys, vp);
}

/**
 * Helper function that applies the necessary transforms to the mouse position
 * and then calls vout_display_SendEventMouseMoved.
 *
 * \param vd vout_display_t.
 * \param m_x Mouse x position (relative to place, origin is top left).
 * \param m_y Mouse y position (relative to place, origin is top left).
 */
static inline void vout_display_SendMouseMovedDisplayCoordinates(vout_display_t *vd, int m_x, int m_y)
{
    vlc_window_ReportMouseMoved(vd->cfg->window, m_x, m_y);
}

static inline bool vout_display_cfg_IsWindowed(const vout_display_cfg_t *cfg)
{
    return cfg->window->type != VLC_WINDOW_TYPE_DUMMY;
}

/**
 * Computes the default display size given the source and
 * the display configuration.
 *
 * This assumes that the picture is already cropped.
 */
VLC_API
void vout_display_GetDefaultDisplaySize(unsigned *width, unsigned *height,
                                        const video_format_t *source,
                                        const struct vout_display_placement *);

/**
 * Video placement.
 *
 * This structure stores the result of a vout_display_PlacePicture() call.
 */
typedef struct {
    int x; /*< Relative pixel offset from the display left edge */
    int y; /*< Relative pixel offset from the display top edge */
    unsigned width; /*< Picture pixel width */
    unsigned height; /*< Picture pixel height */
} vout_display_place_t;

/**
 * Compares two \ref vout_display_place_t.
 */
static inline bool vout_display_PlaceEquals(const vout_display_place_t *p1,
                                            const vout_display_place_t *p2)
{
    return p1->x == p2->x && p1->width == p2->width &&
            p1->y == p2->y && p1->height == p2->height;
}

/**
 * Computes the intended picture placement inside the display.
 *
 * This function computes where to show a picture inside the display with
 * respect to the provided parameters, and returns the result
 * in a \ref vout_display_place_t structure.
 *
 * This assumes that cropping is done by an external mean.
 *
 * \param place Storage space for the picture placement [OUT]
 * \param source Video source format
 * \param cfg Display configuration
 */
VLC_API
void vout_display_PlacePicture(vout_display_place_t *restrict place,
                               const video_format_t *restrict source,
                               const struct vout_display_placement *cfg);

/**
 * Translates coordinates.
 *
 * This translates coordinates from window pixel coordinate space to
 * original video sample coordinate space.
 *
 * \param x pointer to abscissa to be translated
 * \param y pointer to ordinate to be translated
 * \param fmt video format
 * \param dp display configuration
 */
void vout_display_TranslateCoordinates(int *x, int *y,
                                       const video_format_t *fmt,
                                       const struct vout_display_placement *dp);

/** @} */
#endif /* VLC_VOUT_DISPLAY_H */
