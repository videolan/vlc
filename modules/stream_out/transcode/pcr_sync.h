/*****************************************************************************
 * pcr_sync.h
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

#ifndef PCR_SYNC_H
#define PCR_SYNC_H

#if defined(__cplusplus)
 extern "C" {
#endif

/**
 * The PCR-Sync utility is to be used in "stream output filters" such as Transcode where frames are
 * treated asynchronously and are supposedly sent along with PCR events.
 *
 * schematic representation of the problematic:
 *
 *                +------------------+
 * ES-1 Frames -->|   Async Frames   |------>
 * ES-2 Frames -->|  Processing Unit |------>
 *                +------------------+
 *         PCR ----------------------------->
 *         ^
 *         +- PCR doesn't go through the processing unit, thus needs to be synchronized to the
 *            unit's frame output to be relevant.
 */

/**
 * Opaque PCR-Sync internal state.
 */
typedef struct vlc_pcr_sync vlc_pcr_sync_t;

/**
 * Allocate and initialize a new pcr_sync.
 *
 * \return An heap allocated pcr_sync or NULL if the allocation failed.
 */
vlc_pcr_sync_t *vlc_pcr_sync_New(void) VLC_USED;

/**
 * Delete a pcr_sync and discard its internal state.
 */
void vlc_pcr_sync_Delete(vlc_pcr_sync_t *);

/**
 * Create a new ES ID.
 *
 * Create a new id to provide an ES context to frame input.
 *
 * \param[out] id The assigned ID if the function is successful, undefined otherwise.
 *
 * \retval VLC_SUCCESS On success.
 * \retval VLC_ENOMEM On internal allocation error.
 */
int vlc_pcr_sync_NewESID(vlc_pcr_sync_t *, unsigned int *id);

/**
 * Delete the given ES ID
 */
void vlc_pcr_sync_DelESID(vlc_pcr_sync_t *, unsigned int id);

/**
 * Signal a frame input from the Frame Processing Unit.
 *
 * The frame timestamp will be cached and used in later \ref SignalPCR calls to determine the
 * correct PCR forwarding point.
 *
 * \param id The ES ID (See \ref vlc_pcr_sync_NewESID).
 */
void vlc_pcr_sync_SignalFrame(vlc_pcr_sync_t *, unsigned int id, const vlc_frame_t *);

/**
 * Signal a frame output from the Frame Processing Unit.
 *
 * This function return the following PCR to forward if any is available.
 * \note This can be called multiple times to handle multiple following PCR's.
 *
 * \param id The ES ID (See \ref vlc_pcr_sync_NewESID).
 *
 * \return The PCR value following the frame or VLC_TICK_INVALID if no PCR is following.
 */
vlc_tick_t vlc_pcr_sync_SignalFrameOutput(vlc_pcr_sync_t *, unsigned int id, const vlc_frame_t *);

/**
 * Status code returned by \ref vlc_pcr_sync_SignalPCR when PCR could be directly forwarded
 */
#define VLC_PCR_SYNC_FORWARD_PCR (-EAGAIN)

/**
 * Signal a PCR event.
 *
 * \param pcr The PCR value.
 *
 * \retval VLC_SUCCESS On success.
 * \retval VLC_ENOMEM On allocation error.
 * \retval VLC_PCR_SYNC_FORWARD_PCR When the Signaled PCR could be immediately sent and is not
 * queued.
 */
int vlc_pcr_sync_SignalPCR(vlc_pcr_sync_t *, vlc_tick_t pcr);

#if defined( __cplusplus )
}
#endif

#endif
