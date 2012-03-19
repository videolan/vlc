/*****************************************************************************
 * plugin.c : Low-level dynamic library handling
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_common.h>
#include "modules/modules.h"

#include <sys/types.h>
#include <dlfcn.h>

#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

/**
 * Load a dynamically linked library using a system dependent method.
 *
 * \param p_this vlc object
 * \param path library file
 * \param p_handle the module handle returned
 * \return 0 on success as well as the module handle.
 */
int module_Load (vlc_object_t *p_this, const char *path,
                 module_handle_t *p_handle, bool lazy)
{
#if defined (RTLD_NOW)
    const int flags = lazy ? RTLD_LAZY : RTLD_NOW;
#elif defined (DL_LAZY)
    const int flags = DL_LAZY;
#else
    const int flags = 0;
#endif

    module_handle_t handle = dlopen (path, flags);
    if( handle == NULL )
    {
        msg_Warn( p_this, "cannot load module `%s' (%s)", path, dlerror() );
        return -1;
    }
    *p_handle = handle;
    return 0;
}

/**
 * CloseModule: unload a dynamic library
 *
 * This function unloads a previously opened dynamically linked library
 * using a system dependent method. No return value is taken in consideration,
 * since some libraries sometimes refuse to close properly.
 * \param handle handle of the library
 * \return nothing
 */
void module_Unload( module_handle_t handle )
{
#ifdef HAVE_VALGRIND_VALGRIND_H
    if( RUNNING_ON_VALGRIND > 0 )
        return; /* do not dlclose() so that we get proper stack traces */
#endif
    dlclose( handle );
}

/**
 * Looks up a symbol from a dynamically loaded library
 *
 * This function queries a loaded library for a symbol specified in a
 * string, and returns a pointer to it. We don't check for dlerror() or
 * similar functions, since we want a non-NULL symbol anyway.
 *
 * @param handle handle to the module
 * @param psz_function function name
 * @return NULL on error, or the address of the symbol
 */
void *module_Lookup( module_handle_t handle, const char *psz_function )
{
    return dlsym( handle, psz_function );
}
