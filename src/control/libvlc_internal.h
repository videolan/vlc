/*****************************************************************************
 * libvlc_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _LIBVLC_INTERNAL_H
#define _LIBVLC_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/libvlc_structures.h>
#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_events.h>

#include <vlc_common.h>

/***************************************************************************
 * Internal creation and destruction functions
 ***************************************************************************/
VLC_EXPORT (libvlc_int_t *, libvlc_InternalCreate, ( void ) );
VLC_EXPORT (int, libvlc_InternalInit, ( libvlc_int_t *, int, const char *ppsz_argv[] ) );
VLC_EXPORT (void, libvlc_InternalCleanup, ( libvlc_int_t * ) );
VLC_EXPORT (void, libvlc_InternalDestroy, ( libvlc_int_t * ) );

VLC_EXPORT (int, libvlc_InternalAddIntf, ( libvlc_int_t *, const char * ) );
VLC_EXPORT (void, libvlc_InternalWait, ( libvlc_int_t * ) );

typedef void (*libvlc_vlm_release_func_t)( libvlc_instance_t * ) ;

/***************************************************************************
 * Opaque structures for libvlc API
 ***************************************************************************/

typedef struct libvlc_vlm_t
{
    vlm_t                  *p_vlm;
    libvlc_event_manager_t *p_event_manager;
    libvlc_vlm_release_func_t pf_release;
} libvlc_vlm_t;

struct libvlc_instance_t
{
    libvlc_int_t *p_libvlc_int;
    libvlc_vlm_t  libvlc_vlm;
    unsigned      ref_count;
    int           verbosity;
    vlc_mutex_t   instance_lock;
    struct libvlc_callback_entry_list_t *p_callback_list;
};


/***************************************************************************
 * Other internal functions
 ***************************************************************************/

/* Thread context */
void libvlc_init_threads (void);
void libvlc_deinit_threads (void);

/* Events */
libvlc_event_manager_t * libvlc_event_manager_new(
        void * p_obj, libvlc_instance_t * p_libvlc_inst,
        libvlc_exception_t *p_e );

void libvlc_event_manager_release(
        libvlc_event_manager_t * p_em );

void libvlc_event_manager_register_event_type(
        libvlc_event_manager_t * p_em,
        libvlc_event_type_t event_type,
        libvlc_exception_t * p_e );

void libvlc_event_send(
        libvlc_event_manager_t * p_em,
        libvlc_event_t * p_event );

void libvlc_event_attach_async( libvlc_event_manager_t * p_event_manager,
                               libvlc_event_type_t event_type,
                               libvlc_callback_t pf_callback,
                               void *p_user_data,
                               libvlc_exception_t *p_e );

/* Exception shorcuts */

#define RAISENULL( ... ) { libvlc_printerr(__VA_ARGS__); \
                           libvlc_exception_raise( p_e ); \
                           return NULL; }
#define RAISEZERO( ... ) { libvlc_printerr(__VA_ARGS__); \
                           libvlc_exception_raise( p_e ); \
                           return 0; }

static inline void clear_if_needed(libvlc_exception_t *e)
{
    if (libvlc_exception_raised(e))
        libvlc_exception_clear(e);
}

static inline libvlc_time_t from_mtime(mtime_t time)
{
    return (time + 500ULL)/ 1000ULL;
}

static inline mtime_t to_mtime(libvlc_time_t time)
{
    return time * 1000ULL;
}

#endif
