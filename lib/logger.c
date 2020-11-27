/*****************************************************************************
 * logger.c: Libvlc API logger functions
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_messages.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_logger.h>
#include "logger_internal.h"


struct libvlc_logger_callback
{
    libvlc_logger_t logger;

    /* Private data */
    void         *opaque;
    libvlc_log_cb callback;
};

static void callback_logger_log(
        void *data, int type, const vlc_log_t *item, const char *fmt,
        va_list args )
{
    libvlc_logger_t *logger = data;

    struct libvlc_logger_callback *impl =
        container_of(logger, struct libvlc_logger_callback, logger);
    impl->callback(impl->opaque, type, item, fmt, args);
}

static void callback_logger_destroy(void *data)
{
    libvlc_logger_t *logger = data;

    struct libvlc_logger_callback *impl =
        container_of(logger, struct libvlc_logger_callback, logger);
    free(impl);
}

struct vlc_logger_operations logger_callback_ops =
{
    .log = callback_logger_log,
    .destroy = callback_logger_destroy,
};

libvlc_logger_t * libvlc_logger_new_callback( libvlc_log_cb log_cb, void *data )
{
    struct libvlc_logger_callback *impl = malloc(sizeof *impl);
    if (!impl)
        return NULL;
    impl->opaque = data;
    impl->callback = log_cb;
    impl->logger.internal.ops = &logger_callback_ops;
    return &impl->logger;
}

void libvlc_logger_release( libvlc_logger_t *logger )
{
    vlc_LogDestroy( &logger->internal );
}
