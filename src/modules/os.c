/*****************************************************************************
 * os.c : Low-level dynamic library handling
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h> /* MODULE_SUFFIX */
#include <vlc_charset.h>
#include "libvlc.h"
#include "modules.h"

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                              /* strdup() */

#include <sys/types.h>

#if !defined(HAVE_DYNAMIC_PLUGINS)
    /* no support for plugins */
#elif defined(HAVE_DL_BEOS)
#   if defined(HAVE_IMAGE_H)
#       include <image.h>
#   endif
#elif defined(__APPLE__)
#   include <dlfcn.h>
#elif defined(HAVE_DL_WINDOWS)
#   include <windows.h>
#elif defined(HAVE_DL_DLOPEN)
#   if defined(HAVE_DLFCN_H) /* Linux, BSD, Hurd */
#       include <dlfcn.h>
#   endif
#   if defined(HAVE_SYS_DL_H)
#       include <sys/dl.h>
#   endif
#elif defined(HAVE_DL_SHL_LOAD)
#   if defined(HAVE_DL_H)
#       include <dl.h>
#   endif
#endif
#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void *module_Lookup( module_handle_t, const char * );

#if defined(HAVE_DL_WINDOWS)
static char * GetWindowsError  ( void );
#endif

/**
 * module Call
 *
 * Call a symbol given its name and a module structure. The symbol MUST
 * refer to a function returning int and taking a module_t* as an argument.
 * \param p_module the modules
 * \return 0 if it pass and -1 in case of a failure
 */
