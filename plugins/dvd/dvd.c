/*****************************************************************************
 * dvd.c : DVD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: dvd.c,v 1.25 2002/03/05 18:17:52 stef Exp $
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <videolan/vlc.h>

#ifdef GOD_DAMN_DMCA
#   include <dlfcn.h>
#   include "dummy_dvdcss.h"
#endif

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( access_getfunctions)( function_list_t * p_function_list );
void _M( demux_getfunctions)( function_list_t * p_function_list );

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#ifdef GOD_DAMN_DMCA
static void *p_libdvdcss;
static void ProbeLibDVDCSS  ( void );
static void UnprobeLibDVDCSS( void );
#endif

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_CATEGORY_HINT( "[dvd:][device][@raw_device][@[title][,[chapter][,angle]]]", NULL )
MODULE_CONFIG_STOP

MODULE_INIT_START
    ADD_CAPABILITY( DEMUX, 200 )
#ifndef WIN32
#  ifdef GOD_DAMN_DMCA
    SET_DESCRIPTION( "DVD input module, uses libdvdcss if present" )
    ADD_CAPABILITY( ACCESS, 90 )
#  else
    SET_DESCRIPTION( "DVD input module, linked with libdvdcss" )
    ADD_CAPABILITY( ACCESS, 100 )
#  endif
#else
    SET_DESCRIPTION( "DVD input module" )
    ADD_CAPABILITY( ACCESS, 0 )
#endif
    ADD_SHORTCUT( "dvd" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( access_getfunctions)( &p_module->p_functions->access );
    _M( demux_getfunctions)( &p_module->p_functions->demux );
#ifdef GOD_DAMN_DMCA
    ProbeLibDVDCSS();
#endif
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
#ifdef GOD_DAMN_DMCA
    UnprobeLibDVDCSS();
#endif
MODULE_DEACTIVATE_STOP


/* Following functions are local */

#ifdef GOD_DAMN_DMCA
/*****************************************************************************
 * ProbeLibDVDCSS: look for a libdvdcss object.
 *****************************************************************************
 * This functions looks for libdvdcss, using dlopen(), and fills function
 * pointers with what it finds. On failure, uses the dummy libdvdcss
 * replacement provided by vlc.
 *****************************************************************************/
static void ProbeLibDVDCSS( void )
{
    char *pp_filelist[4] = { "libdvdcss.so.1",
                             "./libdvdcss.so.1",
                             "./lib/libdvdcss.so.1",
                             NULL };
    char **pp_file = pp_filelist;

    /* Try to open the dynamic object */
    do
    {
        p_libdvdcss = dlopen( *pp_file, RTLD_LAZY );
        if( p_libdvdcss != NULL )
        {
            intf_WarnMsg( 2, "module: builtin module `dvd' found libdvdcss "
                             "in `%s'", *pp_file );
            break;
        }
        pp_file++;

    } while( *pp_file != NULL );

    /* If libdvdcss.so was found, check that it's valid */
    if( p_libdvdcss == NULL )
    {
        intf_ErrMsg( "dvd warning: libdvdcss.so.1 not present" );
    }
    else
    {
        ____dvdcss_open = dlsym( p_libdvdcss, "dvdcss_open" );
        ____dvdcss_close = dlsym( p_libdvdcss, "dvdcss_close" );
        ____dvdcss_title = dlsym( p_libdvdcss, "dvdcss_title" );
        ____dvdcss_seek = dlsym( p_libdvdcss, "dvdcss_seek" );
        ____dvdcss_read = dlsym( p_libdvdcss, "dvdcss_read" );
        ____dvdcss_readv = dlsym( p_libdvdcss, "dvdcss_readv" );
        ____dvdcss_error = dlsym( p_libdvdcss, "dvdcss_error" );

        if( ____dvdcss_open == NULL || ____dvdcss_close == NULL
             || ____dvdcss_title == NULL || ____dvdcss_seek == NULL
             || ____dvdcss_read == NULL || ____dvdcss_readv == NULL
             || ____dvdcss_error == NULL )
        {
            intf_ErrMsg( "dvd warning: missing symbols in libdvdcss.so.1, "
                         "this shouldn't happen !" );
            dlclose( p_libdvdcss );
            p_libdvdcss = NULL;
        }
    }

    /* If libdvdcss was not found or was not valid, use the dummy
     * replacement functions. */
    if( p_libdvdcss == NULL )
    {
        intf_ErrMsg( "dvd warning: no valid libdvdcss found, "
                     "I will only play unencrypted DVDs" );
        intf_ErrMsg( "dvd warning: get libdvdcss at "
                     "http://www.videolan.org/libdvdcss/" );

        ____dvdcss_open = dummy_dvdcss_open;
        ____dvdcss_close = dummy_dvdcss_close;
        ____dvdcss_title = dummy_dvdcss_title;
        ____dvdcss_seek = dummy_dvdcss_seek;
        ____dvdcss_read = dummy_dvdcss_read;
        ____dvdcss_readv = dummy_dvdcss_readv;
        ____dvdcss_error = dummy_dvdcss_error;
    }
}

/*****************************************************************************
 * UnprobeLibDVDCSS: free resources allocated by ProbeLibDVDCSS, if any.
 *****************************************************************************/
static void UnprobeLibDVDCSS( void )
{
    if( p_libdvdcss != NULL )
    {
        dlclose( p_libdvdcss );
        p_libdvdcss = NULL;
    }
}
#endif

