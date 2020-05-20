/*****************************************************************************
 * clock.c: Output modules synchronisation clock
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include <assert.h>
#include <limits.h>
#include "clock.h"
#include "clock_internal.h"

struct vlc_clock_main_t
{
    vlc_mutex_t lock;
    vlc_cond_t cond;

    vlc_clock_t *master;

    unsigned rc;

    /**
     * Linear function
     * system = ts * coeff / rate + offset
     */
    clock_point_t last;
    average_t coeff_avg; /* Moving average to smooth out the instant coeff */
    double rate;
    double coeff;
    vlc_tick_t offset;
    vlc_tick_t delay;

    vlc_tick_t pause_date;

    unsigned wait_sync_ref_priority;
    clock_point_t wait_sync_ref; /* When the master */
    clock_point_t first_pcr;
    vlc_tick_t output_dejitter; /* Delay used to absorb the output clock jitter */
    vlc_tick_t input_dejitter; /* Delay used to absorb the input jitter */
    bool abort;
};

struct vlc_clock_t
{
    vlc_tick_t (*update)(vlc_clock_t *clock, vlc_tick_t system_now,
                         vlc_tick_t ts, double rate,
                         unsigned frame_rate, unsigned frame_rate_base);
    void (*reset)(vlc_clock_t *clock);
    vlc_tick_t (*set_delay)(vlc_clock_t *clock, vlc_tick_t delay);
    vlc_tick_t (*to_system_locked)(vlc_clock_t *clock, vlc_tick_t system_now,
                                   vlc_tick_t ts, double rate);

    vlc_clock_main_t *owner;
    vlc_tick_t delay;
    unsigned priority;

    const struct vlc_clock_cbs *cbs;
    void *cbs_data;
};

static vlc_tick_t main_stream_to_system(vlc_clock_main_t *main_clock,
                                        vlc_tick_t ts)
{
    if (main_clock->offset == VLC_TICK_INVALID)
        return VLC_TICK_INVALID;
    return (vlc_tick_t)
        (ts * main_clock->coeff / main_clock->rate + main_clock->offset);
}

static void vlc_clock_main_reset(vlc_clock_main_t *main_clock)
{
    AvgReset(&main_clock->coeff_avg);
    main_clock->coeff = 1.0f;
    main_clock->rate = 1.0f;
    main_clock->offset = VLC_TICK_INVALID;

    main_clock->wait_sync_ref_priority = UINT_MAX;
    main_clock->wait_sync_ref =
        main_clock->last = clock_point_Create(VLC_TICK_INVALID, VLC_TICK_INVALID);
    vlc_cond_broadcast(&main_clock->cond);
}

static inline void vlc_clock_on_update(vlc_clock_t *clock,
                                       vlc_tick_t system_now,
                                       vlc_tick_t ts, double rate,
                                       unsigned frame_rate,
                                       unsigned frame_rate_base)
{
    if (clock->cbs)
        clock->cbs->on_update(system_now, ts, rate, frame_rate, frame_rate_base,
                              clock->cbs_data);
}

static vlc_tick_t vlc_clock_master_update(vlc_clock_t *clock,
                                          vlc_tick_t system_now,
                                          vlc_tick_t original_ts, double rate,
                                          unsigned frame_rate,
                                          unsigned frame_rate_base)
{
    vlc_clock_main_t *main_clock = clock->owner;

    if (unlikely(original_ts == VLC_TICK_INVALID
     || system_now == VLC_TICK_INVALID))
        return VLC_TICK_INVALID;

    const vlc_tick_t ts = original_ts + clock->delay;

    vlc_mutex_lock(&main_clock->lock);

    /* If system_now is INT64_MAX, the update is forced, don't modify anything
     * but only notify the new clock point. */
    if (system_now != INT64_MAX)
    {
        if (main_clock->offset != VLC_TICK_INVALID
         && ts != main_clock->last.stream)
        {
            if (rate == main_clock->rate)
            {
                /* We have a reference so we can update coeff */
                double instant_coeff = (system_now - main_clock->last.system)
                                     / (double)(ts - main_clock->last.stream)
                                     * rate;
                AvgUpdate(&main_clock->coeff_avg, instant_coeff);
                main_clock->coeff = AvgGet(&main_clock->coeff_avg);
            }
        }
        else
        {
            main_clock->wait_sync_ref_priority = UINT_MAX;
            main_clock->wait_sync_ref =
                clock_point_Create(VLC_TICK_INVALID, VLC_TICK_INVALID);
        }

        main_clock->offset = system_now - ts * main_clock->coeff / rate;

        main_clock->last = clock_point_Create(system_now, ts);

        main_clock->rate = rate;
        vlc_cond_broadcast(&main_clock->cond);
    }

    vlc_mutex_unlock(&main_clock->lock);

    vlc_clock_on_update(clock, system_now, original_ts, rate, frame_rate,
                        frame_rate_base);
    return VLC_TICK_INVALID;
}

