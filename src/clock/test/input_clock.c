/*****************************************************************************
 * input_clock.c: tests for the input buffering code
 *****************************************************************************
 * Copyright (C) 2023-2025 Alexandre Janniaux <ajanni@videolabs.io>
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
#include <vlc_modules.h>
#include <vlc_messages.h>
#include <vlc_tick.h>

#include <vlc/libvlc.h>

#include "../../../src/clock/input_clock.h"
#include "../../../src/clock/clock.h"
#include "../../../src/clock/clock_internal.h"

const char vlc_module_name[] = "test_input_clock";

static void LoggerLog(void *data, int type, const vlc_log_t *item,
        const char *fmt, va_list args)
{
    (void)data; (void)type; (void)item;
    vfprintf(stderr, fmt, args);
}

static const struct vlc_logger_operations logger_ops =
{
    .log = LoggerLog,
};
static struct vlc_logger logger = { &logger_ops };

static void test_clock_update(void)
{
    input_clock_t *clock = input_clock_New(&logger, 1.f);
    assert(clock != NULL);

    {
        vlc_tick_t system_now = VLC_TICK_0;
        vlc_tick_t system_duration = input_clock_GetSystemDuration(clock, system_now);
        assert(system_duration == 0);
    }

    bool can_pace_control = true;
    bool buffering_allowed = true;
    vlc_tick_t date_stream = VLC_TICK_0;
    vlc_tick_t date_system = VLC_TICK_0;
    input_clock_Update(clock, can_pace_control, buffering_allowed, false, date_stream, date_system);

    vlc_tick_t stream_duration, system_duration;
    input_clock_GetBufferingDuration(clock, &stream_duration, &system_duration);
    fprintf(stderr, "system_duration=%" PRId64 " stream_duration=%" PRId64 "\n",
            system_duration, stream_duration);
    assert(stream_duration == 0);
    assert(system_duration == 0);

    vlc_tick_t system_now = VLC_TICK_0;
    system_duration = input_clock_GetSystemDuration(clock, system_now);
    assert(system_duration == 0);

    /* Reset drift */
    input_clock_Reset(clock);
    input_clock_Delete(clock);
}

static void clock_update_abort(
    vlc_tick_t system_ts, vlc_tick_t ts, double rate,
    unsigned frame_rate, unsigned frame_rate_base, void *data)
{
    (void)system_ts; (void)ts; (void)rate;
    (void)frame_rate; (void)frame_rate_base;
    (void)data;
    assert(!"No clock update should happen");
}

static const struct vlc_clock_cbs clock_abort_ops =
{
    .on_update = clock_update_abort,
};

struct clock_validate {
    bool validate;
    struct {
        vlc_tick_t system;
        vlc_tick_t stream;
    } expected;
};

static void clock_update_validate(
    vlc_tick_t system_ts, vlc_tick_t ts, double rate,
    unsigned frame_rate, unsigned frame_rate_base, void *data)
{
    (void)system_ts; (void)ts; (void)rate;
    (void)frame_rate; (void)frame_rate_base;

    struct clock_validate *validate = data;
    fprintf(stderr, "%s: system=%" PRId64 " stream=%" PRId64
            ", checking against system=%"PRId64 " stream=%" PRId64 "\n",
            __func__, system_ts, ts, validate->expected.system, validate->expected.stream);
    validate->validate = true;
    assert(validate->expected.system == system_ts);
    assert(validate->expected.stream == ts);
}
static const struct vlc_clock_cbs clock_validate_ops =
{
    .on_update = clock_update_validate,
};

struct clock_listener_switch {
    const struct vlc_clock_cbs *cbs;
    void *opaque;
};

static void clock_update_switch(
    vlc_tick_t system_ts, vlc_tick_t ts, double rate,
    unsigned frame_rate, unsigned frame_rate_base, void *data)
{
    struct clock_listener_switch *listener = data;
    listener->cbs->on_update(system_ts, ts, rate, frame_rate, frame_rate_base, listener->opaque);
}

static const struct vlc_clock_cbs clock_switch_ops = {
    .on_update = clock_update_switch,
};



int main(void)
{
    fprintf(stderr, "test_clock_update:\n");
    test_clock_update();
    return 0;
}
