/*****************************************************************************
 * deinterlace.h : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Christophe Massiot <massiot@via.ecp.fr>
 *         Laurent Aimar <fenrir@videolan.org>
 *         Juha Jeronen <juha.jeronen@jyu.fi>
 *         ...and others
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

#ifndef VLC_DEINTERLACE_H
#define VLC_DEINTERLACE_H 1

/* Forward declarations */
struct filter_t;
struct picture_t;
struct vlc_object_t;

#include <vlc_common.h>
#include <vlc_mouse.h>

/* Local algorithm headers */
#include "algo_basic.h"
#include "algo_x.h"
#include "algo_yadif.h"
#include "algo_phosphor.h"
#include "algo_ivtc.h"

/*****************************************************************************
 * Local data
 *****************************************************************************/

/** Available deinterlace modes. */
static const char *const mode_list[] = {
    "discard", "blend", "mean", "bob", "linear", "x",
    "yadif", "yadif2x", "phosphor", "ivtc" };

/** User labels for the available deinterlace modes. */
static const char *const mode_list_text[] = {
    N_("Discard"), N_("Blend"), N_("Mean"), N_("Bob"), N_("Linear"), "X",
    "Yadif", "Yadif (2x)", N_("Phosphor"), N_("Film NTSC (IVTC)") };

/*****************************************************************************
 * Data structures
 *****************************************************************************/

/**
 * Available deinterlace algorithms.
 * @see SetFilterMethod()
 */
typedef enum { DEINTERLACE_DISCARD, DEINTERLACE_MEAN,    DEINTERLACE_BLEND,
               DEINTERLACE_BOB,     DEINTERLACE_LINEAR,  DEINTERLACE_X,
               DEINTERLACE_YADIF,   DEINTERLACE_YADIF2X, DEINTERLACE_PHOSPHOR,
               DEINTERLACE_IVTC } deinterlace_mode;

#define METADATA_SIZE (3)
/**
 * Metadata history structure, used for framerate doublers.
 * This is used for computing field duration in Deinterlace().
 * @see Deinterlace()
 */
typedef struct {
    mtime_t pi_date[METADATA_SIZE];
    int     pi_nb_fields[METADATA_SIZE];
    bool    pb_top_field_first[METADATA_SIZE];
} metadata_history_t;

#define HISTORY_SIZE (3)
#define CUSTOM_PTS -1
/**
 * Top-level deinterlace subsystem state.
 */
struct filter_sys_t
{
    const vlc_chroma_description_t *chroma;

    uint8_t  i_mode;              /**< Deinterlace mode */

    /* Algorithm behaviour flags */
    bool b_double_rate;       /**< Shall we double the framerate? */
    bool b_half_height;       /**< Shall be divide the height by 2 */
    bool b_use_frame_history; /**< Use the input frame history buffer? */

    /** Merge routine: C, MMX, SSE, ALTIVEC, NEON, ... */
    void (*pf_merge) ( void *, const void *, const void *, size_t );
#if defined (__i386__) || defined (__x86_64__)
    /** Merge finalization routine for SSE */
    void (*pf_end_merge) ( void );
#endif

    /**
     * Metadata history (PTS, nb_fields, TFF). Used for framerate doublers.
     * @see metadata_history_t
     */
    metadata_history_t meta;

    /** Output frame timing / framerate doubler control
        (see extra documentation in deinterlace.h) */
    int i_frame_offset;

    /** Input frame history buffer for algorithms with temporal filtering. */
    picture_t *pp_history[HISTORY_SIZE];

    /* Algorithm-specific substructures */
    phosphor_sys_t phosphor; /**< Phosphor algorithm state. */
    ivtc_sys_t ivtc;         /**< IVTC algorithm state. */
};

/*****************************************************************************
 * video filter2 functions
 *****************************************************************************/

/**
 * Top-level filtering method.
 *
 * Open() sets this up as the processing method (pf_video_filter)
 * in the filter structure.
 *
 * Note that there is no guarantee that the returned picture directly
 * corresponds to p_pic. The first few times, the filter may not even
 * return a picture, if it is still filling the history for temporal
 * filtering (although such filters often return *something* also
 * while starting up). It should be assumed that N input pictures map to
 * M output pictures, with no restrictions for N and M (except that there
 * is not much delay).
 *
 * Also, there is no guarantee that the PTS of the frame stays untouched.
 * In fact, framerate doublers automatically compute the proper PTSs for the
 * two output frames for each input frame, and IVTC does a nontrivial
 * framerate conversion (29.97 > 23.976 fps).
 *
 * Yadif has an offset of one frame between input and output, but introduces
 * no delay: the returned frame is the *previous* input frame deinterlaced,
 * complete with its original PTS.
 *
 * Finally, note that returning NULL sometimes can be normal behaviour for some
 * algorithms (e.g. IVTC).
 *
 * Currently:
 *   Most algorithms:        1 -> 1, no offset
 *   All framerate doublers: 1 -> 2, no offset
 *   Yadif:                  1 -> 1, offset of one frame
 *   IVTC:                   1 -> 1 or 0 (depends on whether a drop was needed)
 *                                with an offset of one frame (in most cases)
 *                                and framerate conversion.
 *
 * @param p_filter The filter instance.
 * @param p_pic The latest input picture.
 * @return Deinterlaced picture(s). Linked list of picture_t's or NULL.
 * @see Open()
 * @see filter_t
 * @see filter_sys_t
 */
picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic );

