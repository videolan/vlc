/*****************************************************************************
 * libvlc_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _LIBVLC_INTERNAL_H
#define _LIBVLC_INTERNAL_H 1

#include <vlc/libvlc.h>
#include <vlc/libvlc_dialog.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_events.h>

#include <vlc_atomic.h>
#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_threads.h>

typedef struct vlc_preparser_t vlc_preparser_t;
typedef struct vlc_preparser_t vlc_preparser_t;

/* Note well: this header is included from LibVLC core.
 * Therefore, static inline functions MUST NOT call LibVLC functions here
 * (this can cause linkage failure on some platforms). */

/***************************************************************************
 * Internal creation and destruction functions
 ***************************************************************************/
VLC_API libvlc_int_t *libvlc_InternalCreate( void );
VLC_API int libvlc_InternalInit( libvlc_int_t *, int, const char *ppsz_argv[] );
VLC_API void libvlc_InternalCleanup( libvlc_int_t * );
VLC_API void libvlc_InternalDestroy( libvlc_int_t * );

/**
 * Try to start a user interface for the libvlc instance.
 *
 * \param libvlcint the internal instance
 * \param name interface name, or NULL for default
 * \return 0 on success, -1 on error.
 */
VLC_API int libvlc_InternalAddIntf( libvlc_int_t *libvlcint, const char *name );

/**
 * Start playing the main playlist
 *
 * The main playlist can only be populated via an interface created by the
 * libvlc_InternalAddIntf() function. The control and media flow will only be
 * controlled by the interface previously added.
 *
 * One of these 2 functions (libvlc_InternalAddIntf() or libvlc_InternalPlay())
 * will trigger the creation of an internal playlist and player.
 *
 * \param libvlcint the internal instance
 */
VLC_API void libvlc_InternalPlay( libvlc_int_t *libvlcint );
VLC_API void libvlc_InternalWait( libvlc_int_t * );

/**
 * Registers a callback for the LibVLC exit event. This is mostly useful if
 * the VLC playlist and/or at least one interface are started with
 * libvlc_InternalPlay() or libvlc_InternalAddIntf () respectively.
 * Typically, this function will wake up your application main loop (from
 * another thread).
 *
 * \note This function should be called before the playlist or interface are
 * started. Otherwise, there is a small race condition: the exit event could
 * be raised before the handler is registered.
 *
 * \param libvlcint the internal instance
 * \param cb callback to invoke when LibVLC wants to exit,
 *           or NULL to disable the exit handler (as by default)
 * \param opaque data pointer for the callback
 */
VLC_API void libvlc_SetExitHandler( libvlc_int_t *libvlcint, void (*cb) (void *),
                                    void *opaque );

/***************************************************************************
 * Opaque structures for libvlc API
 ***************************************************************************/

struct libvlc_instance_t
{
    libvlc_int_t *p_libvlc_int;
    vlc_atomic_rc_t ref_count;
    struct libvlc_callback_entry_list_t *p_callback_list;

    vlc_mutex_t lazy_init_lock;
    vlc_preparser_t *parser;
    vlc_preparser_t *thumbnailer;

    struct
    {
        void (*cb) (void *, int, const libvlc_log_t *, const char *, va_list);
        void *data;
    } log;
    struct
    {
        libvlc_dialog_cbs cbs;
        void *data;
    } dialog;
};

struct libvlc_event_manager_t
{
    void * p_obj;
    vlc_array_t listeners;
    vlc_mutex_t lock;
};

/***************************************************************************
 * Other internal functions
 ***************************************************************************/

/* Thread context */
void libvlc_threads_init (void);
void libvlc_threads_deinit (void);

/* Events */
void libvlc_event_manager_init(libvlc_event_manager_t *, void *);
void libvlc_event_manager_destroy(libvlc_event_manager_t *);

void libvlc_event_send(
        libvlc_event_manager_t * p_em,
        libvlc_event_t * p_event );

static inline libvlc_time_t libvlc_time_from_vlc_tick(vlc_tick_t time)
{
    return MS_FROM_VLC_TICK(time + VLC_TICK_FROM_US(500));
}

static inline vlc_tick_t vlc_tick_from_libvlc_time(libvlc_time_t time)
{
    return VLC_TICK_FROM_MS(time);
}

vlc_preparser_t *libvlc_get_preparser(libvlc_instance_t *instance);
vlc_preparser_t *libvlc_get_thumbnailer(libvlc_instance_t *instance);

#endif
