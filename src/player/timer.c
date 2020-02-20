/*****************************************************************************
 * player.c: Player interface
 *****************************************************************************
 * Copyright © 2018 VLC authors and VideoLAN
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

#include <limits.h>

#include "player.h"

void
vlc_player_ResetTimer(vlc_player_t *player)
{
    vlc_mutex_lock(&player->timer.lock);

    player->timer.state = VLC_PLAYER_TIMER_STATE_DISCONTINUITY;
    player->timer.input_length = VLC_TICK_INVALID;
    player->timer.input_normal_time = VLC_TICK_0;
    player->timer.last_ts = VLC_TICK_INVALID;
    player->timer.input_position = 0.f;
    player->timer.smpte_source.smpte.last_framenum = ULONG_MAX;

    vlc_mutex_unlock(&player->timer.lock);
}

static void
vlc_player_SendTimerSourceUpdates(vlc_player_t *player,
                                  struct vlc_player_timer_source *source,
                                  bool force_update,
                                  const struct vlc_player_timer_point *point)
{
    (void) player;
    vlc_player_timer_id *timer;

    vlc_list_foreach(timer, &source->listeners, node)
    {
        /* Respect refresh delay of the timer */
        if (force_update || timer->period == VLC_TICK_INVALID
         || timer->last_update_date == VLC_TICK_INVALID
         || point->system_date == INT64_MAX /* always update when paused */
         || point->system_date - timer->last_update_date >= timer->period)
        {
            timer->cbs->on_update(point, timer->data);
            timer->last_update_date = point->system_date == INT64_MAX ?
                                      VLC_TICK_INVALID : point->system_date;
        }
    }
}

static void
vlc_player_SendSmpteTimerSourceUpdates(vlc_player_t *player,
                                       struct vlc_player_timer_source *source,
                                       const struct vlc_player_timer_point *point)
{
    (void) player;
    vlc_player_timer_id *timer;

    struct vlc_player_timer_smpte_timecode tc;
    unsigned long framenum;
    unsigned frame_rate;
    unsigned frame_rate_base;

    if (source->smpte.df > 0)
    {
        /* Use the exact SMPTE framerate that can be different from the input
         * source (at demuxer/decoder level) */
        assert(source->smpte.df_fps == 30 || source->smpte.df_fps == 60);
        frame_rate = source->smpte.df_fps * 1000;
        frame_rate_base = 1001;

        /* Convert the ts to a frame number */
        framenum = round(point->ts * frame_rate
                         / (double) frame_rate_base / VLC_TICK_FROM_SEC(1));

        /* Drop 2 or 4 frames every minutes except every 10 minutes in order to
         * make one hour of timecode match one hour on the clock. */
        ldiv_t res;
        res = ldiv(framenum, source->smpte.frames_per_10mins);

        framenum += (9 * source->smpte.df * res.quot)
                  + (source->smpte.df * ((res.rem - source->smpte.df)
                     / (source->smpte.frames_per_10mins / 10)));

        tc.drop_frame = true;

        /* Use 30 or 60 framerates for the next frames/seconds/minutes/hours
         * calculaton */
        frame_rate = source->smpte.df_fps;
        frame_rate_base = 1;
    }
    else
    {
        frame_rate = source->smpte.frame_rate;
        frame_rate_base = source->smpte.frame_rate_base;

        /* Convert the ts to a frame number */
        framenum = round(point->ts * frame_rate
                         / (double) frame_rate_base / VLC_TICK_FROM_SEC(1));

        tc.drop_frame = false;
    }
    if (framenum == source->smpte.last_framenum)
        return;

    source->smpte.last_framenum = framenum;

    tc.frames = framenum % (frame_rate / frame_rate_base);
    tc.seconds = (framenum * frame_rate_base / frame_rate) % 60;
    tc.minutes = (framenum * frame_rate_base / frame_rate / 60) % 60;
    tc.hours = framenum * frame_rate_base / frame_rate / 3600;

    tc.frame_resolution = source->smpte.frame_resolution;

    vlc_list_foreach(timer, &source->listeners, node)
        timer->smpte_cbs->on_update(&tc, timer->data);
}

