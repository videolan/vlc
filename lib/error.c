/*****************************************************************************
 * error.c: Error handling for libvlc
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#include "libvlc_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <vlc/libvlc.h>


static const char oom[] = "Out of memory";
/* TODO: use only one thread-specific key for whole libvlc */
static vlc_threadvar_t context;

static vlc_mutex_t lock = VLC_STATIC_MUTEX;
static uintptr_t refs = 0;

static void free_msg (void *msg)
{
    if (msg != oom)
        free (msg);
}

void libvlc_threads_init (void)
{
    vlc_mutex_lock (&lock);
    if (refs++ == 0)
        vlc_threadvar_create (&context, free_msg);
    vlc_mutex_unlock (&lock);
}

void libvlc_threads_deinit (void)
{
    vlc_mutex_lock (&lock);
    assert (refs > 0);
    if (--refs == 0)
        vlc_threadvar_delete (&context);
    vlc_mutex_unlock (&lock);
}

static char *get_error (void)
{
    return vlc_threadvar_get (context);
}

static void free_error (void)
{
    free_msg (get_error ());
}

/**
 * Gets a human-readable error message for the last LibVLC error in the calling
 * thread. The resulting string is valid until another error occurs (at least
 * until the next LibVLC call).
 *
 * @return NULL if there was no error, a nul-terminated string otherwise.
 */
const char *libvlc_errmsg (void)
{
    return get_error ();
}

/**
 * Clears the LibVLC error status for the current thread. This is optional.
 * By default, the error status is automatically overridden when a new error
 * occurs, and destroyed when the thread exits.
 */
void libvlc_clearerr (void)
{
    free_error ();
    vlc_threadvar_set (context, NULL);
}

/**
 * Sets the LibVLC error status and message for the current thread.
 * Any previous error is overridden.
 * @return a nul terminated string (always)
 */
const char *libvlc_vprinterr (const char *fmt, va_list ap)
{
    char *msg;

    assert (fmt != NULL);
    if (vasprintf (&msg, fmt, ap) == -1)
        msg = (char *)oom;

    free_error ();
    vlc_threadvar_set (context, msg);
    return msg;
}

/**
 * Sets the LibVLC error status and message for the current thread.
 * Any previous error is overridden.
 * @return a nul terminated string (always)
 */
const char *libvlc_printerr (const char *fmt, ...)
{
    va_list ap;
    const char *msg;

    va_start (ap, fmt);
    msg = libvlc_vprinterr (fmt, ap);
    va_end (ap);
    return msg;
}

