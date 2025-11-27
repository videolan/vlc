// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * common.h: common helpers for the player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include <vlc_common.h>
#include <vlc_player.h>

struct report_timer
{
    enum
    {
        REPORT_TIMER_POINT,
        REPORT_TIMER_TC,
        REPORT_TIMER_PAUSED,
    } type;
    union
    {
        struct vlc_player_timer_point point;
        struct vlc_player_timer_smpte_timecode tc;
        vlc_tick_t paused_date;
    };
};
typedef struct VLC_VECTOR(struct report_timer) vec_report_timer;

struct timer_state
{
    vlc_player_timer_id *id;
    vlc_tick_t delay;
    vec_report_timer vec;
};

static inline void
timers_on_update(const struct vlc_player_timer_point *point, void *data)
{
    struct timer_state *timer = data;
    struct report_timer report =
    {
        .type = REPORT_TIMER_POINT,
        .point = *point,
    };
    bool success = vlc_vector_push(&timer->vec, report);
    assert(success);
}

static inline void
timers_on_paused(vlc_tick_t system_date, void *data)
{
    struct timer_state *timer = data;
    struct report_timer report =
    {
        .type = REPORT_TIMER_PAUSED,
        .paused_date = system_date,
    };
    bool success = vlc_vector_push(&timer->vec, report);
    assert(success);
}

static inline void
timers_smpte_on_update(const struct vlc_player_timer_smpte_timecode *tc,
                       void *data)
{
    struct timer_state *timer = data;
    struct report_timer report =
    {
        .type = REPORT_TIMER_TC,
        .tc = *tc,
    };
    bool success = vlc_vector_push(&timer->vec, report);
    assert(success);
}