static void
vlc_player_UpdateSmpteTimerFPS(vlc_player_t *player,
                               struct vlc_player_timer_source *source,
                               unsigned frame_rate, unsigned frame_rate_base)
{
    (void) player;
    source->smpte.frame_rate = frame_rate;
    source->smpte.frame_rate_base = frame_rate_base;

    /* Calculate everything that will be needed to create smpte timecodes */
    source->smpte.frame_resolution = 0;

    unsigned max_frames = frame_rate / frame_rate_base;

    if (max_frames == 29 && (100 * frame_rate / frame_rate_base) == 2997)
    {
        /* SMPTE Timecode: 29.97 fps DF */
        source->smpte.df = 2;
        source->smpte.df_fps = 30;
        source->smpte.frames_per_10mins = 17982; /* 29.97 * 60 * 10 */
    }
    else if (max_frames == 59 && (100 * frame_rate / frame_rate_base) == 5994)
    {
        /* SMPTE Timecode: 59.94 fps DF */
        source->smpte.df = 4;
        source->smpte.df_fps = 60;
        source->smpte.frames_per_10mins = 35964; /* 59.94 * 60 * 10 */
    }
    else
        source->smpte.df = 0;

    while (max_frames != 0)
    {
        max_frames /= 10;
        source->smpte.frame_resolution++;
    }
}

void
vlc_player_UpdateTimerState(vlc_player_t *player, vlc_es_id_t *es_source,
                            enum vlc_player_timer_state state,
                            vlc_tick_t system_date)
{
    vlc_mutex_lock(&player->timer.lock);

    if (state == VLC_PLAYER_TIMER_STATE_DISCONTINUITY)
    {
        assert(system_date == VLC_TICK_INVALID);

        /* Discontinuity is signalled by all output clocks and the input.
         * discard the event if it was already signalled or not on the good
         * es_source. */
        bool signal_discontinuity = false;
        for (size_t i = 0; i < VLC_PLAYER_TIMER_TYPE_COUNT; ++i)
        {
            struct vlc_player_timer_source *source = &player->timer.sources[i];
            if (source->es == es_source
             && source->point.system_date != VLC_TICK_INVALID)
            {
                source->point.system_date = VLC_TICK_INVALID;
                /* signal discontinuity only on best source */
                if (i == VLC_PLAYER_TIMER_TYPE_BEST)
                    signal_discontinuity = true;
            }
        }
        if (!signal_discontinuity)
        {
            vlc_mutex_unlock(&player->timer.lock);
            return;
        }
    }
    else
    {
        assert(state == VLC_PLAYER_TIMER_STATE_PAUSED);
        assert(system_date != VLC_TICK_INVALID);
    }

    player->timer.state = state;

    struct vlc_player_timer_source *source = &player->timer.best_source;
    vlc_player_timer_id *timer;
    vlc_list_foreach(timer, &source->listeners, node)
    {
        timer->last_update_date = VLC_TICK_INVALID;
        timer->cbs->on_discontinuity(system_date, timer->data);
    }

    vlc_mutex_unlock(&player->timer.lock);
}

static void
vlc_player_UpdateTimerSource(vlc_player_t *player,
                             struct vlc_player_timer_source *source,
                             double rate, vlc_tick_t ts, vlc_tick_t system_date)
{
    assert(ts >= VLC_TICK_0);
    assert(player->timer.input_normal_time >= VLC_TICK_0);

    source->point.rate = rate;
    source->point.ts = ts - player->timer.input_normal_time + VLC_TICK_0;
    source->point.length = player->timer.input_length;

    /* Put an invalid date for the first point in order to disable
     * interpolation (behave as paused), indeed, we should wait for one more
     * point before starting interpolation (ideally, it should be more) */
    if (source->point.system_date == VLC_TICK_INVALID)
        source->point.system_date = INT64_MAX;
    else
        source->point.system_date = system_date;

    if (source->point.length != VLC_TICK_INVALID)
        source->point.position = (ts - player->timer.input_normal_time)
                               / (double) source->point.length;
    else
        source->point.position = player->timer.input_position;
}

