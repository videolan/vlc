/*****************************************************************************
 * vlc_messages.h: messages interface
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 VLC authors and VideoLAN
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

#ifndef VLC_MESSAGES_H_
#define VLC_MESSAGES_H_

#include <stdarg.h>

/**
 * \defgroup messages Logging
 * \ingroup os
 * \brief Message logs
 *
 * Functions for modules to emit log messages.
 *
 * @{
 * \file
 * Logging functions
 */

/** Message types */
enum vlc_log_type
{
    VLC_MSG_INFO=0, /**< Important information */
    VLC_MSG_ERR,    /**< Error */
    VLC_MSG_WARN,   /**< Warning */
    VLC_MSG_DBG,    /**< Debug */
};

/**
 * Log message
 */
typedef struct vlc_log_t
{
    uintptr_t   i_object_id; /**< Emitter (temporarily) unique object ID or 0 */
    const char *psz_object_type; /**< Emitter object type name */
    const char *psz_module; /**< Emitter module (source code) */
    const char *psz_header; /**< Additional header (used by VLM media) */
    const char *file; /**< Source code file name or NULL */
    int line; /**< Source code file line number or -1 */
    const char *func; /**< Source code calling function name or NULL */
    unsigned long tid; /**< Emitter thread ID */
} vlc_log_t;

/**
 * Emit a log message.
 *
 * \param obj VLC object emitting the message or NULL
 * \param type VLC_MSG_* message type (info, error, warning or debug)
 * \param module name of module from which the message come
 *               (normally vlc_module_name)
 * \param file source module file name (normally __FILE__) or NULL
 * \param line function call source line number (normally __LINE__) or 0
 * \param func calling function name (normally __func__) or NULL
 * \param format printf-like message format
 */
VLC_API void vlc_object_Log(vlc_object_t *obj, int type, const char *module,
                            const char *file, unsigned line, const char *func,
                            const char *format, ...) VLC_FORMAT(7, 8);

/**
 * Emit a log message.
 *
 * This function is the variable argument list equivalent to vlc_object_Log().
 */
VLC_API void vlc_object_vaLog(vlc_object_t *obj, int prio, const char *module,
                             const char *file, unsigned line, const char *func,
                              const char *format, va_list ap);

#define msg_GenericVa(o, p, fmt, ap) \
    vlc_object_vaLog(VLC_OBJECT(o), p, vlc_module_name, __FILE__, __LINE__, \
                     __func__, fmt, ap)

#define msg_Generic(o, p, ...) \
    vlc_object_Log(VLC_OBJECT(o), p, vlc_module_name, __FILE__, __LINE__, \
                   __func__, __VA_ARGS__)
#define msg_Info(p_this, ...) \
    msg_Generic(p_this, VLC_MSG_INFO, __VA_ARGS__)
#define msg_Err(p_this, ...) \
    msg_Generic(p_this, VLC_MSG_ERR, __VA_ARGS__)
#define msg_Warn(p_this, ...) \
    msg_Generic(p_this, VLC_MSG_WARN, __VA_ARGS__)
#define msg_Dbg(p_this, ...) \
    msg_Generic(p_this, VLC_MSG_DBG, __VA_ARGS__)

#ifndef __cplusplus
extern const char vlc_module_name[];
#else
extern "C" const char vlc_module_name[];
#endif

VLC_API const char *vlc_strerror(int);
VLC_API const char *vlc_strerror_c(int);

/**
 * \defgroup logger Logger
 * \brief Message log back-end.
 *
 * @{
 */

struct vlc_logger;

VLC_API void vlc_Log(struct vlc_logger *const *logger, int prio,
                     const char *type, const char *module,
                     const char *file, unsigned line, const char *func,
                     const char *format, ...) VLC_FORMAT(8, 9);
VLC_API void vlc_vaLog(struct vlc_logger *const *logger, int prio,
                       const char *type, const char *module,
                       const char *file, unsigned line, const char *func,
                       const char *format, va_list ap);

#define vlc_log_gen(logger, prio, ...) \
        vlc_Log(&(logger), prio, "generic", vlc_module_name, \
                __FILE__, __LINE__, __func__, __VA_ARGS__)
#define vlc_info(logger, ...)    vlc_log_gen(logger, VLC_MSG_INFO, __VA_ARGS__)
#define vlc_error(logger, ...)   vlc_log_gen(logger, VLC_MSG_ERR,  __VA_ARGS__)
#define vlc_warning(logger, ...) vlc_log_gen(logger, VLC_MSG_WARN, __VA_ARGS__)
#define vlc_debug(logger, ...)   vlc_log_gen(logger, VLC_MSG_DBG,  __VA_ARGS__)

/**
 * Message logging callback signature.
 * \param data data pointer as provided to vlc_LogSet().
 * \param type message type (VLC_MSG_* values from enum vlc_log_type)
 * \param item meta information
 * \param fmt format string
 * \param args format string arguments
 */
typedef void (*vlc_log_cb) (void *data, int type, const vlc_log_t *item,
                            const char *fmt, va_list args);

struct vlc_logger_operations
{
    vlc_log_cb log;
    void (*destroy)(void *data);
};

/**
 * Creates a prefixed message log.
 *
 * This creates a message log that prefixes all its messages and forwards them
 * in another log.
 * \param parent message log to inject into
 * \param str nul-terminated prefix (a.k.a. "header")
 * \return a new message log on success or @c NULL on error
 */
VLC_API struct vlc_logger *vlc_LogHeaderCreate(struct vlc_logger *parent,
                                               const char *str) VLC_USED;

/**
 * Destroys a message log.
 */
VLC_API void vlc_LogDestroy(struct vlc_logger *);

/**
 * @}
 */
/**
 * @}
 */
#endif
