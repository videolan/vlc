/*****************************************************************************
 * modules_core.h : Module management functions used by the core application.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
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

/* Function to load a dynamic module, returns 0 if successful. */
static __inline__ int
module_load( char * psz_filename, module_handle_t * handle )
{
#ifdef SYS_BEOS
    *handle = load_add_on( psz_filename );
    return( *handle >= 0 );
#else
    *handle = dlopen( psz_filename, RTLD_NOW | RTLD_GLOBAL );
    return( *handle != NULL );
#endif
}

/* Unload a dynamic module. */
static __inline__ void
module_unload( module_handle_t handle )
{
#ifdef SYS_BEOS
    unload_add_on( handle );
#else
    dlclose( handle );
#endif
    return;
}

/* Get a given symbol from a module. */
static __inline__ void *
module_getsymbol( module_handle_t handle, char * psz_function )
{
#ifdef SYS_BEOS
    void * p_symbol;
    get_image_symbol( handle, psz_function, B_SYMBOL_TYPE_TEXT, &p_symbol );
    return( p_symbol );
#else
    return( dlsym( handle, psz_function ) );
#endif
}

/* Wrapper to dlerror() for systems that don't have it. */
static __inline__ char *
module_error( void )
{
#ifdef SYS_BEOS
    return( "failed" );
#else
    return( dlerror() );
#endif
}

