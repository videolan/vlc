/*****************************************************************************
 * clock.h: Output modules synchronisation clock
 *****************************************************************************
 * Copyright (C) 2018-2019 VLC authors, VideoLAN and Videolabs SAS
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
#ifndef VLC_CLOCK_H
#define VLC_CLOCK_H

typedef struct vlc_clock_main_t vlc_clock_main_t;
typedef struct vlc_clock_t vlc_clock_t;

/**
 * This function creates the vlc_clock_main_t of the program
 */
vlc_clock_main_t *vlc_clock_main_New(void);

/**
 * Destroy the clock main
 */
void vlc_clock_main_Delete(vlc_clock_main_t *main_clock);

/**
 * Abort all the pending vlc_clock_Wait
 */
void vlc_clock_main_Abort(vlc_clock_main_t *main_clock);

/**
 * Reset the vlc_clock_main_t
 */
void vlc_clock_main_Reset(vlc_clock_main_t *main_clock);

void vlc_clock_main_SetFirstPcr(vlc_clock_main_t *main_clock,
                                vlc_tick_t system_now, vlc_tick_t pts);
void vlc_clock_main_SetInputDejitter(vlc_clock_main_t *main_clock,
                                     vlc_tick_t delay);

/**
 * This function allows changing the pause status.
 */
void vlc_clock_main_ChangePause(vlc_clock_main_t *clock, vlc_tick_t system_now,
                                bool paused);

/**
 * This function set the allocated interface as the master making the current
 * master if any a slave.
 */
void vlc_clock_main_SetMaster(vlc_clock_main_t *main_clock, vlc_clock_t *clock);

/**
 * This function creates a new master vlc_clock_t interface
 *
 * You must use vlc_clock_Delete to free it.
 */
vlc_clock_t *vlc_clock_main_CreateMaster(vlc_clock_main_t *main_clock);

/**
 * This function creates a new slave vlc_clock_t interface
 *
 * You must use vlc_clock_Delete to free it.
 */
vlc_clock_t *vlc_clock_main_CreateSlave(vlc_clock_main_t *main_clock);

/**
 * This function creates a new slave vlc_clock_t interface
 *
 * You must use vlc_clock_Delete to free it.
 */
vlc_clock_t *vlc_clock_CreateSlave(const vlc_clock_t *clock);

/**
 * This function free the resources allocated by vlc_clock*Create*()
 */
void vlc_clock_Delete(vlc_clock_t *clock);

/**
 * This function will update the clock drift and returns the drift
 */
vlc_tick_t vlc_clock_Update(vlc_clock_t *clock, vlc_tick_t system_now,
                            vlc_tick_t pts, double rate);

/**
 * This function resets the clock drift
 */
void vlc_clock_Reset(vlc_clock_t *clock);

/**
 * This functions change the clock delay
 *
 * It returns the amount of time the clock owner need to wait in order to reach
 * the time introduced by the new positive delay.
 */
vlc_tick_t vlc_clock_SetDelay(vlc_clock_t *clock, vlc_tick_t pts_delay);

/**
 * Wait for a timestamp expressed in stream time
 */
int vlc_clock_Wait(vlc_clock_t *clock, vlc_tick_t system_now, vlc_tick_t pts,
                   double rate, vlc_tick_t max_duration);

/**
 * This function converts a timestamp from stream to system
 */
vlc_tick_t vlc_clock_ConvertToSystem(vlc_clock_t *clock, vlc_tick_t system_now,
                                     vlc_tick_t pts, double rate);

/**
 * This functon converts an array of timestamp from stream to system
 */
void vlc_clock_ConvertArrayToSystem(vlc_clock_t *clock, vlc_tick_t system_now,
                                    vlc_tick_t *pts_array, size_t pts_count,
                                    double rate);

/**
 * This function sets the dejitter delay to absorb the clock jitter
 *
 * Also used as the maximum delay before the synchro is considered to kick in.
 */
void vlc_clock_SetDejitter(vlc_clock_t *clock, vlc_tick_t delay);

#endif /*VLC_CLOCK_H*/
