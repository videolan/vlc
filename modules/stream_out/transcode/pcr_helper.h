/*****************************************************************************
 * pcr_helper.h:
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#ifndef PCR_HELPER_H
#define PCR_HELPER_H

#include <vlc_common.h>

#include <vlc_frame.h>

#include "pcr_sync.h"

/**
 * The transcode PCR helper is a wrapper of the pcr_sync helper specific for processing
 * units that have a high risk of altering the frames timestamps or simply dropping them. The usual
 * case is an encoder and/or a filter chain which is excactly what transcode does.
 *
 * This helper uses an approximation of the max frame `delay` taken by the processing unit. When
 * this delay is bypassed, it is assumed that a frame was dropped and eventually a new PCR can be
 * synthetised and forwarded to keep the stream going.
 */

/**
 * Opaque internal state.
 */
typedef struct transcode_track_pcr_helper transcode_track_pcr_helper_t;

/**
 * Allocate a new pcr helper.
 *
 * \param sync_ref Pointer to the pcr_sync, must be valid during all the helper's lifetime.
 * \param max_delay Maximum frame delay that can be taken by the processing unit (See explanations
 * above).
 *
 * \return A pointer to the allocated pcr helper or NULL if the allocation failed.
 */
transcode_track_pcr_helper_t *transcode_track_pcr_helper_New(vlc_pcr_sync_t *sync_ref,
                                                             vlc_tick_t max_delay);

/**
 * Delete and free the helper.
 */
void transcode_track_pcr_helper_Delete(transcode_track_pcr_helper_t *);

/**
 * Signal a frame input to the processing unit.
 *
 * \param frame The entering frame (will be treated as a single frame).
 * \param[out] dropped_frame_pcr If a frame was dropped, contains a synthetised PCR.
 * VLC_TICK_INVALID otherwise.
 *
 * \retval VLC_SUCCESS On success.
 * \retval VLC_ENOMEM On allocation error.
 */
int transcode_track_pcr_helper_SignalEnteringFrame(transcode_track_pcr_helper_t *,
                                                   const vlc_frame_t *frame,
                                                   vlc_tick_t *dropped_frame_pcr);

/**
 * Signal a frame output from the processing unit.
 *
 * This function return the following PCR to forward if any is available.
 * \note This can be called multiple times to handle multiple following PCR's.
 * \note If the frame output has seen its timestamp altered, this helper will synthetise a new PCR.
 *
 * \param frame The leaving frame (will be treated as a single frame).
 *
 * \return The PCR value following the frame or VLC_TICK_INVALID if no PCR is following.
 */
vlc_tick_t transcode_track_pcr_helper_SignalLeavingFrame(transcode_track_pcr_helper_t *,
                                                         const vlc_frame_t *frame);

#endif