static void vlc_clock_master_reset(vlc_clock_t *clock)
{
    vlc_clock_main_t *main_clock = clock->owner;

    vlc_mutex_lock(&main_clock->lock);
    vlc_clock_main_reset(main_clock);

    assert(main_clock->delay <= 0);
    assert(clock->delay >= 0);
    /* Move the delay from the slaves to the master */
    if (clock->delay != 0 && main_clock->delay != 0)
    {
        vlc_tick_t delta = clock->delay + main_clock->delay;
        if (delta > 0)
        {
            clock->delay = delta;
            main_clock->delay = 0;
        }
        else
        {
            clock->delay = 0;
            main_clock->delay = delta;
        }
    }

    vlc_mutex_unlock(&main_clock->lock);

    vlc_clock_on_update(clock, VLC_TICK_INVALID, VLC_TICK_INVALID, 1.f, 0, 0);
}

static vlc_tick_t vlc_clock_master_set_delay(vlc_clock_t *clock, vlc_tick_t delay)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);

    vlc_tick_t delta = delay - clock->delay;

    if (delta > 0)
    {
        /* The master clock is delayed */
        main_clock->delay = 0;
        clock->delay = delay;
    }
    else
    {
        /* Delay all slaves clock instead of advancing the master one */
        main_clock->delay = delta;
    }

    assert(main_clock->delay <= 0);
    assert(clock->delay >= 0);

    vlc_cond_broadcast(&main_clock->cond);
    vlc_mutex_unlock(&main_clock->lock);
    return delta;
}

static vlc_tick_t
vlc_clock_monotonic_to_system_locked(vlc_clock_t *clock, vlc_tick_t now,
                                     vlc_tick_t ts, double rate)
{
    vlc_clock_main_t *main_clock = clock->owner;

    if (clock->priority < main_clock->wait_sync_ref_priority)
    {
        /* XXX: This input_delay calculation is needed until we (finally) get
         * ride of the input clock. This code is adapted from input_clock.c and
         * is used to introduce the same delay than the input clock (first PTS
         * - first PCR). */
        const vlc_tick_t pcr_delay =
            main_clock->first_pcr.system == VLC_TICK_INVALID ? 0 :
            (ts - main_clock->first_pcr.stream) / rate +
            main_clock->first_pcr.system - now;

        const vlc_tick_t input_delay = main_clock->input_dejitter + pcr_delay;

        const vlc_tick_t delay =
            __MAX(input_delay, main_clock->output_dejitter);

        main_clock->wait_sync_ref_priority = clock->priority;
        main_clock->wait_sync_ref = clock_point_Create(now + delay, ts);
    }
    return (ts - main_clock->wait_sync_ref.stream) / rate
        + main_clock->wait_sync_ref.system;
}

static vlc_tick_t vlc_clock_slave_to_system_locked(vlc_clock_t *clock,
                                                   vlc_tick_t now,
                                                   vlc_tick_t ts, double rate)
{
    vlc_clock_main_t *main_clock = clock->owner;
    if (main_clock->pause_date != VLC_TICK_INVALID)
        return INT64_MAX;

    vlc_tick_t system = main_stream_to_system(main_clock, ts);
    if (system == VLC_TICK_INVALID)
    {
        /* We don't have a master sync point, let's fallback to a monotonic ref
         * point */
        system = vlc_clock_monotonic_to_system_locked(clock, now, ts, rate);
    }

    return system + (clock->delay - main_clock->delay) * rate;
}

