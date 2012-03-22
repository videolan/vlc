/*****************************************************************************
 * plugin.c : Low-level dynamic library handling
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 * Copyright (C) 2012 KO Myung-Hun
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
 *          KO Myung-Hun <komh@chollian.net>
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
#include <vlc_charset.h>
#include "modules/modules.h"

#include <sys/types.h>
#include <dlfcn.h>

/**
 * Load a dynamically linked library using a system dependent method.
 *
 * \param p_this vlc object
 * \param psz_file library file
 * \param p_handle the module handle returned
 * \return 0 on success as well as the module handle.
 */
int module_Load( vlc_object_t *p_this, const char *psz_file,
                 module_handle_t *p_handle, bool lazy )
{
    const int flags = lazy ? RTLD_LAZY : RTLD_NOW;
    char *path = ToLocale( psz_file );

    module_handle_t handle = dlopen( path, flags );
    if( handle == NULL )
    {
        msg_Warn( p_this, "cannot load module `%s' (%s)", path, dlerror() );
        LocaleFree( path );
        return -1;
    }
    LocaleFree( path );
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
