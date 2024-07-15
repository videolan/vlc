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
#ifndef CLOCK_H
#define CLOCK_H

#include <vlc_clock.h>

enum vlc_clock_master_source
{
    VLC_CLOCK_MASTER_AUTO= 0,
    VLC_CLOCK_MASTER_AUDIO,
    VLC_CLOCK_MASTER_INPUT,
    VLC_CLOCK_MASTER_MONOTONIC,
};

/**
 * Callbacks for the owner of the main clock
 */
struct vlc_clock_cbs
{
    /**
     * Called when a clock is updated
     *
     * @param system_ts system date when the ts will be rendered,
     * VLC_TICK_INVALID when the clock is reset or VLC_TICK_MAX when the update
     * is forced (an output was still rendered while paused for example). Note:
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
 * Event callbacks for the user of a vlc_clock_t
 */
struct vlc_clock_event_cbs
{
    /**
     * Called when the master source triggered a discontinuity.
     *
     * A discontinuity happens when:
     *  - The first point is updated from the master source
     *
     * @param data opaque pointer set from vlc_clock_main_New()
     */
    void (*on_discontinuity)(void *data);
};

typedef struct vlc_clock_listener_id vlc_clock_listener_id;

/**
 * This function creates the vlc_clock_main_t of the program
 */
vlc_clock_main_t *vlc_clock_main_New(struct vlc_logger *parent_logger, struct vlc_tracer *parent_tracer);

/**
 * Destroy the clock main
 *
 * @param main_clock the unlocked main_clock
 */
void vlc_clock_main_Delete(vlc_clock_main_t *main_clock);

/**
 * Lock the main_clock mutex
 *
 * All vlc_clock_main_t functions must be called with the lock held, except
 * vlc_clock_main_Delete()
 *
 * @param main_clock the unlocked main_clock
 */
void vlc_clock_main_Lock(vlc_clock_main_t *main_clock);

/**
 * Unlock the main_clock mutex
 *
 * @param main_clock the locked main_clock
 */
void vlc_clock_main_Unlock(vlc_clock_main_t *main_clock);

/**
 * Reset the vlc_clock_main_t
 *
 * @param main_clock the locked main_clock
 */
void vlc_clock_main_Reset(vlc_clock_main_t *main_clock);

/**
 * Set the first PCR point
 *
 * @param main_clock the locked main_clock
 */
void vlc_clock_main_SetFirstPcr(vlc_clock_main_t *main_clock,
                                vlc_tick_t system_now, vlc_tick_t ts);

/**
 * Set the input dejitter
 *
 * @param main_clock the locked main_clock
 */
void vlc_clock_main_SetInputDejitter(vlc_clock_main_t *main_clock,
                                     vlc_tick_t delay);

/**
 * This function sets the dejitter delay to absorb the clock jitter
 *
 * Also used as the maximum delay before the synchro is considered to kick in.
 *
 * @param main_clock the locked main_clock
 */
void vlc_clock_main_SetDejitter(vlc_clock_main_t *main_clock, vlc_tick_t dejitter);


/**
 * This function allows changing the pause status.
 *
 * @param main_clock the locked main_clock
 */
void vlc_clock_main_ChangePause(vlc_clock_main_t *clock, vlc_tick_t system_now,
                                bool paused);

/**
 * This function creates a new master vlc_clock_t interface
 *
 * @warning There can be only one master at a given time.
 *
 * You must use vlc_clock_Delete to free it.
 *
 * @param main_clock the locked main_clock
 */
vlc_clock_t *vlc_clock_main_CreateMaster(vlc_clock_main_t *main_clock,
                                         const char *track_str_id,
                                         const struct vlc_clock_cbs *cbs,
                                         void *cbs_data);

/**
 * This function creates a new input master vlc_clock_t interface
 *
 * Once the input master is created, the current or future master clock created
 * from vlc_clock_main_CreateMaster() will be demoted as slave.
 *
 * @warning There can be only one input master at a given time.
 *
 * You must use vlc_clock_Delete to free it.
 *
 * @param main_clock the locked main_clock
 */
vlc_clock_t *vlc_clock_main_CreateInputMaster(vlc_clock_main_t *main_clock);

/**
 * This function creates a new input slave vlc_clock_t interface
 *
 * @warning There can be only one input, slave or master, at a given time.
 *
 * You must use vlc_clock_Delete to free it.
 *
 * @param main_clock the locked main_clock
 */
vlc_clock_t *vlc_clock_main_CreateInputSlave(vlc_clock_main_t *main_clock);

/**
 * This function creates a new slave vlc_clock_t interface
 *
 * You must use vlc_clock_Delete to free it.
 *
 * @param main_clock the locked main_clock
 */
vlc_clock_t *vlc_clock_main_CreateSlave(vlc_clock_main_t *main_clock,
                                        const char *track_str_id,
                                        enum es_format_category_e cat,
                                        const struct vlc_clock_cbs *cbs,
                                        void *cbs_data);

/**
 * This function creates a new slave vlc_clock_t interface
 *
 * You must use vlc_clock_Delete to free it.
 *
 * @param main_clock the locked main_clock
 */
vlc_clock_t *vlc_clock_CreateSlave(const vlc_clock_t *clock,
                                   enum es_format_category_e cat);

/**
 * This function free the resources allocated by vlc_clock*Create*()
 *
 * @param clock the unlocked clock used by the source
 */
void vlc_clock_Delete(vlc_clock_t *clock);

/**
 * This function will update the clock drift and returns the drift
 *
 * @param clock the locked clock used by the source
 * @param system_now valid system time or VLC_TICK_MAX is the updated point is
 *        forced (when paused for example)
 * @param ts the timestamp in media time for the updated point
 * @param rate the current playback speed
 *
 * @return a valid drift relative time, VLC_TICK_INVALID if there is no drift
 * (clock is master)
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
 *
 * @param clock the locked clock used by the source
 */
void vlc_clock_Reset(vlc_clock_t *clock);

/**
 * This functions change the clock delay
 *
 * It returns the amount of time the clock owner need to wait in order to reach
 * the time introduced by the new positive delay.
 *
 * @param clock the locked clock used by the source
 */
vlc_tick_t vlc_clock_SetDelay(vlc_clock_t *clock, vlc_tick_t ts_delay);

/**
 * Lock the clock mutex
 *
 * @param clock the unlocked clock used by the source
 */
void vlc_clock_Lock(vlc_clock_t *clock);

/**
 * Unlock the clock mutex
 *
 * @param clock the locked clock used by the source
 */
void vlc_clock_Unlock(vlc_clock_t *clock);

/**
 * Indicate if the clock is paused
 *
 * The clock mutex must be locked.
 *
 * @param clock the locked clock used by the source
 * @retval true if the clock is paused
 * @retval false if the clock is not paused
 */
bool vlc_clock_IsPaused(const vlc_clock_t *clock);

/**
 * Wait for a timestamp expressed in system time
 *
 * The wait will be interrupted (signaled) on clock state changes which could
 * invalidate the computed deadline. In that case, the caller must recompute
 * the new deadline and call it again.
 *
 * @param clock the locked clock used by the source
 * @return 0 if the condition was signaled, an error code in case of timeout
 */
int vlc_clock_Wait(vlc_clock_t *clock, vlc_tick_t system_deadline);

/**
 * Wake up any vlc_clock_Wait()
 *
 * @param clock the locked clock used by the source
 */
void vlc_clock_Wake(vlc_clock_t *clock);

/**
 * Add a listener for events
 *
 * @param clock the locked clock used by the source
 * @param cbs valid pointer to register events
 * @param data opaque data used by cbs
 * @return a valid listener id, or NULL in case of allocation error
 */
vlc_clock_listener_id *
vlc_clock_AddListener(vlc_clock_t *clock,
                      const struct vlc_clock_event_cbs *cbs,
                      void *data);

/**
 * Remove a event listener callback
 *
 * @param clock the locked clock used by the source
 * @param listener_id listener id returned by vlc_clock_AddListener()
 */
void
vlc_clock_RemoveListener(vlc_clock_t *clock, vlc_clock_listener_id *listener_id);

/**
 * This function converts a timestamp from stream to system
 *
 * @param clock the locked clock used by the source
 * @param clock_id pointer to the clock id used for conversion. Can be NULL.
 * @return the valid system time
 */
vlc_tick_t vlc_clock_ConvertToSystem(vlc_clock_t *clock,
                                     vlc_tick_t system_now, vlc_tick_t ts,
                                     double rate, uint32_t *clock_id);

/**
 * Starts a new clock based on the given clock point, accounting for
 * previous updates.
 *
 * @param clock       an input or output clock that is being started
 * @param start_date  the system time at which the first update starts
 * @param first_ts    the media time origin
 **/
void vlc_clock_Start(vlc_clock_t *clock,
                     vlc_tick_t start_date,
                     vlc_tick_t first_ts);

#endif /*CLOCK_H*/
