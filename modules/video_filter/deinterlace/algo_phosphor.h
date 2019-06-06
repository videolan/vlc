/*****************************************************************************
 * algo_phosphor.h : Phosphor algorithm for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Author: Juha Jeronen <juha.jeronen@jyu.fi>
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

#ifndef VLC_DEINTERLACE_ALGO_PHOSPHOR_H
#define VLC_DEINTERLACE_ALGO_PHOSPHOR_H 1

/* Forward declarations */
struct filter_t;
struct picture_t;

/*****************************************************************************
 * Data structures etc.
 *****************************************************************************/

/* These numbers, and phosphor_chroma_list[], should be in the same order
   as phosphor_chroma_list_text[]. The value 0 is reserved, because
   var_GetInteger() returns 0 in case of error. */
/** Valid Phosphor 4:2:0 chroma handling modes. */
typedef enum { PC_LATEST = 1, PC_ALTLINE   = 2,
               PC_BLEND  = 3, PC_UPCONVERT = 4 } phosphor_chroma_t;
/** Phosphor 4:2:0 chroma handling modes (config item). */
static const int phosphor_chroma_list[] = { PC_LATEST, PC_ALTLINE,
                                            PC_BLEND,  PC_UPCONVERT };
/** User labels for Phosphor 4:2:0 chroma handling modes (config item). */
static const char *const phosphor_chroma_list_text[] = { N_("Latest"),
                                                         N_("AltLine"),
                                                         N_("Blend"),
                                                         N_("Upconvert") };

/* Same here. Same order as in phosphor_dimmer_list_text[],
   and the value 0 is reserved for config error. */
/** Phosphor dimmer strengths (config item). */
static const int phosphor_dimmer_list[] = { 1, 2, 3, 4 };
/** User labels for Phosphor dimmer strengths (config item). */
static const char *const phosphor_dimmer_list_text[] = { N_("Off"),
                                                         N_("Low"),
                                                         N_("Medium"),
                                                         N_("High") };

/** Algorithm-specific state for Phosphor. */
typedef struct
{
    phosphor_chroma_t i_chroma_for_420;
    int i_dimmer_strength;
} phosphor_sys_t;

/*****************************************************************************
 * Functions
 *****************************************************************************/

/**
 * "Phosphor" deinterlace algorithm: framerate-doubling CRT TV simulator.
 *
 * There is no "1x" mode in this filter; only framerate doubling is supported.
 *
 * There is no input frame parameter, because the input frames
 * are taken from the history buffer.
 *
 * Soft field repeat (repeat_pict) is supported. Note that the generated
 * "repeated" output picture is unique because of the simulated light decay.
 * Its "old" field comes from the same input frame as the "new" one, unlike
 * the first output picture of the same frame.
 *
 * As many output frames should be requested for each input frame as is
 * indicated by p_src->i_nb_fields. This is done by calling this function
 * several times, first with i_order = 0, and then with all other parameters
 * the same, but a new p_dst, increasing i_order (1 for second field,
 * and then if i_nb_fields = 3, also i_order = 2 to get the repeated first
 * field), and alternating i_field (starting, at i_order = 0, with the field
 * according to p_src->b_top_field_first). See Deinterlace() for an example.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param p_dst Output frame. Must be allocated by caller.
 * @param i_order Temporal field number: 0 = first, 1 = second, 2 = rep. first.
 * @param i_field Render which field? 0 = top field, 1 = bottom field.
 * @return VLC error code (int).
 * @retval VLC_SUCCESS The requested field was rendered into p_dst.
 * @retval VLC_EGENERIC No pictures in history buffer, cannot render.
 * @see RenderBob()
 * @see RenderLinear()
 * @see Deinterlace()
 */
int RenderPhosphor( filter_t *p_filter,
                    picture_t *p_dst, picture_t *p_pic,
                    int i_order, int i_field );

/*****************************************************************************
 * Extra documentation
 *****************************************************************************/

/**
 * \file
 * "Phosphor" deinterlace algorithm. This simulates the rendering mechanism
 * of an interlaced CRT TV, actually producing *interlaced* output.
 *
 * The main use case for this filter is anime for which IVTC is not applicable.
 * This is the case, if 24fps telecined material has been mixed with 60fps
 * interlaced effects, such as in Sol Bianca or Silent Mobius. It can also
 * be used for true interlaced video, such as most camcorder recordings.
 *
 * The filter has several modes for handling 4:2:0 chroma for those output
 * frames that fall across input frame temporal boundaries (i.e. fields come
 * from different frames). Upconvert (to 4:2:2) provides the most accurate
 * CRT simulation, but requires more CPU and memory bandwidth than the other
 * modes. The other modes keep the chroma at 4:2:0.
 *
 * About these modes: telecined input (such as NTSC anime DVDs) works better
 * with AltLine, while true interlaced input works better with Latest.
 * Merge is a compromise, which may or may not look acceptable.
 * The mode can be set in the VLC advanced configuration,
 * All settings > Video > Filters > Deinterlace
 *
 * Technically speaking, this is an interlaced field renderer targeted for
 * progressive displays. It works by framerate doubling, and simulating one
 * step of light output decay of the "old" field during the "new" field,
 * until the next new field comes in to replace the "old" one.
 *
 * While playback is running, the simulated light decay gives the picture an
 * appearance of visible "scanlines", much like on a real TV. Only when the
 * video is paused, it is clearly visible that one of the fields is actually
 * brighter than the other.
 *
 * The main differences to the Bob algorithm are:
 *  - in addition to the current field, the previous one (fading out)
 *    is also rendered
 *  - some horizontal lines don't seem to flicker as much
 *  - scanline visual effect (adjustable; the dimmer strength can be set
 *    in the VLC advanced configuration)
 *  - the picture appears 25%, 38% or 44% darker on average (for dimmer
 *    strengths 1, 2 and 3)
 *  - if the input has 4:2:0 chroma, the colours may look messed up in some
 *    output frames. This is a limitation of the 4:2:0 chroma format, and due
 *    to the fact that both fields are present in each output picture. Usually
 *    this doesn't matter in practice, but see the 4:2:0 chroma mode setting
 *    in the configuration if needed (it may help a bit).
 *
 * In addition, when this filter is used on an LCD computer monitor,
 * the main differences to a real CRT TV are:
 *  - Pixel shape and grid layout; CRT TVs were designed for interlaced
 *    field rendering, while LCD monitors weren't.
 *  - No scan flicker even though the display runs (usually) at 60Hz.
 *    (This at least is a good thing.)
 *
 * The output vertical resolution should be large enough for the scaling
 * not to have a too adverse effect on the regular scanline pattern.
 * In practice, NTSC video can be acceptably rendered already at 1024x600
 * if fullscreen even on an LCD. PAL video requires more.
 *
 * Just like Bob, this filter works properly only if the input framerate
 * is stable. Otherwise the scanline effect breaks down and the picture
 * will flicker.
 */

#endif
