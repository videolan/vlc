/*****************************************************************************
 * input_clock.h: Input clocks synchronisation
 *****************************************************************************
 * Copyright (C) 2008-2018 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 * Copyright (C) 2023-2025 Alexandre Janniaux <ajanni@videolabs.io>
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#ifndef LIBVLC_INPUT_CLOCK_H
#define LIBVLC_INPUT_CLOCK_H 1

#include <vlc_common.h>
#include <vlc_input.h> /* FIXME Needed for input_clock_t */
#include "clock.h"

/** @struct input_clock_t
 * This structure is used to manage clock drift and reception jitters
 *
 * This API is reentrant but not thread-safe.
 */
typedef struct input_clock_t input_clock_t;

/**
 * Callbacks for the input_clock_t listener.
 *
 * \see input_clock_AttachListener
 */
struct vlc_input_clock_cbs {
    /**
     * Notify the listener that the buffering made progress.
     *
     * \param opaque        listener private data set from
     *                      \ref input_clock_AttachListener
     * \param ck_system     time reference for the buffering progress
     * \param ck_stream     progress of the buffering in tick
     * \param rate          current playback rate for the buffering
     * \param discontinuity PCR discontinuity with the previous update
     *
     * \return              how much time the playback has drifted from
     *                      the main clock
     */
    vlc_tick_t (*update)(void *opaque, vlc_tick_t ck_system,
                         vlc_tick_t ck_stream, double rate, bool discontinuity);

    /**
     * Notify the listener that the buffering needed a reset.
     *
     * \param opaque        listener private data set from
     *                      \ref input_clock_AttachListener
     */
    void (*reset)(void *opaque);
};

/**
 * This function creates a new input_clock_t.
 *
 * You must use input_clock_Delete to delete it once unused.
 */
input_clock_t *input_clock_New(struct vlc_logger *logger, float rate);

/**
 * This function attach a clock listener to the input clock
 *
 * It can be called only one time, with a valid clock, before the first update
 * (input_clock_Update()).
 *
 * \param clock the input clock to attach the listener to
 * \param listener an input clock listener virtual table
 * \param opaque an opaque pointer forwarded to the listener
 *
 */
void input_clock_AttachListener(input_clock_t *clock,
                                const struct vlc_input_clock_cbs *clock_listener,
                                void *opaque);

/**
 * This function destroys a input_clock_t created by input_clock_New.
 */
void input_clock_Delete(input_clock_t *);

/**
 * This function will update a input_clock_t with a new clock reference point.
 * It will also tell if the clock point is late regarding our buffering.
 *
 * \param clock the input clock object to update with the new point
 * \param b_can_pace_control whether the input can control the speed of playback
 * \param b_buffering whether the input is buffering
 * \param b_extra_buffering_allowed tells if we are allowed to bufferize more
 *        data in advance (if possible).
 * \param i_clock the new clock reference value
 * \param i_system the timestmap at which the new reference has been reported
 *
 * \return clock update delay
 */
vlc_tick_t input_clock_Update(input_clock_t *clock,
                            bool b_can_pace_control, bool b_buffering,
                            bool b_extra_buffering_allowed,
                            vlc_tick_t i_clock, vlc_tick_t i_system );

/**
 * This function will reset the drift of a input_clock_t.
 *
 * The actual jitter estimation will not be reset by it.
 */
void input_clock_Reset( input_clock_t * );

/**
 * This functions will return a deadline used to control the reading speed.
 */
vlc_tick_t input_clock_GetWakeup( input_clock_t * );

/**
 * This functions allows changing the actual reading speed.
 */
void input_clock_ChangeRate(input_clock_t *, float rate);

/**
 * This function allows changing the pause status.
 */
void input_clock_ChangePause(input_clock_t *, bool b_paused, vlc_tick_t i_date);

/**
 * This function allows rebasing the original system value date (a valid
 * reference point must have been set).
 */
void input_clock_ChangeSystemOrigin(input_clock_t *, vlc_tick_t i_system);

/**
 * This function returns the current rate.
 */
float input_clock_GetRate( input_clock_t * );

/**
 * This function returns current clock state or VLC_EGENERIC if there is not a
 * reference point.
 */
int input_clock_GetState( input_clock_t *,
                          vlc_tick_t *pi_stream_start, vlc_tick_t *pi_system_start,
                          vlc_tick_t *pi_stream_duration, vlc_tick_t *pi_system_duration );

/**
 * Return the duration of stream buffered as well as for how long the
 * buffering has been started.
 *
 * @param clock An input clock with valid references
 * @param stream_duration the available buffering duration
 * @param system_duration the time spent in buffering
 */
int input_clock_GetBufferingDuration(
    const input_clock_t *clock,
    vlc_tick_t *stream_duration,
    vlc_tick_t *system_duration);

/**
 * Return for how long the buffering would be running up to the system
 * reference given.
 */
vlc_tick_t input_clock_GetSystemDuration(
    const input_clock_t *clock,
    vlc_tick_t system_reference);

/**
 * This function allows the set the minimal configuration for the jitter estimation algo.
 */
void input_clock_SetJitter( input_clock_t *,
                            vlc_tick_t i_pts_delay, int i_cr_average );

/**
 * This function returns an estimation of the pts_delay needed to avoid rebufferization.
 * XXX in the current implementation, the pts_delay will never be decreased.
 */
vlc_tick_t input_clock_GetJitter( input_clock_t * );

#endif