static vlc_tick_t vlc_clock_master_to_system_locked(vlc_clock_t *clock,
                                                    vlc_tick_t now,
                                                    vlc_tick_t ts, double rate)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_tick_t system = main_stream_to_system(main_clock, ts);
    if (system == VLC_TICK_INVALID)
    {
        /* We don't have a master sync point, let's fallback to a monotonic ref
         * point */
        system = vlc_clock_monotonic_to_system_locked(clock, now, ts, rate);
    }

    return system + clock->delay * rate;
}

static vlc_tick_t vlc_clock_slave_update(vlc_clock_t *clock,
                                         vlc_tick_t system_now,
                                         vlc_tick_t ts, double rate,
                                         unsigned frame_rate,
                                         unsigned frame_rate_base)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);

    /* If system_now is INT64_MAX, the update is forced, don't modify anything
     * but only notify the new clock point. */
    vlc_tick_t computed = system_now == INT64_MAX ? INT64_MAX
                        : clock->to_system_locked(clock, system_now, ts, rate);

    vlc_mutex_unlock(&main_clock->lock);

    vlc_clock_on_update(clock, computed, ts, rate, frame_rate, frame_rate_base);
    return computed != INT64_MAX ? computed - system_now : INT64_MAX;
}

static void vlc_clock_slave_reset(vlc_clock_t *clock)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);
    main_clock->wait_sync_ref_priority = UINT_MAX;
    main_clock->wait_sync_ref =
        clock_point_Create(VLC_TICK_INVALID, VLC_TICK_INVALID);

    vlc_mutex_unlock(&main_clock->lock);

    vlc_clock_on_update(clock, VLC_TICK_INVALID, VLC_TICK_INVALID, 1.0f, 0, 0);
}

static vlc_tick_t vlc_clock_slave_set_delay(vlc_clock_t *clock, vlc_tick_t delay)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);

    clock->delay = delay;

    vlc_cond_broadcast(&main_clock->cond);
    vlc_mutex_unlock(&main_clock->lock);
    return 0;
}

int vlc_clock_Wait(vlc_clock_t *clock, vlc_tick_t system_now, vlc_tick_t ts,
                   double rate, vlc_tick_t max_duration)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);
    const vlc_tick_t max_deadline =
        max_duration > 0 ? system_now + max_duration : INT64_MAX;
    while (!main_clock->abort)
    {
        vlc_tick_t deadline;
        if (main_clock->pause_date != VLC_TICK_INVALID)
            deadline = INT64_MAX;
        else
            deadline = clock->to_system_locked(clock, system_now, ts, rate);
        deadline = __MIN(deadline, max_deadline);

        if (vlc_cond_timedwait(&main_clock->cond, &main_clock->lock, deadline))
        {
            vlc_mutex_unlock(&main_clock->lock);
            return 0;
        }
    }
    vlc_mutex_unlock(&main_clock->lock);
    return -1;
}

vlc_clock_main_t *vlc_clock_main_New(void)
{
    vlc_clock_main_t *main_clock = malloc(sizeof(vlc_clock_main_t));

    if (main_clock == NULL)
        return NULL;

    vlc_mutex_init(&main_clock->lock);
    vlc_cond_init(&main_clock->cond);
    main_clock->master = NULL;
    main_clock->rc = 1;

    main_clock->coeff = 1.0f;
    main_clock->rate = 1.0f;
    main_clock->offset = VLC_TICK_INVALID;
    main_clock->delay = 0;

    main_clock->first_pcr =
        clock_point_Create(VLC_TICK_INVALID, VLC_TICK_INVALID);
    main_clock->wait_sync_ref_priority = UINT_MAX;
    main_clock->wait_sync_ref = main_clock->last =
        clock_point_Create(VLC_TICK_INVALID, VLC_TICK_INVALID);

    main_clock->pause_date = VLC_TICK_INVALID;
    main_clock->input_dejitter = DEFAULT_PTS_DELAY;
    main_clock->output_dejitter = AOUT_MAX_PTS_ADVANCE * 2;
    main_clock->abort = false;

    AvgInit(&main_clock->coeff_avg, 10);

    return main_clock;
}

void vlc_clock_main_Abort(vlc_clock_main_t *main_clock)
{
    vlc_mutex_lock(&main_clock->lock);
    main_clock->abort = true;
    vlc_cond_broadcast(&main_clock->cond);

    vlc_mutex_unlock(&main_clock->lock);
}