void
vlc_player_UpdateTimer(vlc_player_t *player, vlc_es_id_t *es_source,
                       bool es_source_is_master,
                       const struct vlc_player_timer_point *point,
                       vlc_tick_t normal_time,
                       unsigned frame_rate, unsigned frame_rate_base)
{
    struct vlc_player_timer_source *source;
    assert(point);
    /* A null source can't be the master */
    assert(es_source == NULL ? !es_source_is_master : true);

    vlc_mutex_lock(&player->timer.lock);

    bool force_update = false;
    if (!es_source) /* input source */
    {
        /* Only valid for input sources */
        if (player->timer.input_normal_time != normal_time)
        {
            player->timer.input_normal_time = normal_time;
            player->timer.last_ts = VLC_TICK_INVALID;
            force_update = true;
        }
        if (player->timer.input_length != point->length)
        {
            player->timer.input_length = point->length;
            player->timer.last_ts = VLC_TICK_INVALID;
            force_update = true;
        }
        /* Will likely be overridden by non input source */
        player->timer.input_position = point->position;

        if (point->ts == VLC_TICK_INVALID
         || point->system_date == VLC_TICK_INVALID)
        {
            /* ts can only be invalid from the input source */
            vlc_mutex_unlock(&player->timer.lock);
            return;
        }
    }

    assert(point->ts != VLC_TICK_INVALID);

    /* An update after a discontinuity means that the playback is resumed */
    if (player->timer.state == VLC_PLAYER_TIMER_STATE_DISCONTINUITY)
        player->timer.state = VLC_PLAYER_TIMER_STATE_PLAYING;

    /* Best source priority:
     * 1/ es_source != NULL when paused (any ES tracks when paused. Indeed,
     * there is likely no audio update (master) when paused but only video
     * ones, via vlc_player_NextVideoFrame() for example)
     * 2/ es_source != NULL + master (from the master ES track)
     * 3/ es_source != NULL (from the first ES track updated)
     * 4/ es_source == NULL (from the input)
     */
    source = &player->timer.best_source;
    if (!source->es || es_source_is_master
     || (es_source && player->timer.state == VLC_PLAYER_TIMER_STATE_PAUSED))
        source->es = es_source;

    /* Notify the best source */
    if (source->es == es_source)
    {
        if (source->point.rate != point->rate)
        {
            player->timer.last_ts = VLC_TICK_INVALID;
            force_update = true;
        }

        /* When paused (INT64_MAX), the same ts can be send more than one time
         * from the video source, only send it if different in that case. */
        if (point->ts != player->timer.last_ts
          || source->point.system_date != point->system_date
          || point->system_date != INT64_MAX)
        {
            vlc_player_UpdateTimerSource(player, source, point->rate, point->ts,
                                         point->system_date);

            if (!vlc_list_is_empty(&source->listeners))
                vlc_player_SendTimerSourceUpdates(player, source, force_update,
                                                  &source->point);
        }
    }

    source = &player->timer.smpte_source;
    /* SMPTE source: only the video source */
    if (!source->es && es_source && vlc_es_id_GetCat(es_source) == VIDEO_ES)
        source->es = es_source;

    /* Notify the SMPTE source, also notify when the video output was rendered
     * while the clock was paused */
    if (source->es == es_source && source->es)
    {
        if (frame_rate != 0 && (frame_rate != source->smpte.frame_rate
         || frame_rate_base != source->smpte.frame_rate_base))
        {
            assert(frame_rate_base != 0);
            player->timer.last_ts = VLC_TICK_INVALID;
            vlc_player_UpdateSmpteTimerFPS(player, source, frame_rate,
                                           frame_rate_base);
        }

        if (point->ts != player->timer.last_ts && source->smpte.frame_rate != 0)
        {
            vlc_player_UpdateTimerSource(player, source, point->rate, point->ts,
                                         point->system_date);

            if (!vlc_list_is_empty(&source->listeners))
                vlc_player_SendSmpteTimerSourceUpdates(player, source,
                                                       &source->point);
        }
    }

    player->timer.last_ts = point->ts;

    vlc_mutex_unlock(&player->timer.lock);
}

void
vlc_player_RemoveTimerSource(vlc_player_t *player, vlc_es_id_t *es_source)
{
    vlc_mutex_lock(&player->timer.lock);
    for (size_t i = 0; i < VLC_PLAYER_TIMER_TYPE_COUNT; ++i)
    {
        struct vlc_player_timer_source *source = &player->timer.sources[i];
        if (source->es == es_source)
        {
            /* Discontinuity should have been already signaled */
            assert(source->point.system_date == VLC_TICK_INVALID);
            source->es = NULL;
        }
    }
    vlc_mutex_unlock(&player->timer.lock);
}

int
vlc_player_GetTimerPoint(vlc_player_t *player, vlc_tick_t system_now,
                         vlc_tick_t *out_ts, float *out_pos)
{
    vlc_mutex_lock(&player->timer.lock);
    if (player->timer.best_source.point.system_date == VLC_TICK_INVALID)
    {
        vlc_mutex_unlock(&player->timer.lock);
        return VLC_EGENERIC;
    }
    int ret =
        vlc_player_timer_point_Interpolate(&player->timer.best_source.point,
                                           system_now, out_ts, out_pos);

    vlc_mutex_unlock(&player->timer.lock);
    return ret;
}