/**
 * Reads the configuration, sets up and starts the filter.
 *
 * Possible reasons for returning VLC_EGENERIC:
 *  - Unsupported input chroma. See IsChromaSupported().
 *  - Caller has set p_filter->b_allow_fmt_out_change to false,
 *    but the algorithm chosen in the configuration
 *    wants to convert the output to a format different
 *    from the input. See SetFilterMethod().
 *
 * Open() is atomic: if an error occurs, the state of p_this
 * is left as it was before the call to this function.
 *
 * @param p_this The filter instance as vlc_object_t.
 * @return VLC error code
 * @retval VLC_SUCCESS All ok, filter set up and started.
 * @retval VLC_ENOMEM Memory allocation error, initialization aborted.
 * @retval VLC_EGENERIC Something went wrong, initialization aborted.
 * @see IsChromaSupported()
 * @see SetFilterMethod()
 */
int Open( vlc_object_t *p_this );

/**
 * Resets the filter state, including resetting all algorithm-specific state
 * and discarding all histories, but does not stop the filter.
 *
 * Open() sets this up as the flush method (pf_video_flush)
 * in the filter structure.
 *
 * @param p_filter The filter instance.
 * @see Open()
 * @see filter_t
 * @see filter_sys_t
 * @see metadata_history_t
 * @see phosphor_sys_t
 * @see ivtc_sys_t
 */
void Flush( filter_t *p_filter );

/**
 * Mouse callback for the deinterlace filter.
 *
 * Open() sets this up as the mouse callback method (pf_video_mouse)
 * in the filter structure.
 *
 * Currently, this handles the scaling of the y coordinate for algorithms
 * that halve the output height.
 *
 * @param p_filter The filter instance.
 * @param[out] p_mouse Updated mouse position data.
 * @param[in] p_old Previous mouse position data. Unused in this filter.
 * @param[in] p_new Latest mouse position data.
 * @return VLC error code; currently always VLC_SUCCESS.
 * @retval VLC_SUCCESS All ok.
 * @see Open()
 * @see filter_t
 * @see vlc_mouse_t
 */
int Mouse( filter_t *p_filter,
           vlc_mouse_t *p_mouse,
           const vlc_mouse_t *p_old,
           const vlc_mouse_t *p_new );

/**
 * Stops and uninitializes the filter, and deallocates memory.
 * @param p_this The filter instance as vlc_object_t.
 */
void Close( vlc_object_t *p_this );

/*****************************************************************************
 * Extra documentation
 *****************************************************************************/

/**
 * \file
 * Deinterlacer plugin for vlc. Data structures and video filter2 functions.
 *
 * Note on i_frame_offset:
 *
 * This value indicates the offset between input and output frames in the
 * currently active deinterlace algorithm. See the rationale below for why
 * this is needed and how it is used.
 *
 * Valid range: 0 <= i_frame_offset < METADATA_SIZE, or
 *              i_frame_offset = CUSTOM_PTS.
 *              The special value CUSTOM_PTS is only allowed
 *              if b_double_rate is false.
 *
 *              If CUSTOM_PTS is used, the algorithm must compute the outgoing
 *              PTSs itself, and additionally, read the TFF/BFF information
 *              itself (if it needs it) from the incoming frames.
 *
 * Meaning of values:
 * 0 = output frame corresponds to the current input frame
 *     (no frame offset; default if not set),
 * 1 = output frame corresponds to the previous input frame
 *     (e.g. Yadif and Yadif2x work like this),
 * ...
 *
 * If necessary, i_frame_offset should be updated by the active deinterlace
 * algorithm to indicate the correct delay for the *next* input frame.
 * It does not matter at which i_order the algorithm updates this information,
 * but the new value will only take effect upon the next call to Deinterlace()
 * (i.e. at the next incoming frame).
 *
 * The first-ever frame that arrives to the filter after Open() is always
 * handled as having i_frame_offset = 0. For the second and all subsequent
 * frames, each algorithm is responsible for setting the offset correctly.
 * (The default is 0, so if that is correct, there's no need to do anything.)
 *
 * This solution guarantees that i_frame_offset:
 *   1) is up to date at the start of each frame,
 *   2) does not change (as far as Deinterlace() is concerned) during
 *      a frame, and
 *   3) does not need a special API for setting the value at the start of each
 *      input frame, before the algorithm starts rendering the (first) output
 *      frame for that input frame.
 *
 * The deinterlace algorithm is allowed to behave differently for different
 * input frames. This is especially important for startup, when full history
 * (as defined by each algorithm) is not yet available. During the first-ever
 * input frame, it is clear that it is the only possible source for
 * information, so i_frame_offset = 0 is necessarily correct. After that,
 * what to do is up to each algorithm.
 *
 * Having the correct offset at the start of each input frame is critically
 * important in order to:
 *   1) Allocate the correct number of output frames for framerate doublers,
 *      and to
 *   2) Pass correct TFF/BFF information to the algorithm.
 *
 * These points are important for proper soft field repeat support. This
 * feature is used in some streams (especially NTSC) originating from film.
 * For example, in soft NTSC telecine, the number of fields alternates
 * as 3,2,3,2,... and the video field dominance flips every two frames (after
 * every "3"). Also, some streams request an occasional field repeat
 * (nb_fields = 3), after which the video field dominance flips.
 * To render such streams correctly, the nb_fields and TFF/BFF information
 * must be taken from the specific input frame that the algorithm intends
 * to render.
 *
 * Additionally, the output PTS is automatically computed by Deinterlace()
 * from i_frame_offset and i_order.
 *
 * It is possible to use the special value CUSTOM_PTS to indicate that the
 * algorithm computes the output PTSs itself. In this case, Deinterlace()
 * will pass them through. This special value is not valid for framerate
 * doublers, as by definition they are field renderers, so they need to
 * use the original field timings to work correctly. Basically, this special
 * value is only intended for algorithms that need to perform nontrivial
 * framerate conversions (such as IVTC).
 */

#endif
