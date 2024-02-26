/*****************************************************************************
 * clock/clock_start.c: test for the vlc clock
 *****************************************************************************
 * Copyright (C) 2024 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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
#include <vlc_tick.h>
#include <vlc_es.h>
#include <vlc_tracer.h>

#include "../../../src/clock/clock.h"

#include <vlc/vlc.h>
#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"

#define MODULE_NAME test_clock_clock
#undef VLC_DYNAMIC_PLUGIN

#include <vlc_plugin.h>
#include <vlc_vector.h>

/* Define a builtin module for mocked parts */
const char vlc_module_name[] = MODULE_STRING;

/**
 * Fixture structure containing the clocks used for each test.
 */
struct clock_fixture_simple {
    vlc_clock_main_t *main;
    vlc_clock_t *input;
    vlc_clock_t *master;
    vlc_clock_t *slave;
};

struct clock_point
{
    vlc_tick_t system;
    vlc_tick_t stream;
};

/**
 * Initialize the fixture.
 *
 * It must be released using \ref destroy_fixture_simple.
 */
static struct clock_fixture_simple
init_fixture_simple(struct vlc_logger *logger,
                    bool is_input_master,
                    vlc_tick_t input_dejitter,
                    vlc_tick_t output_dejitter)
{
    struct clock_fixture_simple f = {
        .main = vlc_clock_main_New(logger, NULL),
    };
    assert(f.main != NULL);

    vlc_clock_main_Lock(f.main);
    vlc_clock_main_SetInputDejitter(f.main, input_dejitter);
    vlc_clock_main_SetDejitter(f.main, output_dejitter);

    if (is_input_master)
    {
        f.input = vlc_clock_main_CreateInputMaster(f.main);
        f.master = f.input;
    }
    else
    {
        f.input = vlc_clock_main_CreateInputSlave(f.main);
        f.master = vlc_clock_main_CreateMaster(f.main, "Driving", NULL, NULL);
    }
    assert(f.input != NULL);
    assert(f.master != NULL);

    f.slave = vlc_clock_main_CreateSlave(f.main, "Output", AUDIO_ES,
                                         NULL, NULL);
    assert(f.slave != NULL);
    vlc_clock_main_Unlock(f.main);

    return f;
}

static void destroy_fixture_simple(struct clock_fixture_simple *f)
{
    vlc_clock_Delete(f->slave);
    if (f->master != f->input)
        vlc_clock_Delete(f->master);
    vlc_clock_Delete(f->input);
    vlc_clock_main_Delete(f->main);
}

/**
 * Check that conversion doesn't work as long as clock has not
 * been started.
 **/
#if 0
static void test_clock_without_start(struct vlc_logger *logger)
{
    struct clock_fixture_simple f = init_fixture_simple(logger, true, 0, 0);

    vlc_clock_main_Lock(f.main);
    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);
    {
        vlc_clock_Update(f.master, now, now * 1000, 1.);
        vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, now, now * 1000, 1., NULL);
        //assert(result == VLC_TICK_INVALID);
    }

    vlc_clock_Start(f.slave, now, now * 1000);

    {
        vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, now, now * 1000, 1., NULL);
        assert(result == VLC_TICK_INVALID);
    }
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
}
#endif

/**
 * Check that conversion works and is valid after the clock start.
 **/
static void test_clock_with_start(struct vlc_logger *logger)
{
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, true, 0, 0);
    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);

    vlc_clock_main_Lock(f.main);
    vlc_clock_Update(f.master, now, now * 1000, 1.);
    vlc_clock_Start(f.input, now, now * 1000);
    vlc_clock_Start(f.slave, now, now * 1000);

    vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, now, now * 1000, 1., NULL);
    assert(result != VLC_TICK_INVALID);
    assert(result == now);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
}

/**
 * Check that master clock can start.
 **/
static void test_clock_with_start_master(struct vlc_logger *logger)
{
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, false, 0, 0);
    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);

    vlc_clock_main_Lock(f.main);
    vlc_clock_Start(f.input, now, now * 1000);
    vlc_clock_Start(f.master, now, now * 1000);
    vlc_clock_Start(f.slave, now, now * 1000);
    vlc_clock_Update(f.master, now, now * 1000, 1.);

    vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, now, now * 1000, 1., NULL);
    assert(result != VLC_TICK_INVALID);
    assert(result == now);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
}

/**
 * Check that conversion works and is valid after the clock start,
 * when start is done later.
 **/