vlc_player_timer_id *
vlc_player_AddTimer(vlc_player_t *player, vlc_tick_t min_period,
                    const struct vlc_player_timer_cbs *cbs, void *data)
{
    assert(min_period >= VLC_TICK_0 || min_period == VLC_TICK_INVALID);
    assert(cbs && cbs->on_update);

    struct vlc_player_timer_id *timer = malloc(sizeof(*timer));
    if (!timer)
        return NULL;
    timer->period = min_period;
    timer->last_update_date = VLC_TICK_INVALID;
    timer->cbs = cbs;
    timer->data = data;

    vlc_mutex_lock(&player->timer.lock);
    vlc_list_append(&timer->node, &player->timer.best_source.listeners);
    vlc_mutex_unlock(&player->timer.lock);

    return timer;
}

vlc_player_timer_id *
vlc_player_AddSmpteTimer(vlc_player_t *player,
                         const struct vlc_player_timer_smpte_cbs *cbs,
                         void *data)
{
    assert(cbs && cbs->on_update);

    struct vlc_player_timer_id *timer = malloc(sizeof(*timer));
    if (!timer)
        return NULL;
    timer->period = VLC_TICK_INVALID;
    timer->last_update_date = VLC_TICK_INVALID;
    timer->smpte_cbs = cbs;
    timer->data = data;

    vlc_mutex_lock(&player->timer.lock);
    vlc_list_append(&timer->node, &player->timer.smpte_source.listeners);
    vlc_mutex_unlock(&player->timer.lock);

    return timer;
}

void
vlc_player_RemoveTimer(vlc_player_t *player, vlc_player_timer_id *timer)
{
    assert(timer);

    vlc_mutex_lock(&player->timer.lock);
    vlc_list_remove(&timer->node);
    vlc_mutex_unlock(&player->timer.lock);

    free(timer);
}

int
vlc_player_timer_point_Interpolate(const struct vlc_player_timer_point *point,
                                   vlc_tick_t system_now,
                                   vlc_tick_t *out_ts, float *out_pos)
{
    assert(point);
    assert(system_now > 0);
    assert(out_ts || out_pos);

    /* A system_date == INT64_MAX means the clock was paused when it updated
     * this point, so there is nothing to interpolate */
    const vlc_tick_t drift = point->system_date == INT64_MAX ? 0
                           : (system_now - point->system_date) * point->rate;
    vlc_tick_t ts = point->ts;
    float pos = point->position;

    if (ts != VLC_TICK_INVALID)
    {
        ts += drift;
        if (unlikely(ts < VLC_TICK_0))
            return VLC_EGENERIC;
    }
    if (point->length != VLC_TICK_INVALID)
    {
        pos += drift / (float) point->length;
        if (unlikely(pos < 0.f))
            return VLC_EGENERIC;
        if (pos > 1.f)
            pos = 1.f;
        if (ts > point->length)
            ts = point->length;
    }

    if (out_ts)
        *out_ts = ts;
    if (out_pos)
        *out_pos = pos;

    return VLC_SUCCESS;
}

vlc_tick_t
vlc_player_timer_point_GetNextIntervalDate(const struct vlc_player_timer_point *point,
                                           vlc_tick_t system_now,
                                           vlc_tick_t interpolated_ts,
                                           vlc_tick_t next_interval)
{
    assert(point);
    assert(system_now > 0);
    assert(next_interval != VLC_TICK_INVALID);

    const unsigned ts_interval = interpolated_ts / next_interval;
    const vlc_tick_t ts_next_interval =
        ts_interval * next_interval + next_interval;

    return (ts_next_interval - interpolated_ts) / point->rate + system_now;
}

void
vlc_player_InitTimer(vlc_player_t *player)
{
    vlc_mutex_init(&player->timer.lock);

    for (size_t i = 0; i < VLC_PLAYER_TIMER_TYPE_COUNT; ++i)
    {
        vlc_list_init(&player->timer.sources[i].listeners);
        player->timer.sources[i].point.system_date = VLC_TICK_INVALID;
        player->timer.sources[i].es = NULL;
    }
    vlc_player_ResetTimer(player);
}

void
vlc_player_DestroyTimer(vlc_player_t *player)
{
    for (size_t i = 0; i < VLC_PLAYER_TIMER_TYPE_COUNT; ++i)
        assert(vlc_list_is_empty(&player->timer.sources[i].listeners));
}