void vlc_clock_main_Reset(vlc_clock_main_t *main_clock)
{
    vlc_mutex_lock(&main_clock->lock);
    vlc_clock_main_reset(main_clock);
    main_clock->first_pcr =
        clock_point_Create(VLC_TICK_INVALID, VLC_TICK_INVALID);
    vlc_mutex_unlock(&main_clock->lock);
}

void vlc_clock_main_SetFirstPcr(vlc_clock_main_t *main_clock,
                                vlc_tick_t system_now, vlc_tick_t ts)
{
    vlc_mutex_lock(&main_clock->lock);
    if (main_clock->first_pcr.system == VLC_TICK_INVALID)
    {
        main_clock->first_pcr = clock_point_Create(system_now, ts);
        main_clock->wait_sync_ref_priority = UINT_MAX;
        main_clock->wait_sync_ref =
            clock_point_Create(VLC_TICK_INVALID, VLC_TICK_INVALID);
    }
    vlc_mutex_unlock(&main_clock->lock);
}

void vlc_clock_main_SetInputDejitter(vlc_clock_main_t *main_clock,
                                     vlc_tick_t delay)
{
    vlc_mutex_lock(&main_clock->lock);
    main_clock->input_dejitter = delay;
    vlc_mutex_unlock(&main_clock->lock);
}

void vlc_clock_main_SetDejitter(vlc_clock_main_t *main_clock,
                                vlc_tick_t dejitter)
{
    vlc_mutex_lock(&main_clock->lock);
    main_clock->output_dejitter = dejitter;
    vlc_mutex_unlock(&main_clock->lock);
}

void vlc_clock_main_ChangePause(vlc_clock_main_t *main_clock, vlc_tick_t now,
                                bool paused)
{
    vlc_mutex_lock(&main_clock->lock);
    assert(paused == (main_clock->pause_date == VLC_TICK_INVALID));

    if (paused)
        main_clock->pause_date = now;
    else
    {
        /**
         * Only apply a delay if the clock has a reference point to avoid
         * messing up the timings if the stream was paused then seeked
         */
        const vlc_tick_t delay = now - main_clock->pause_date;
        if (main_clock->offset != VLC_TICK_INVALID)
        {
            main_clock->last.system += delay;
            main_clock->offset += delay;
        }
        if (main_clock->first_pcr.system != VLC_TICK_INVALID)
            main_clock->first_pcr.system += delay;
        if (main_clock->wait_sync_ref.system != VLC_TICK_INVALID)
            main_clock->wait_sync_ref.system += delay;
        main_clock->pause_date = VLC_TICK_INVALID;
        vlc_cond_broadcast(&main_clock->cond);
    }
    vlc_mutex_unlock(&main_clock->lock);
}

void vlc_clock_main_Delete(vlc_clock_main_t *main_clock)
{
    assert(main_clock->rc == 1);
    free(main_clock);
}

vlc_tick_t vlc_clock_Update(vlc_clock_t *clock, vlc_tick_t system_now,
                            vlc_tick_t ts, double rate)
{
    return clock->update(clock, system_now, ts, rate, 0, 0);
}

vlc_tick_t vlc_clock_UpdateVideo(vlc_clock_t *clock, vlc_tick_t system_now,
                                 vlc_tick_t ts, double rate,
                                 unsigned frame_rate, unsigned frame_rate_base)
{
    return clock->update(clock, system_now, ts, rate, frame_rate, frame_rate_base);
}

void vlc_clock_Reset(vlc_clock_t *clock)
{
    clock->reset(clock);
}

vlc_tick_t vlc_clock_SetDelay(vlc_clock_t *clock, vlc_tick_t delay)
{
    return clock->set_delay(clock, delay);
}

vlc_tick_t vlc_clock_ConvertToSystem(vlc_clock_t *clock, vlc_tick_t system_now,
                                     vlc_tick_t ts, double rate)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);
    vlc_tick_t system = clock->to_system_locked(clock, system_now, ts, rate);
    vlc_mutex_unlock(&main_clock->lock);
    return system;
}

void vlc_clock_ConvertArrayToSystem(vlc_clock_t *clock, vlc_tick_t system_now,
                                    vlc_tick_t *ts_array, size_t ts_count,
                                    double rate)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);
    for (size_t i = 0; i < ts_count; ++i)
        ts_array[i] = clock->to_system_locked(clock, system_now, ts_array[i],
                                              rate);
    vlc_mutex_unlock(&main_clock->lock);
}