static void test_clock_with_output_dejitter(struct vlc_logger *logger)
{
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, true, 0, 0);
    vlc_clock_main_Lock(f.main);

    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);
    vlc_clock_Update(f.master, now, now * 1000, 1.);
    vlc_clock_Start(f.input, 2 * now, now * 1000);
    vlc_clock_Start(f.slave, 2 * now, now * 1000);

    vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, now, now * 1000, 1., NULL);
    fprintf(stderr, "%s:%d: now=%" PRId64 " converted=%" PRId64 "\n",
            __func__, __LINE__, now, result);
    assert(result != VLC_TICK_INVALID);
    assert(result == 2 * now);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
}

/**
 * Check that clock start is working correctly with an output driving
 * the clock bus.
 **/
static void test_clock_with_output_clock_start(struct vlc_logger *logger)
{
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, false, 0, 0);
    vlc_clock_main_Lock(f.main);

    /* When the input is not driving the clock bus, we should be able to start
     * without any reference point since the output will provide it later. */
    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);
    vlc_clock_Start(f.input, now, now * 1000);
    vlc_clock_Start(f.slave, now, now * 1000);

    /* We provide the clock point to the clock bus. The system time should
     * approximately match the start date. */
    vlc_clock_Update(f.master, now, now * 1000, 1.);

    vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, now, now * 1000, 1., NULL);
    assert(result != VLC_TICK_INVALID);
    assert(result == now);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
}

/**
 * Check that monotonic clock start is working correctly.
 **/
static void test_clock_with_monotonic_clock(struct vlc_logger *logger)
{
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, false, 0, 0);
    vlc_clock_main_Lock(f.main);

    /* When the input is not driving the clock bus, we should be able to start
     * without any reference point since the output will provide it later. */
    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);
    vlc_clock_Start(f.input, now, now * 1000);

    /* The output wants to start before a reference point is found. */
    vlc_clock_Start(f.slave, now, now * 1000);

    // TODO
    vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, now, now * 1000, 1., NULL);
    assert(result != VLC_TICK_INVALID);
    assert(result == now);
    vlc_clock_main_Unlock(f.main);


    destroy_fixture_simple(&f);
}

/**
 * Check that monotonic clock start accounts for the start date.
 **/
static vlc_tick_t test_clock_with_monotonic_clock_start(
    struct vlc_logger *logger,
    vlc_tick_t input_dejitter,
    vlc_tick_t output_dejitter,
    struct clock_point start_point,
    struct clock_point output_point,
    struct clock_point convert_point
){
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, false, input_dejitter, output_dejitter);
    vlc_clock_main_Lock(f.main);

    /* When the input is not driving the clock bus, we should be able to start
     * without any reference point since the output will provide it later. */
    vlc_clock_Start(f.input, start_point.system, start_point.stream);

    /* The output wants to start before a reference point is found. */
    vlc_clock_Start(f.slave, output_point.system, output_point.stream);

    vlc_tick_t result = vlc_clock_ConvertToSystem(f.slave, convert_point.system, convert_point.stream, 1., NULL);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
    return result;
}

static vlc_tick_t test_clock_slave_update(
    struct vlc_logger *logger,
    struct clock_point start_point,
    struct clock_point output_point
){
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, false, 0, 0);
    vlc_clock_main_Lock(f.main);

    /* When the input is not driving the clock bus, we should be able to start
     * without any reference point since the output will provide it later. */
    vlc_clock_Start(f.input, start_point.system, start_point.stream);
    vlc_clock_Update(f.master, start_point.system, start_point.stream, 1.);

    /* The output wants to start before a reference point is found. */
    vlc_clock_Start(f.slave, output_point.system, output_point.stream);

    vlc_tick_t result = vlc_clock_Update(f.slave, output_point.system, output_point.stream, 1.);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
    return result;
}

static void test_clock_start_reset(
    struct vlc_logger *logger
){
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, false, 0, 0);
    vlc_clock_main_Lock(f.main);

    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);
    struct clock_point start_point;
    start_point = (struct clock_point){ now, now * 1000 };

    /* When the input is not driving the clock bus, we should be able to start
     * without any reference point since the output will provide it later. */
    vlc_clock_Start(f.input, start_point.system, start_point.stream);
    vlc_clock_Update(f.master, start_point.system, start_point.stream, 1.);
    vlc_clock_main_Reset(f.main);

    vlc_tick_t result;
    result = vlc_clock_ConvertToSystem(f.slave, start_point.system, start_point.stream, 1., NULL);
    //assert(result == VLC_TICK_INVALID);

    start_point.system *= 2;
    vlc_clock_Start(f.input, start_point.system, start_point.stream);
    vlc_clock_Update(f.master, start_point.system, start_point.stream, 1.);
    result = vlc_clock_ConvertToSystem(f.slave, start_point.system, start_point.stream, 1., NULL);
    assert(result == start_point.system);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
}