int module_Call( vlc_object_t *obj, module_t *p_module )
{
    static const char psz_name[] = "vlc_entry" MODULE_SUFFIX;
    int (* pf_symbol) ( module_t * p_module );

    /* Try to resolve the symbol */
    pf_symbol = (int (*)(module_t *)) module_Lookup( p_module->handle,
                                                     psz_name );

    if( pf_symbol == NULL )
    {
#if defined(HAVE_DL_BEOS)
        msg_Warn( obj, "cannot find symbol \"%s\" in file `%s'",
                  psz_name, p_module->psz_filename );
#elif defined(HAVE_DL_WINDOWS)
        char *psz_error = GetWindowsError();
        msg_Warn( obj, "cannot find symbol \"%s\" in file `%s' (%s)",
                  psz_name, p_module->psz_filename, psz_error );
        free( psz_error );
#elif defined(HAVE_DL_DLOPEN)
        msg_Warn( obj, "cannot find symbol \"%s\" in file `%s' (%s)",
                  psz_name, p_module->psz_filename, dlerror() );
#elif defined(HAVE_DL_SHL_LOAD)
        msg_Warn( obj, "cannot find symbol \"%s\" in file `%s' (%m)",
                  psz_name, p_module->psz_filename );
#else
#   error "Something is wrong in modules.c"
#endif
        return -1;
    }

    /* We can now try to call the symbol */
    if( pf_symbol( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( obj, "Failed to call symbol \"%s\" in file `%s'",
                 psz_name, p_module->psz_filename );
        return -1;
    }

    /* Everything worked fine, we can return */
    return 0;
}

/**
 * Load a dynamically linked library using a system dependent method.
 *
 * \param p_this vlc object
 * \param psz_file library file
 * \param p_handle the module handle returned
 * \return 0 on success as well as the module handle.
 */
int module_Load( vlc_object_t *p_this, const char *psz_file,
                 module_handle_t *p_handle )
{
    module_handle_t handle;

#if defined(HAVE_DL_BEOS)
    handle = load_add_on( psz_file );
    if( handle < 0 )
    {
        msg_Warn( p_this, "cannot load module `%s'", psz_file );
        return -1;
    }

#elif defined(HAVE_DL_WINDOWS)
    wchar_t psz_wfile[MAX_PATH];
    MultiByteToWideChar( CP_UTF8, 0, psz_file, -1, psz_wfile, MAX_PATH );

#ifndef UNDER_CE
    /* FIXME: this is not thread-safe -- Courmisch */
    UINT mode = SetErrorMode (SEM_FAILCRITICALERRORS);
    SetErrorMode (mode|SEM_FAILCRITICALERRORS);
#endif

    handle = LoadLibraryW( psz_wfile );

#ifndef UNDER_CE
    SetErrorMode (mode);
#endif

    if( handle == NULL )
    {
        char *psz_err = GetWindowsError();
        msg_Warn( p_this, "cannot load module `%s' (%s)", psz_file, psz_err );
        free( psz_err );
        return -1;
    }

#elif defined(HAVE_DL_DLOPEN)

# if defined (RTLD_NOW)
    const int flags = RTLD_NOW;
# elif defined (DL_LAZY)
    const int flags = DL_LAZY;
# else
    const int flags = 0;
# endif
    char *path = ToLocale( psz_file );

    handle = dlopen( path, flags );
    if( handle == NULL )
    {
        msg_Warn( p_this, "cannot load module `%s' (%s)", path, dlerror() );
        LocaleFree( path );
        return -1;
    }
    LocaleFree( path );

#elif defined(HAVE_DL_SHL_LOAD)
    handle = shl_load( psz_file, BIND_IMMEDIATE | BIND_NONFATAL, NULL );
    if( handle == NULL )
    {
        msg_Warn( p_this, "cannot load module `%s' (%m)", psz_file );
        return -1;
    }

#else
#   error "Something is wrong in modules.c"

#endif

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
#if defined(HAVE_DL_BEOS)
    unload_add_on( handle );

#elif defined(HAVE_DL_WINDOWS)
    FreeLibrary( handle );

#elif defined(HAVE_DL_DLOPEN)
# ifdef HAVE_VALGRIND_VALGRIND_H
    if( RUNNING_ON_VALGRIND > 0 )
        return; /* do not dlclose() so that we get proper stack traces */
# endif
    dlclose( handle );

#elif defined(HAVE_DL_SHL_LOAD)
    shl_unload( handle );

#endif
    return;
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
static void *module_Lookup( module_handle_t handle, const char *psz_function )
{
#if defined(HAVE_DL_BEOS)
    void * p_symbol;
    if( B_OK == get_image_symbol( handle, psz_function,
                                  B_SYMBOL_TYPE_TEXT, &p_symbol ) )
    {
        return p_symbol;
    }
    else
    {
        return NULL;
    }

#elif defined(HAVE_DL_WINDOWS) && defined(UNDER_CE)
    wchar_t wide[strlen( psz_function ) + 1];
    size_t i = 0;
    do
        wide[i] = psz_function[i]; /* UTF-16 <- ASCII */
    while( psz_function[i++] );

    return (void *)GetProcAddress( handle, wide );

#elif defined(HAVE_DL_WINDOWS) && defined(WIN32)
    return (void *)GetProcAddress( handle, (char *)psz_function );

#elif defined(HAVE_DL_DLOPEN)
    return dlsym( handle, psz_function );

#elif defined(HAVE_DL_SHL_LOAD)
    void *p_sym;
    shl_findsym( &handle, psz_function, TYPE_UNDEFINED, &p_sym );
    return p_sym;

#endif
}

#if defined(HAVE_DL_WINDOWS)
static char * GetWindowsError( void )
{
#if defined(UNDER_CE)
    wchar_t psz_tmp[MAX_PATH];
    char * psz_buffer = malloc( MAX_PATH );
#else
    char * psz_tmp = malloc( MAX_PATH );
#endif
    int i = 0, i_error = GetLastError();

    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, i_error, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR)psz_tmp, MAX_PATH, NULL );

    /* Go to the end of the string */
    while( psz_tmp[i] && psz_tmp[i] != _T('\r') && psz_tmp[i] != _T('\n') )
    {
        i++;
    }

    if( psz_tmp[i] )
    {
#if defined(UNDER_CE)
        swprintf( psz_tmp + i, L" (error %i)", i_error );
        psz_tmp[ 255 ] = L'\0';
#else
        snprintf( psz_tmp + i, 256 - i, " (error %i)", i_error );
        psz_tmp[ 255 ] = '\0';
#endif
    }

#if defined(UNDER_CE)
    wcstombs( psz_buffer, psz_tmp, MAX_PATH );
    return psz_buffer;
#else
    return psz_tmp;
#endif
}
#endif /* HAVE_DL_WINDOWS */
#endif /* HAVE_DYNAMIC_PLUGINS */
