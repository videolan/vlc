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

enum vlc_clock_master_source
{
    VLC_CLOCK_MASTER_AUDIO = 0,
    VLC_CLOCK_MASTER_MONOTONIC,
    VLC_CLOCK_MASTER_DEFAULT = VLC_CLOCK_MASTER_AUDIO,
};

typedef struct vlc_clock_main_t vlc_clock_main_t;
typedef struct vlc_clock_t vlc_clock_t;

/**
 * Callbacks for the owner of the main clock
 */
struct vlc_clock_cbs
{
    /**
     * Called when a clock is updated
     *
     * @param system_ts system date when the ts will be rendered,
     * VLC_TICK_INVALID when the clock is reset or INT64_MAX when the update is
     * forced (an output was still rendered while paused for example). Note:
     * when valid, this date can be in the future, it is not necessarily now.
     * @param ts stream timestamp or VLC_TICK_INVALID when the clock is reset,
     * should be subtracted with VLC_TICK_0 to get the original value
     * @param rate rate used when updated
     * @param frame_rate fps of the video owning the clock
     * @param frame_rate_base fps denominator
     * @param data opaque pointer set from vlc_clock_main_New()
     */
    void (*on_update)(vlc_tick_t system_ts, vlc_tick_t ts, double rate,
                      unsigned frame_rate, unsigned frame_rate_base,
                      void *data);
};

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
                                vlc_tick_t system_now, vlc_tick_t ts);
void vlc_clock_main_SetInputDejitter(vlc_clock_main_t *main_clock,
                                     vlc_tick_t delay);

/**
 * This function sets the dejitter delay to absorb the clock jitter
 *
 * Also used as the maximum delay before the synchro is considered to kick in.
 */
void vlc_clock_main_SetDejitter(vlc_clock_main_t *main_clock, vlc_tick_t dejitter);


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
vlc_clock_t *vlc_clock_main_CreateMaster(vlc_clock_main_t *main_clock,
                                         const struct vlc_clock_cbs *cbs,
                                         void *cbs_data);

/**
 * This function creates a new slave vlc_clock_t interface
 *
 * You must use vlc_clock_Delete to free it.
 */
vlc_clock_t *vlc_clock_main_CreateSlave(vlc_clock_main_t *main_clock,
                                        enum es_format_category_e cat,
                                        const struct vlc_clock_cbs *cbs,
                                        void *cbs_data);

/**
 * This function creates a new slave vlc_clock_t interface
 *
 * You must use vlc_clock_Delete to free it.
 */
vlc_clock_t *vlc_clock_CreateSlave(const vlc_clock_t *clock,
                                   enum es_format_category_e cat);

/**
 * This function free the resources allocated by vlc_clock*Create*()
 */
void vlc_clock_Delete(vlc_clock_t *clock);

/**
 * This function will update the clock drift and returns the drift
 * @param system_now valid system time or INT64_MAX is the updated point is
 * forced (when paused for example)
 * @return a valid drift relative time, VLC_TICK_INVALID if there is no drift
 * (clock is master) or INT64_MAX if the clock is paused
 */
vlc_tick_t vlc_clock_Update(vlc_clock_t *clock, vlc_tick_t system_now,
                            vlc_tick_t ts, double rate);

/**
 * This function will update the video clock drift and returns the drift
 *
 * Same behavior than vlc_clock_Update() except that the video is passed to the
 * clock, this will be used for clock update callbacks.
 */
vlc_tick_t vlc_clock_UpdateVideo(vlc_clock_t *clock, vlc_tick_t system_now,
                                 vlc_tick_t ts, double rate,
                                 unsigned frame_rate, unsigned frame_rate_base);

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
vlc_tick_t vlc_clock_SetDelay(vlc_clock_t *clock, vlc_tick_t ts_delay);

/**
 * Wait for a timestamp expressed in stream time
 */
int vlc_clock_Wait(vlc_clock_t *clock, vlc_tick_t system_now, vlc_tick_t ts,
                   double rate, vlc_tick_t max_duration);

/**
 * This function converts a timestamp from stream to system
 * @return the valid system time or INT64_MAX when the clock is paused
 */
vlc_tick_t vlc_clock_ConvertToSystem(vlc_clock_t *clock, vlc_tick_t system_now,
                                     vlc_tick_t ts, double rate);

/**
 * This functon converts an array of timestamp from stream to system
 */
void vlc_clock_ConvertArrayToSystem(vlc_clock_t *clock, vlc_tick_t system_now,
                                    vlc_tick_t *ts_array, size_t ts_count,
                                    double rate);

#endif /*VLC_CLOCK_H*/
