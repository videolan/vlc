/*****************************************************************************
 * helpers.h : Generic helper functions for the VLC deinterlacer
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

#ifndef VLC_DEINTERLACE_HELPERS_H
#define VLC_DEINTERLACE_HELPERS_H 1

/**
 * \file
 * Generic helper functions for the VLC deinterlacer, used in
 * some of the advanced algorithms.
 */

/* Forward declarations */
struct filter_t;
struct picture_t;
struct plane_t;

/**
 * Chroma operation types for composing 4:2:0 frames.
 * @see ComposeFrame()
 */
typedef enum { CC_ALTLINE, CC_UPCONVERT, CC_SOURCE_TOP, CC_SOURCE_BOTTOM,
               CC_MERGE } compose_chroma_t;

/**
 * Helper function: composes a frame from the given field pair.
 *
 * Caller must manage allocation/deallocation of p_outpic.
 *
 * The inputs are full pictures (frames); only one field
 * will be used from each.
 *
 * Chroma formats of the inputs must match. It is also desirable that the
 * visible pitches of both inputs are the same, so that this will do something
 * sensible. The pitch or visible pitch of the output does not need to match
 * with the input; the compatible (smaller) part of the visible pitch will
 * be filled.
 *
 * The i_output_chroma parameter must always be supplied, but it is only used
 * when the chroma format of the input is detected as 4:2:0. Available modes:
 *   - CC_ALTLINE:       Alternate line copy, like for luma. Chroma line 0
 *                       comes from top field picture, chroma line 1 comes
 *                       from bottom field picture, chroma line 2 from top
 *                       field picture, and so on. This is usually the right
 *                       choice for IVTCing NTSC DVD material, but rarely
 *                       for any other use cases.
 *   - CC_UPCONVERT:     The output will have 4:2:2 chroma. All 4:2:0 chroma
 *                       data from both input fields will be used to generate
 *                       the 4:2:2 chroma data of the output. Each output line
 *                       will thus have independent chroma. This is a good
 *                       choice for most purposes except IVTC, if the machine
 *                       can handle the increased throughput. (Make sure to
 *                       allocate a 4:2:2 output picture first!)
 *                       This mode can also be used for converting a 4:2:0
 *                       frame to 4:2:2 format (by passing the same input
 *                       picture for both input fields).
 *                       Conversions: I420, YV12 --> I422
 *                                    J420       --> J422
 *   - CC_SOURCE_TOP:    Copy chroma of source top field picture.
 *                       Ignore chroma of source bottom field picture.
 *   - CC_SOURCE_BOTTOM: Copy chroma of source bottom field picture.
 *                       Ignore chroma of source top field picture.
 *   - CC_MERGE:         Average the chroma of the input field pictures.
 *                       (Note that this has no effect if the input fields
 *                        come from the same frame.)
 *
 * @param p_filter The filter instance (determines input chroma).
 * @param p_outpic Composed picture is written here. Allocated by caller.
 * @param p_inpic_top Picture to extract the top field from.
 * @param p_inpic_bottom Picture to extract the bottom field from.
 * @param i_output_chroma Chroma operation mode for 4:2:0 (see function doc)
 * @param swapped_uv_conversion Swap UV while up converting (for YV12)
 * @see compose_chroma_t
 * @see RenderPhosphor()
 * @see RenderIVTC()
 */
void ComposeFrame( filter_t *p_filter,
                   picture_t *p_outpic,
                   picture_t *p_inpic_top, picture_t *p_inpic_bottom,
                   compose_chroma_t i_output_chroma, bool swapped_uv_conversion );

/**
 * Helper function: Estimates the number of 8x8 blocks which have motion
 * between the given pictures. Needed for various detectors in RenderIVTC().
 *
 * Number of planes and visible lines in each plane, in the inputs must match.
 * If the visible pitches do not match, only the compatible (smaller)
 * part will be tested.
 *
 * Note that the return value is NOT simply *pi_top + *pi_bot, because
 * the fields and the full block use different motion thresholds.
 *
 * If you do not want the separate field scores, pass NULL for pi_top and
 * pi_bot. This does not affect computation speed, and is only provided as
 * a syntactic convenience.
 *
 * Motion in each picture plane (Y, U, V) counts separately.
 * The sum of number of blocks with motion across all planes is returned.
 *
 * For 4:2:0 chroma, even-numbered chroma lines make up the "top field" for
 * chroma, and odd-numbered chroma lines the "bottom field" for chroma.
 * This is correct for IVTC purposes.
 *
 * @param[in] p_prev Previous picture
 * @param[in] p_curr Current picture
 * @param[out] pi_top Number of 8x8 blocks where top field has motion.
 * @param[out] pi_bot Number of 8x8 blocks where bottom field has motion.
 * @return Number of 8x8 blocks that have motion.
 * @retval -1 Error: incompatible input pictures.
 * @see TestForMotionInBlock()
 * @see RenderIVTC()
 */
int EstimateNumBlocksWithMotion( const picture_t* p_prev,
                                 const picture_t* p_curr,
                                 int *pi_top, int *pi_bot);

/**
 * Helper function: estimates "how much interlaced" the given field pair is.
 *
 * It is allowed that p_pic_top == p_pic_bottom.
 *
 * If p_pic_top != p_pic_bot (fields come from different pictures), you can use
 * ComposeFrame() to actually construct the picture if needed.
 *
 * Number of planes, and number of lines in each plane, in p_pic_top and
 * p_pic_bot must match. If the visible pitches differ, only the compatible
 * (smaller) part will be tested.
 *
 * Luma and chroma planes are tested in the same way. This is correct for
 * telecined input, where in the interlaced frames also chroma alternates
 * every chroma line, even if the chroma format is 4:2:0!
 *
 * This is just a raw detector that produces a score. The overall score
 * indicating a progressive or interlaced frame may vary wildly, depending on
 * the material, especially in anime. The scores should be compared to
 * each other locally (in the temporal sense) to make meaningful decisions
 * about progressive or interlaced frames.
 *
 * @param p_pic_top Picture to take the top field from.
 * @param p_pic_bot Picture to take the bottom field from (same or different).
 * @return Interlace score, >= 0. Higher values mean more interlaced.
 * @retval -1 Error: incompatible input pictures.
 * @see RenderIVTC()
 * @see ComposeFrame()
 */
int CalculateInterlaceScore( const picture_t* p_pic_top,
                             const picture_t* p_pic_bot );

#endif
