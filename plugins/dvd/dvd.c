/*****************************************************************************
 * dvd.c : DVD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: dvd.c,v 1.12 2001/08/06 13:28:00 sam Exp $
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

#define MODULE_NAME dvd
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#ifdef GOD_DAMN_DMCA
#   include <dlfcn.h>
#   include "dummy_dvdcss.h"
#endif

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list );

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
ADD_WINDOW( "Configuration for DVD module" )
    ADD_COMMENT( "foobar !" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_INPUT;
#ifdef GOD_DAMN_DMCA
    p_module->psz_longname = "DVD input module, uses libdvdcss if present";
#else
    p_module->psz_longname = "DVD input module, linked with libdvdcss";
#endif
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( input_getfunctions )( &p_module->p_functions->input );
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
    char *pp_filelist[4] = { "libdvdcss.so.0",
                             "./libdvdcss.so.0",
                             "./lib/libdvdcss.so.0",
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
        intf_ErrMsg( "dvd warning: libdvdcss.so.0 not present" );
    }
    else
    {
        /* Check for libdvdcss 0.0.1 */
        if( dlsym( p_libdvdcss, "dvdcss_crack" ) != NULL )
        {
            intf_ErrMsg( "dvd warning: libdvdcss.so.0 has deprecated symbol "
                         "dvdcss_crack(), please upgrade" );
            dlclose( p_libdvdcss );
            p_libdvdcss = NULL;
        }
        else
        {
            dvdcss_open = dlsym( p_libdvdcss, "dvdcss_open" );
            dvdcss_close = dlsym( p_libdvdcss, "dvdcss_close" );
            dvdcss_title = dlsym( p_libdvdcss, "dvdcss_title" );
            dvdcss_seek = dlsym( p_libdvdcss, "dvdcss_seek" );
            dvdcss_read = dlsym( p_libdvdcss, "dvdcss_read" );
            dvdcss_readv = dlsym( p_libdvdcss, "dvdcss_readv" );
            dvdcss_error = dlsym( p_libdvdcss, "dvdcss_error" );

            if( dvdcss_open == NULL || dvdcss_close == NULL
                 || dvdcss_title == NULL || dvdcss_seek == NULL
                 || dvdcss_read == NULL || dvdcss_readv == NULL
                 || dvdcss_error == NULL )
            {
                intf_ErrMsg( "dvd warning: missing symbols in libdvdcss.so.0, "
                             "please upgrade libdvdcss or vlc" );
                dlclose( p_libdvdcss );
                p_libdvdcss = NULL;
            }
        }
    }

    /* If libdvdcss was not found or was not valid, use the dummy
     * replacement functions. */
    if( p_libdvdcss == NULL )
    {
        intf_ErrMsg( "dvd warning: no valid libdvdcss found, "
                     "I will only play unencrypted DVDs" );

        dvdcss_open = dummy_dvdcss_open;
        dvdcss_close = dummy_dvdcss_close;
        dvdcss_title = dummy_dvdcss_title;
        dvdcss_seek = dummy_dvdcss_seek;
        dvdcss_read = dummy_dvdcss_read;
        dvdcss_readv = dummy_dvdcss_readv;
        dvdcss_error = dummy_dvdcss_error;
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

