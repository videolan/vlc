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
        /* The suggested way for an application to make sure that global_init
         * has been called is by using gcry_check_version. (see global_init
         * comments in gcrypt sources) */
        gcry_check_version(NULL);
        done = true;
    }
    vlc_global_unlock (VLC_GCRYPT_MUTEX);
}
