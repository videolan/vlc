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

static vlc_rwlock_t log_lock = VLC_STATIC_RWLOCK;
static libvlc_log_subscriber_t *log_first = NULL;
static msg_subscription_t sub;

VLC_FORMAT(2,3)
static void libvlc_log (int level, const char *fmt, ...)
{
    libvlc_log_subscriber_t *sub;
    va_list ap;

    switch (level)
    {
        case VLC_MSG_INFO: level = LIBVLC_NOTICE;  break;
        case VLC_MSG_ERR:  level = LIBVLC_ERROR;   break;
        case VLC_MSG_WARN: level = LIBVLC_WARNING; break;
        case VLC_MSG_DBG:  level = LIBVLC_DEBUG;   break;
    }

    va_start (ap, fmt);
    vlc_rwlock_rdlock (&log_lock);
    for (sub = log_first; sub != NULL; sub = sub->next)
        sub->func (sub->opaque, level, fmt, ap);
    vlc_rwlock_unlock (&log_lock);
    va_end (ap);
}

static void libvlc_logf (void *dummy, int level, const vlc_log_t *item,
                         const char *fmt, va_list ap)
{
    char *msg;

    if (unlikely(vasprintf (&msg, fmt, ap) == -1))
        msg = NULL;
    if (item->psz_header != NULL)
        libvlc_log (level, "[%p] [%s]: %s %s %s", (void *)item->i_object_id,
                    item->psz_header, item->psz_module, item->psz_object_type,
                    msg ? msg : "Not enough memory");
    else
        libvlc_log (level, "[%p]: %s %s %s", (void *)item->i_object_id,
                    item->psz_module, item->psz_object_type,
                    msg ? msg : "Not enough memory");
    free (msg);
    (void) dummy;
}

void libvlc_log_init (void)
{
    vlc_Subscribe (&sub, libvlc_logf, NULL);
}

void libvlc_log_deinit (void)
{
    vlc_Unsubscribe (&sub);
}

void libvlc_log_subscribe (libvlc_log_subscriber_t *sub,
                           libvlc_log_cb cb, void *data)
{
    sub->prev = NULL;
    sub->func = cb;
    sub->opaque = data;
    vlc_rwlock_wrlock (&log_lock);
    sub->next = log_first;
    log_first = sub;
    vlc_rwlock_unlock (&log_lock);
}

void libvlc_log_unsubscribe( libvlc_log_subscriber_t *sub )
{
    vlc_rwlock_wrlock (&log_lock);
    if (sub->next != NULL)
        sub->next->prev = sub->prev;
    if (sub->prev != NULL)
        sub->prev->next = sub->next;
    else
        log_first = sub->next;
    vlc_rwlock_unlock (&log_lock);
}

/*** Helpers for logging to files ***/
static void libvlc_log_file (void *data, int level, const char *fmt,
                             va_list ap)
{
    FILE *stream = data;

    flockfile (stream);
    vfprintf (stream, fmt, ap);
    fputc ('\n', stream);
    funlockfile (stream);
    (void) level;
}

void libvlc_log_subscribe_file (libvlc_log_subscriber_t *sub, FILE *stream)
{
    libvlc_log_subscribe (sub, libvlc_log_file, stream);
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