static void test_clock_slave_only(
    struct vlc_logger *logger
){
    fprintf(stderr, "%s:\n", __func__);
    struct clock_fixture_simple f = init_fixture_simple(logger, false, 0, 0);
    vlc_clock_main_Lock(f.main);

    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);
    struct clock_point start_point = { now, now * 1000 };

    /* When the input is not driving the clock bus, we should be able to start
     * without any reference point since the output will provide it later. */
    vlc_clock_Start(f.input, start_point.system, start_point.stream);

    vlc_clock_Start(f.slave, start_point.system, start_point.stream);
    vlc_tick_t result;
    result = vlc_clock_ConvertToSystem(f.slave, start_point.system, start_point.stream, 1., NULL);
    assert(result == start_point.system);

    result = vlc_clock_ConvertToSystem(f.slave, start_point.system, start_point.stream + 1000, 1., NULL);
    assert(result == start_point.system + 1000);
    vlc_clock_main_Unlock(f.main);

    destroy_fixture_simple(&f);
}
int main(void)
{
    libvlc_instance_t *libvlc = libvlc_new(0, NULL);
    assert(libvlc != NULL);

    libvlc_int_t *vlc = libvlc->p_libvlc_int;
    struct vlc_logger *logger = vlc->obj.logger;

    const vlc_tick_t now = VLC_TICK_FROM_MS(1000);

    //test_clock_without_start(logger);
    test_clock_with_start(logger);
    test_clock_with_start_master(logger);
    test_clock_with_output_dejitter(logger);
    test_clock_with_output_clock_start(logger);
    test_clock_with_monotonic_clock(logger);

    struct clock_point start_point, output_point;
    start_point = (struct clock_point){ now, now * 1000 };
    output_point = (struct clock_point){ now, now * 1000 };

    vlc_tick_t result = test_clock_with_monotonic_clock_start(
        logger, 0, 0, start_point, start_point, start_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, start_point.system);
    assert(result == start_point.system);

    /* Monotonic clock must start at the start point. */
    start_point = (struct clock_point){ 2 * now, now * 1000 };
    result = test_clock_with_monotonic_clock_start(
        logger, 0, 0, start_point, output_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, start_point.system);
    assert(result == start_point.system);

    /* A point later in the future will be started later also. */
    start_point = (struct clock_point){ now, now * 1000 };
    output_point = (struct clock_point){ now, now * 1000 + 100};
    result = test_clock_with_monotonic_clock_start(
        logger, 0, 0, start_point, output_point, output_point);
    fprintf(stderr, "result(%" PRId64 ") = start_point.system(%" PRId64 ")\n", result, start_point.system + 100);
    assert(result == start_point.system + 100);

    /* Input dejitter and PCR delay are accounted together. */
    result = test_clock_with_monotonic_clock_start(
        logger, 30, 0, start_point, output_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, start_point.system + 130);
    assert(result == start_point.system + 130);

    /* Output dejitter will not account the current PCR delay. */
    result = test_clock_with_monotonic_clock_start(
        logger, 0, 100, start_point, output_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, start_point.system + 100);
    assert(result == start_point.system + 100);

    /* The highest dejitter value is used. */
    result = test_clock_with_monotonic_clock_start(
        logger, 33, 55, start_point, output_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, start_point.system + 133);
    assert(result == start_point.system + 133);

    /* The highest dejitter value is used. */
    result = test_clock_with_monotonic_clock_start(
        logger, 33, 555, start_point, output_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, start_point.system + 555);
    assert(result == start_point.system + 555);

    start_point = (struct clock_point){ now, now * 1000 };
    output_point = (struct clock_point){ now, now * 1000 };
    result = test_clock_slave_update(logger, start_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, (vlc_tick_t)0);
    assert(result == 0);

    output_point = (struct clock_point){ now + 10, now * 1000 };
    result = test_clock_slave_update(logger, start_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, (vlc_tick_t)-10);
    assert(result == -10);

    output_point = (struct clock_point){ now, now * 1000 + 10 };
    result = test_clock_slave_update(logger, start_point, output_point);
    fprintf(stderr, "%" PRId64 " = %" PRId64 "\n", result, (vlc_tick_t)10);
    assert(result == 10);

    test_clock_start_reset(logger);
    test_clock_slave_only(logger);

    libvlc_release(libvlc);
}

