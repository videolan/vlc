/*****************************************************************************
 * vlc_tracer.h: tracing interface
 * This library provides basic functions for threads to interact with user
 * interface, such as trace output.
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef VLC_TRACES_H
#define VLC_TRACES_H

#include <stdarg.h>

#include <vlc_common.h>
#include <vlc_threads.h>

/**
 * \defgroup tracer Tracer module and API
 * \ingroup os
 *
 * Functions for modules to emit traces.
 *
 * @{
 * \file
 * Tracing functions
 *
 * \defgroup tracer_module Tracer Module implementation
 * \ingroup tracer
 *
 * @{
 */

/**
 * Trace message values
 */
enum vlc_tracer_value
{
    VLC_TRACER_INT,
    VLC_TRACER_DOUBLE,
    VLC_TRACER_STRING
};

typedef union
{
    int64_t integer;
    double double_;
    const char *string;
} vlc_tracer_value_t;

/**
 * Trace message
 */
struct vlc_tracer_entry
{
    const char *key;            /**< Key to identify the value */
    vlc_tracer_value_t value;   /**< Trace value */
    enum vlc_tracer_value type; /**< Type of the value */
};

struct vlc_tracer;

/**
 * Tracer operations returned by the module probe function
 */
struct vlc_tracer_operations
{
    /**
     * Called when tracing data
     *
     * \param sys data pointer set by vlc_tracer_open_cb()
     * \param ts timestamp of the trace (based on vlc_tick_now())
     * \param entries can only be \ref vlc_tracer_entry and the va-args list
     * should be ended by a \ref vlc_tracer_entry with a NULL key.
     */
    void (*trace)(void *sys, vlc_tick_t ts, va_list entries);

    /**
     * Called to clean module specific resources
     *
     * \param sys data pointer set by vlc_tracer_open_cb()
     */
    void (*destroy)(void *sys);
};

/**
 * Module probe/open function signature
 *
 * \param obj a valid object
 * \param[out] sysp to module specific data
 * \return the operations implemented by the module or NULL in case of error
 * */
typedef struct vlc_tracer_operations *(*vlc_tracer_open_cb)(vlc_object_t *obj,
                                                            void **restrict sysp);

/**
 * @}
 *
 * \defgroup tracer_api Tracer API
 * \ingroup tracer
 *
 * @{
 */

/**
 * Emit traces
 *
 * va-args are a list of key / value parameters.
 * Key must be a not NULL string.
 * Value has to be defined with one of the type defined
 * in the \ref vlc_tracer_entry union.
 * \param tracer tracer emitting the traces
 * \param ts timestamp of the current trace
 */
VLC_API void vlc_tracer_TraceWithTs(struct vlc_tracer *tracer, vlc_tick_t ts, ...);

#define vlc_tracer_Trace(tracer, ...) \
    vlc_tracer_TraceWithTs(tracer, vlc_tick_now(), __VA_ARGS__)

static inline struct vlc_tracer_entry vlc_tracer_entry_FromInt(const char *key, int64_t value)
{
    vlc_tracer_value_t tracer_value;
    tracer_value.integer = value;
    struct vlc_tracer_entry trace = { key, tracer_value, VLC_TRACER_INT };
    return trace;
}

static inline struct vlc_tracer_entry vlc_tracer_entry_FromDouble(const char *key, double value)
{
    vlc_tracer_value_t tracer_value = {
        .double_ = value,
    };
    struct vlc_tracer_entry trace = { key, tracer_value, VLC_TRACER_DOUBLE };
    return trace;
}

static inline struct vlc_tracer_entry vlc_tracer_entry_FromString(const char *key, const char *value)
{
    vlc_tracer_value_t tracer_value;
    tracer_value.string = value;
    struct vlc_tracer_entry trace = { key, tracer_value, VLC_TRACER_STRING };
    return trace;
}

#ifndef __cplusplus
#define VLC_TRACE_END \
        vlc_tracer_entry_FromString(NULL, NULL)

