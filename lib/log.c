/*****************************************************************************
 * log.c: libvlc new API log functions
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 *
 * $Id$
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
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
#include <vlc/libvlc.h>
#include "libvlc_internal.h"
#include <vlc_common.h>
#include <vlc_interface.h>

/*** Logging core dispatcher ***/

void libvlc_log_get_context(const libvlc_log_t *ctx,
                            const char **restrict module,
                            const char **restrict file,
                            unsigned *restrict line)
{
    if (module != NULL)
        *module = ctx->psz_module;
    if (file != NULL)
        *file = NULL;
    if (line != NULL)
        *line = 0;
}

void libvlc_log_get_object(const libvlc_log_t *ctx,
                           const char **restrict name,
                           const char **restrict header,
                           uintptr_t *restrict id)
{
    if (name != NULL)
        *name = (ctx->psz_object_type != NULL)
                ? ctx->psz_object_type : "generic";
    if (header != NULL)
        *header = ctx->psz_header;
    if (id != NULL)
        *id = ctx->i_object_id;
}

static void libvlc_logf (void *data, int level, const vlc_log_t *item,
                         const char *fmt, va_list ap)
{
    libvlc_instance_t *inst = data;

    switch (level)
    {
        case VLC_MSG_INFO: level = LIBVLC_NOTICE;  break;
        case VLC_MSG_ERR:  level = LIBVLC_ERROR;   break;
        case VLC_MSG_WARN: level = LIBVLC_WARNING; break;
        case VLC_MSG_DBG:  level = LIBVLC_DEBUG;   break;
    }

    inst->log.cb (inst->log.data, level, item, fmt, ap);
}

void libvlc_log_unset (libvlc_instance_t *inst)
{
    vlc_LogSet (inst->p_libvlc_int, NULL, NULL);
}

void libvlc_log_set (libvlc_instance_t *inst, libvlc_log_cb cb, void *data)
{
    libvlc_log_unset (inst); /* <- Barrier before modifying the callback */
    inst->log.cb = cb;
    inst->log.data = data;
    vlc_LogSet (inst->p_libvlc_int, libvlc_logf, inst);
}

/*** Helpers for logging to files ***/
static void libvlc_log_file (void *data, int level, const libvlc_log_t *log,
                             const char *fmt, va_list ap)
{
    FILE *stream = data;

    flockfile (stream);
    vfprintf (stream, fmt, ap);
    fputc ('\n', stream);
    funlockfile (stream);
    (void) level; (void) log;
}

void libvlc_log_set_file (libvlc_instance_t *inst, FILE *stream)
{
    libvlc_log_set (inst, libvlc_log_file, stream);
}

/*** Stubs for the old interface ***/
unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance )
{
    (void) p_instance;
    return -1;
}

void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level )
{
    (void) p_instance;
    (void) level;
}

libvlc_log_t *libvlc_log_open( libvlc_instance_t *p_instance )
{
    (void) p_instance;
    return malloc(1);
}

void libvlc_log_close( libvlc_log_t *p_log )
{
    free(p_log);
}

unsigned libvlc_log_count( const libvlc_log_t *p_log )
{
    (void) p_log;
    return 0;
}

void libvlc_log_clear( libvlc_log_t *p_log )
{
    (void) p_log;
}

libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *p_log )
{
    return (p_log != NULL) ? malloc(1) : NULL;
}

void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter )
{
    free( p_iter );
}

int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter )
{
    (void) p_iter;
    return 0;
}

libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                libvlc_log_message_t *buffer )
{
    (void) p_iter; (void) buffer;
    return NULL;
}
