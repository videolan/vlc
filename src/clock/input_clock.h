/*****************************************************************************
 * input_clock.h: Input clocks synchronisation
 *****************************************************************************
 * Copyright (C) 2008-2018 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
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
 * This function creates a new input_clock_t.
 *
 * You must use input_clock_Delete to delete it once unused.
 */
input_clock_t *input_clock_New( float rate, bool recovery );

/**
 * This function attach a clock listener to the input clock
 *
 * It can be called only one time, with a valid clock, before the first update
 * (input_clock_Update()).
 *
 * \param clock_listener clock created with vlc_clock_main_CreateInputMaster().
 * The input_clock_t will take ownership of this clock and drive the main
 * clock.
 */
void input_clock_AttachListener( input_clock_t *, vlc_clock_t *clock_listener );

/**
 * This function destroys a input_clock_t created by input_clock_New.
 */
void           input_clock_Delete( input_clock_t * );

/**
 * This function will update a input_clock_t with a new clock reference point.
 * It will also tell if the clock point is late regarding our buffering.
 *
 * \param b_buffering_allowed tells if we are allowed to bufferize more data in
 * advanced (if possible).
 * \return clock update delay
 */
vlc_tick_t input_clock_Update( input_clock_t *, vlc_object_t *p_log,
                            bool b_can_pace_control, bool b_buffering_allowed,
                            vlc_tick_t i_clock, vlc_tick_t i_system );
/**
 * This function will reset the drift of a input_clock_t.
 *
 * The actual jitter estimation will not be reset by it.
 */
void    input_clock_Reset( input_clock_t * );

/**
 * This functions will return a deadline used to control the reading speed.
 */
vlc_tick_t input_clock_GetWakeup( input_clock_t * );

/**
 * This functions allows changing the actual reading speed.
 */
void    input_clock_ChangeRate( input_clock_t *, float rate );

/**
 * This function allows changing the pause status.
 */
void    input_clock_ChangePause( input_clock_t *, bool b_paused, vlc_tick_t i_date );

/**
 * This function returns the original system value date and the delay for the current
 * reference point (a valid reference point must have been set).
 */
void    input_clock_GetSystemOrigin( input_clock_t *, vlc_tick_t *pi_system, vlc_tick_t *pi_delay );

/**
 * This function allows rebasing the original system value date (a valid
 * reference point must have been set).
 * When using the absolute mode, it will create a discontinuity unless
 * called immediately after a input_clock_Update.
 */
void    input_clock_ChangeSystemOrigin( input_clock_t *, bool b_absolute, vlc_tick_t i_system );

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
 * This function allows the set the minimal configuration for the jitter estimation algo.
 */
void input_clock_SetJitter( input_clock_t *,
                            vlc_tick_t i_pts_delay, int i_cr_average );

/**
 * This function returns an estimation of the pts_delay needed to avoid rebufferization.
 * XXX in the current implementation, the pts_delay will never be decreased.
 */
vlc_tick_t input_clock_GetJitter( input_clock_t * );

void input_clock_EnableRecovery( input_clock_t *, bool );

#endif