static void vlc_clock_set_master_callbacks(vlc_clock_t *clock)
{
    clock->update = vlc_clock_master_update;
    clock->reset = vlc_clock_master_reset;
    clock->set_delay = vlc_clock_master_set_delay;
    clock->to_system_locked = vlc_clock_master_to_system_locked;
}

static void vlc_clock_set_slave_callbacks(vlc_clock_t *clock)
{
    clock->update = vlc_clock_slave_update;
    clock->reset = vlc_clock_slave_reset;
    clock->set_delay = vlc_clock_slave_set_delay;
    clock->to_system_locked = vlc_clock_slave_to_system_locked;
}

static vlc_clock_t *vlc_clock_main_Create(vlc_clock_main_t *main_clock,
                                          unsigned priority,
                                          const struct vlc_clock_cbs *cbs,
                                          void *cbs_data)
{
    vlc_clock_t *clock = malloc(sizeof(vlc_clock_t));
    if (clock == NULL)
        return NULL;

    clock->owner = main_clock;
    clock->delay = 0;
    clock->cbs = cbs;
    clock->cbs_data = cbs_data;
    clock->priority = priority;
    assert(!cbs || cbs->on_update);

    return clock;
}

vlc_clock_t *vlc_clock_main_CreateMaster(vlc_clock_main_t *main_clock,
                                         const struct vlc_clock_cbs *cbs,
                                         void *cbs_data)
{
    /* The master has always the 0 priority */
    vlc_clock_t *clock = vlc_clock_main_Create(main_clock, 0, cbs, cbs_data);
    if (!clock)
        return NULL;

    vlc_mutex_lock(&main_clock->lock);
    if (main_clock->master != NULL)
    {
        vlc_clock_master_reset(main_clock->master);
        vlc_clock_set_slave_callbacks(main_clock->master);
    }
    vlc_clock_set_master_callbacks(clock);
    main_clock->master = clock;
    main_clock->rc++;
    vlc_mutex_unlock(&main_clock->lock);

    return clock;
}

vlc_clock_t *vlc_clock_main_CreateSlave(vlc_clock_main_t *main_clock,
                                        enum es_format_category_e cat,
                                        const struct vlc_clock_cbs *cbs,
                                        void *cbs_data)
{
    /* SPU outputs should have lower priority than VIDEO outputs since they
     * necessarily depend on a VIDEO output. This mean that a SPU reference
     * point will always be overridden by AUDIO or VIDEO outputs. Cf.
     * vlc_clock_monotonic_to_system_locked */
    unsigned priority;
    switch (cat)
    {
        case VIDEO_ES:
        case AUDIO_ES:
            priority = 1;
            break;
        case SPU_ES:
        default:
            priority = 2;
            break;
    }

    vlc_clock_t *clock = vlc_clock_main_Create(main_clock, priority, cbs,
                                               cbs_data);
    if (!clock)
        return NULL;

    vlc_mutex_lock(&main_clock->lock);
    vlc_clock_set_slave_callbacks(clock);
    main_clock->rc++;
    vlc_mutex_unlock(&main_clock->lock);

    return clock;
}

vlc_clock_t *vlc_clock_CreateSlave(const vlc_clock_t *clock,
                                   enum es_format_category_e cat)
{
    return vlc_clock_main_CreateSlave(clock->owner, cat, NULL, NULL);
}

void vlc_clock_main_SetMaster(vlc_clock_main_t *main_clock, vlc_clock_t *clock)
{
    vlc_mutex_lock(&main_clock->lock);
    if (main_clock->master != NULL)
    {
        vlc_clock_master_reset(main_clock->master);
        vlc_clock_set_slave_callbacks(main_clock->master);
    }
    vlc_clock_set_master_callbacks(clock);
    main_clock->master = clock;
    vlc_mutex_unlock(&main_clock->lock);
}


void vlc_clock_Delete(vlc_clock_t *clock)
{
    vlc_clock_main_t *main_clock = clock->owner;
    vlc_mutex_lock(&main_clock->lock);
    if (clock == main_clock->master)
    {
        vlc_clock_main_reset(main_clock);
        main_clock->master = NULL;
    }
    main_clock->rc--;
    vlc_mutex_unlock(&main_clock->lock);
    free(clock);
}
