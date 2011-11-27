/*****************************************************************************
 * vlc_gcrypt.h: VLC thread support for gcrypt
 *****************************************************************************
 * Copyright (C) 2004-2010 Rémi Denis-Courmont
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

/**
 * \file
 * This file implements gcrypt support functions in vlc
 */

#include <errno.h>

#ifdef LIBVLC_USE_PTHREAD
/**
 * If possible, use gcrypt-provided thread implementation. This is so that
 * other non-VLC components (inside the process) can also use gcrypt safely.
 */
GCRY_THREAD_OPTION_PTHREAD_IMPL;
# define gcry_threads_vlc gcry_threads_pthread
#else

/**
 * gcrypt thread option VLC implementation
 */

static int gcry_vlc_mutex_init( void **p_sys )
{
    vlc_mutex_t *p_lock = (vlc_mutex_t *)malloc( sizeof( vlc_mutex_t ) );
    if( p_lock == NULL)
        return ENOMEM;

    vlc_mutex_init( p_lock );
    *p_sys = p_lock;
    return VLC_SUCCESS;
}

static int gcry_vlc_mutex_destroy( void **p_sys )
{
    vlc_mutex_t *p_lock = (vlc_mutex_t *)*p_sys;
    vlc_mutex_destroy( p_lock );
    free( p_lock );
    return VLC_SUCCESS;
}

static int gcry_vlc_mutex_lock( void **p_sys )
{
    vlc_mutex_lock( (vlc_mutex_t *)*p_sys );
    return VLC_SUCCESS;
}

static int gcry_vlc_mutex_unlock( void **lock )
{
    vlc_mutex_unlock( (vlc_mutex_t *)*lock );
    return VLC_SUCCESS;
}

static const struct gcry_thread_cbs gcry_threads_vlc =
{
    GCRY_THREAD_OPTION_USER,
    NULL,
    gcry_vlc_mutex_init,
    gcry_vlc_mutex_destroy,
    gcry_vlc_mutex_lock,
    gcry_vlc_mutex_unlock,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
#endif

/**
 * Initializes gcrypt with proper locking.
 */
static inline void vlc_gcrypt_init (void)
{
    /* This would need a process-wide static mutex with all libraries linking
     * to a given instance of libgcrypt. We cannot do this as we have different
     * plugins linking with gcrypt, and some underlying libraries may use it
     * behind our back. Only way is to always link gcrypt statically (ouch!) or
     * have upstream gcrypt provide one shared object per threading system. */
    static bool done = false;

    vlc_global_lock (VLC_GCRYPT_MUTEX);
    if (!done)
    {
        gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_vlc);
        done = true;
    }
    vlc_global_unlock (VLC_GCRYPT_MUTEX);
}
