/*****************************************************************************
 * tracer.c: test for the tracer libvlc API
 *****************************************************************************
 * Copyright (C) 2022 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_libvlc_tracer
#define MODULE_STRING "test_libvlc_tracer"
#undef __PLUGIN__

#include "libvlc/test.h"
#include "../../lib/libvlc_internal.h"
#include <vlc_common.h>
#include <vlc_tracer.h>

/**
 * Trace message values
 */
enum libvlc_tracer_value
{
    LIBVLC_TRACER_INT,
    LIBVLC_TRACER_TICK,
    LIBVLC_TRACER_STRING
};

typedef union
{
    int64_t integer;
    vlc_tick_t tick;
    const char *string;
} libvlc_tracer_value_t;

/**
 * Trace message
 */
typedef struct libvlc_tracer_entry_t
{
    const char *key;            /**< Key to identify the value */
    libvlc_tracer_value_t value;   /**< Trace value */
    enum libvlc_tracer_value type; /**< Type of the value */
} libvlc_tracer_entry_t;



struct tracer_test
{
    libvlc_time_t ts;
    libvlc_tracer_entry_t *entries;
};



static void trace(void *opaque, libvlc_time_t ts, va_list entries)
{
    struct tracer_test *test = opaque;
    assert(test->ts == ts);

    libvlc_tracer_entry_t *next_entry = test->entries;
    while (next_entry != NULL && next_entry->key != NULL)
    {
        fprintf(stderr, "- entry: %s\n", next_entry->key);
        libvlc_tracer_entry_t entry = va_arg(entries, libvlc_tracer_entry_t );
        assert(entry.key != NULL);
        assert(strcmp(entry.key, next_entry->key) == 0);
        assert(entry.type == next_entry->type);
        switch(entry.type)
        {
#define ENTRY(type, field) \
            case type: \
                assert(entry.value.field == next_entry->value.field); \
                break
            ENTRY(LIBVLC_TRACER_INT, integer);
            ENTRY(LIBVLC_TRACER_TICK, tick);
            ENTRY(LIBVLC_TRACER_STRING, string);
            default:
                vlc_assert_unreachable();
        }
        next_entry++;
    }

    libvlc_tracer_entry_t entry = va_arg(entries, libvlc_tracer_entry_t );
    assert(entry.key == NULL);
}

int main( int argc, char **argv )
{
    (void)argc; (void)argv;
    test_init();

    const char * const vlc_argv[] = {
        "-vvv", "--aout=dummy", "--text-renderer=dummy",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(vlc_argv), vlc_argv);
    vlc_object_t *root = &vlc->p_libvlc_int->obj;
    struct vlc_tracer *tracer;

    /* Check that original offset is correct. */
    struct tracer_test test1 = {
        .ts = 0,
    };
    libvlc_trace_set(vlc, trace, &test1);
    tracer = vlc_object_get_tracer(root);
    vlc_tracer_TraceWithTs(tracer, 0, VLC_TRACE_END);

    /* Check that we don't always return the same value. */
    struct tracer_test test2 = {
        .ts = 1,
    };
    libvlc_trace_set(vlc, trace, &test2);
    tracer = vlc_object_get_tracer(root);
    vlc_tracer_TraceWithTs(tracer, 1, VLC_TRACE_END);

    /* Check that unsetting the tracer works correctly. */
    /* This should be a different value than below. */
    libvlc_trace_unset(vlc);
    assert(vlc_object_get_tracer(root) == NULL);

    /* Check that the keys are forwarded. */
    struct tracer_test test4 = {
        .ts = 1,
        .entries = (libvlc_tracer_entry_t []){
            { .key = "test", .value = { .string = "test4" }, .type = LIBVLC_TRACER_STRING },
            { NULL, {0}, 0 },
        }
    };
    libvlc_trace_set(vlc, trace, &test4);
    tracer = vlc_object_get_tracer(root);
    vlc_tracer_TraceWithTs(tracer, 1, VLC_TRACE("test", "test4"), VLC_TRACE_END);

    libvlc_release(vlc);
    return 0;
}
