/*****************************************************************************
 * modules_core.h : Module management functions used by the core application.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules_core.h,v 1.2 2001/06/14 01:49:44 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Inline functions for handling dynamic modules
 *****************************************************************************/

/*****************************************************************************
 * module_load: load a dynamic library
 *****************************************************************************
 * This function loads a dynamically linked library using a system dependant
 * method, and returns a non-zero value on error, zero otherwise.
 *****************************************************************************/
static __inline__ int
module_load( char * psz_filename, module_handle_t * handle )
{
#ifdef SYS_BEOS
    *handle = load_add_on( psz_filename );
    return( *handle < 0 );

#elif defined(WIN32)
    *handle = LoadLibrary( psz_filename );
    return( *handle == NULL ); 

#else
    /* Do not open modules with RTLD_GLOBAL, or we are going to get namespace
     * collisions when two modules have common public symbols */
    *handle = dlopen( psz_filename, RTLD_NOW );
    return( *handle == NULL );

#endif
}

/*****************************************************************************
 * module_unload: unload a dynamic library
 *****************************************************************************
 * This function unloads a previously opened dynamically linked library
 * using a system dependant method. No return value is taken in consideration,
 * since some libraries sometimes refuse to close properly.
 *****************************************************************************/
static __inline__ void
module_unload( module_handle_t handle )
{
#ifdef SYS_BEOS
    unload_add_on( handle );

#elif defined(WIN32)
    FreeLibrary( handle );

#else
    dlclose( handle );

#endif
    return;
}

/*****************************************************************************
 * module_getsymbol: get a symbol from a dynamic library
 *****************************************************************************
 * This function queries a loaded library for a symbol specified in a
 * string, and returns a pointer to it.
 * FIXME: under Unix we should maybe check for dlerror() instead of the
 * return value of dlsym, since we could have loaded a symbol really set
 * to NULL (quite unlikely, though).
 *****************************************************************************/
static __inline__ void *
module_getsymbol( module_handle_t handle, char * psz_function )
{
#ifdef SYS_BEOS
    void * p_symbol;
    if( B_OK == get_image_symbol( handle, psz_function,
                                  B_SYMBOL_TYPE_TEXT, &p_symbol ) )
    {
        return( p_symbol );
    }
    else
    {
        return( NULL );
    }

#elif defined(SYS_DARWIN1_3)
    /* MacOS X dl library expects symbols to begin with "_". That's
     * really lame, but hey, what can we do ? */
    char *  psz_call = malloc( strlen( psz_function ) + 2 );
    void *  p_return;
    strcpy( psz_call + 1, psz_function );
    psz_call[ 0 ] = '_';

    p_return = dlsym( handle, psz_call );

    free( psz_call );
    return( p_return );

#elif defined(WIN32)
    return( (void *)GetProcAddress( handle, psz_function ) );

#else
    return( dlsym( handle, psz_function ) );

#endif
}

/*****************************************************************************
 * module_error: wrapper for dlerror()
 *****************************************************************************
 * This function returns the error message of the last module operation. It
 * returns the string "failed" on systems which do not have the dlerror()
 * function.
 *****************************************************************************/
static __inline__ const char *
module_error( void )
{
#if defined(SYS_BEOS) || defined(WIN32)
    return( "failed" );

#else
    return( dlerror() );

#endif
}