#define VLC_TRACE(key, value) \
        _Generic((value), \
        int64_t: vlc_tracer_entry_FromInt, \
        double: vlc_tracer_entry_FromDouble, \
        char *: vlc_tracer_entry_FromString, \
        const char *: vlc_tracer_entry_FromString) (key, value)
#else
#define VLC_TRACE_END \
        vlc_tracer_entry_FromString(nullptr, nullptr)

static inline struct vlc_tracer_entry VLC_TRACE(const char *key, int64_t value)
{
    return vlc_tracer_entry_FromInt(key, value);
}

static inline struct vlc_tracer_entry VLC_TRACE(const char *key, char *value)
{
    return vlc_tracer_entry_FromString(key, value);
}

static inline struct vlc_tracer_entry VLC_TRACE(const char *key, const char *value)
{
    return vlc_tracer_entry_FromString(key, value);
}
#endif

#define VLC_TRACE_TICK_NS(key, tick) VLC_TRACE((key), NS_FROM_VLC_TICK((tick)))

/**
 * @}
 *
 * \defgroup tracer_helper Tracer helper functions
 * \ingroup tracer
 *
 * @{
 */

static inline void vlc_tracer_TraceStreamPTS(struct vlc_tracer *tracer, const char *type,
                                const char *id, const char* stream,
                                vlc_tick_t pts)
{
    vlc_tracer_Trace(tracer, VLC_TRACE("type", type),
                             VLC_TRACE("id", id),
                             VLC_TRACE("stream", stream),
                             VLC_TRACE_TICK_NS("pts", pts),
                             VLC_TRACE_END);
}

static inline void vlc_tracer_TraceStreamDTS(struct vlc_tracer *tracer, const char *type,
                                const char *id, const char* stream,
                                vlc_tick_t pts, vlc_tick_t dts)
{
    vlc_tracer_Trace(tracer, VLC_TRACE("type", type),
                             VLC_TRACE("id", id),
                             VLC_TRACE("stream", stream),
                             VLC_TRACE_TICK_NS("pts", pts),
                             VLC_TRACE_TICK_NS("dts", dts),
                             VLC_TRACE_END);
}

static inline void vlc_tracer_TraceRender(struct vlc_tracer *tracer, const char *type,
                                const char *id, vlc_tick_t now, vlc_tick_t pts)
{
    if (now != VLC_TICK_MAX && now != VLC_TICK_INVALID)
    {
        vlc_tracer_TraceWithTs(tracer, vlc_tick_now(),
                               VLC_TRACE("type", type),
                               VLC_TRACE("id", id),
                               VLC_TRACE_TICK_NS("pts", pts),
                               VLC_TRACE_TICK_NS("render_ts", now),
                               VLC_TRACE_END);
        vlc_tracer_TraceWithTs(tracer, now,
                               VLC_TRACE("type", type),
                               VLC_TRACE("id", id),
                               VLC_TRACE_TICK_NS("render_pts", pts),
                               VLC_TRACE_END);

    }
    else
        vlc_tracer_Trace(tracer, VLC_TRACE("type", type),
                                 VLC_TRACE("id", id),
                                 VLC_TRACE_TICK_NS("pts", pts),
                                 VLC_TRACE_END);
}

static inline void vlc_tracer_TraceEvent(struct vlc_tracer *tracer, const char *type,
                                         const char *id, const char *event)
{
    vlc_tracer_Trace(tracer, VLC_TRACE("type", type),
                             VLC_TRACE("id", id),
                             VLC_TRACE("event", event),
                             VLC_TRACE_END);
}

static inline void vlc_tracer_TracePCR( struct vlc_tracer *tracer, const char *type,
                                    const char *id, vlc_tick_t pcr)
{
    vlc_tracer_Trace(tracer, VLC_TRACE("type", type),
                             VLC_TRACE("id", id),
                             VLC_TRACE_TICK_NS("pcr", pcr),
                             VLC_TRACE_END);
}

/**
 * @}
 */
/**
 * @}
 */
#endif
