/*****************************************************************************
 * libvlc_logger.h:  libvlc_logger external API
 *****************************************************************************
 * Copyright (C) 2020 Videolabs
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

#ifndef VLC_LIBVLC_LOGGER_H
#define VLC_LIBVLC_LOGGER_H 1

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

typedef struct libvlc_logger_t libvlc_logger_t;
typedef struct vlc_log_t libvlc_log_t;

/**
 * Gets log message debug infos.
 *
 * This function retrieves self-debug information about a log message:
 * - the name of the VLC module emitting the message,
 * - the name of the source code module (i.e. file) and
 * - the line number within the source code module.
 *
 * The returned module name and file name will be NULL if unknown.
 * The returned line number will similarly be zero if unknown.
 *
 * \param ctx message context (as passed to the @ref libvlc_log_cb callback)
 * \param module module name storage (or NULL) [OUT]
 * \param file source code file name storage (or NULL) [OUT]
 * \param line source code file line number storage (or NULL) [OUT]
 * \warning The returned module name and source code file name, if non-NULL,
 * are only valid until the logging callback returns.
 *
 * \version LibVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_log_get_context(const libvlc_log_t *ctx,
                       const char **module, const char **file, unsigned *line);

/**
 * Gets log message info.
 *
 * This function retrieves meta-information about a log message:
 * - the type name of the VLC object emitting the message,
 * - the object header if any, and
 * - a temporaly-unique object identifier.
 *
 * This information is mainly meant for <b>manual</b> troubleshooting.
 *
 * The returned type name may be "generic" if unknown, but it cannot be NULL.
 * The returned header will be NULL if unset; in current versions, the header
 * is used to distinguish for VLM inputs.
 * The returned object ID will be zero if the message is not associated with
 * any VLC object.
 *
 * \param ctx message context (as passed to the @ref libvlc_log_cb callback)
 * \param name object name storage (or NULL) [OUT]
 * \param header object header (or NULL) [OUT]
 * \param line source code file line number storage (or NULL) [OUT]
 * \warning The returned module name and source code file name, if non-NULL,
 * are only valid until the logging callback returns.
 *
 * \version LibVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_log_get_object(const libvlc_log_t *ctx,
                        const char **name, const char **header, uintptr_t *id);


/**
 * Callback prototype for LibVLC log message handler.
 *
 * \param data data pointer as given to libvlc_log_set()
 * \param level message level (@ref libvlc_log_level)
 * \param ctx message context (meta-information about the message)
 * \param fmt printf() format string (as defined by ISO C11)
 * \param args variable argument list for the format
 * \note Log message handlers <b>must</b> be thread-safe.
 * \warning The message context pointer, the format string parameters and the
 *          variable arguments are only valid until the callback returns.
 */

typedef void (*libvlc_log_cb)(void *data, int level, const libvlc_log_t *ctx,
                              const char *fmt, va_list args);

LIBVLC_API libvlc_logger_t *
libvlc_logger_new_callback( libvlc_log_cb log_cb, void *data );

LIBVLC_API void
libvlc_logger_release( libvlc_logger_t *logger );

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_PLAYER_H */
