/*****************************************************************************
 * dvd.c : DVD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: dvd.c,v 1.32 2002/06/01 12:31:58 sam Exp $
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

#include <vlc/vlc.h>

#ifdef GOD_DAMN_DMCA
#   include <stdio.h>
#   include <fcntl.h>
#   include <unistd.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <sys/uio.h>                                      /* struct iovec */
#   include <sys/ioctl.h>
#   include <dlfcn.h>
#   include <netinet/in.h>
#   include <linux/cdrom.h>

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
ADD_CATEGORY_HINT( N_("[dvd:][device][@raw_device][@[title][,[chapter][,angle]]]"), NULL )
MODULE_CONFIG_STOP

MODULE_INIT_START
    ADD_CAPABILITY( DEMUX, 0 )
#ifdef GOD_DAMN_DMCA
    SET_DESCRIPTION( _("DVD input module, uses libdvdcss if present") )
    ADD_CAPABILITY( ACCESS, 90 )
#else
    SET_DESCRIPTION( _("DVD input module, uses libdvdcss") )
    ADD_CAPABILITY( ACCESS, 100 )
#endif
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
    static char *pp_filelist[] = { "libdvdcss.so.2",
                                   "./libdvdcss.so.2",
                                   "./lib/libdvdcss.so.2",
                                   "libdvdcss.so.1",
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
//X            intf_WarnMsg( 2, "module: builtin module `dvd' found libdvdcss "
//X                             "in `%s'", *pp_file );
            break;
        }
        pp_file++;

    } while( *pp_file != NULL );

    /* If libdvdcss.so was found, check that it's valid */
    if( p_libdvdcss == NULL )
    {
//X        intf_ErrMsg( "dvd warning: libdvdcss.so.2 not present" );
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
//X            intf_ErrMsg( "dvd warning: missing symbols in libdvdcss.so.2, "
//X                         "this shouldn't happen !" );
            dlclose( p_libdvdcss );
            p_libdvdcss = NULL;
        }
    }

    /* If libdvdcss was not found or was not valid, use the dummy
     * replacement functions. */
    if( p_libdvdcss == NULL )
    {
//X        intf_ErrMsg( "dvd warning: no valid libdvdcss found, "
//X                     "I will only play unencrypted DVDs" );
//X        intf_ErrMsg( "dvd warning: get libdvdcss at "
//X                     "http://www.videolan.org/libdvdcss/" );

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

/* Dummy libdvdcss with minimal DVD access. */

/*****************************************************************************
 * Local structure
 *****************************************************************************/
struct dvdcss_s
{
    /* File descriptor */
    int i_fd;
};

/*****************************************************************************
 * dvdcss_open: initialize library, open a DVD device, crack CSS key
 *****************************************************************************/
extern dvdcss_handle dummy_dvdcss_open ( char *psz_target )
{
    dvdcss_handle dvdcss;
    dvd_struct    dvd;

    /* Allocate the library structure */
    dvdcss = malloc( sizeof( struct dvdcss_s ) );
    if( dvdcss == NULL )
    {
        fprintf( stderr, "dvd error: "
                         "dummy libdvdcss could not allocate memory\n" );
        return NULL;
    }

    /* Open the device */
    dvdcss->i_fd = open( psz_target, 0 );
    if( dvdcss->i_fd < 0 )
    {
        fprintf( stderr, "dvd error: "
                         "dummy libdvdcss could not open device\n" );
        free( dvdcss );
        return NULL;
    }

    /* Check for encryption or ioctl failure */
    dvd.type = DVD_STRUCT_COPYRIGHT;
    dvd.copyright.layer_num = 0;
    if( ioctl( dvdcss->i_fd, DVD_READ_STRUCT, &dvd ) != 0
         || dvd.copyright.cpst )
    {
        fprintf( stderr, "dvd error: "
                         "dummy libdvdcss could not decrypt disc\n" );
        close( dvdcss->i_fd );
        free( dvdcss );
        return NULL;
    }

    return dvdcss;
}

/*****************************************************************************
 * dvdcss_error: return the last libdvdcss error message
 *****************************************************************************/
extern char * dummy_dvdcss_error ( dvdcss_handle dvdcss )
{
    return "generic error";
}

/*****************************************************************************
 * dvdcss_seek: seek into the device
 *****************************************************************************/
extern int dummy_dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks,
                                                     int i_flags )
{
    off_t i_read;

    i_read = lseek( dvdcss->i_fd,
                    (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE, SEEK_SET );

    return i_read / DVDCSS_BLOCK_SIZE;
}

/*****************************************************************************
 * dvdcss_title: crack the current title key if needed
 *****************************************************************************/
extern int dummy_dvdcss_title ( dvdcss_handle dvdcss, int i_block )
{
    return 0;
}

/*****************************************************************************
 * dvdcss_read: read data from the device, decrypt if requested
 *****************************************************************************/
extern int dummy_dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer,
                                                     int i_blocks,
                                                     int i_flags )
{
    int i_bytes;

    i_bytes = read( dvdcss->i_fd, p_buffer,
                    (size_t)i_blocks * DVDCSS_BLOCK_SIZE );

    return i_bytes / DVDCSS_BLOCK_SIZE;
}

/*****************************************************************************
 * dvdcss_readv: read data to an iovec structure, decrypt if reaquested
 *****************************************************************************/
extern int dummy_dvdcss_readv ( dvdcss_handle dvdcss, void *p_iovec,
                                                      int i_blocks,
                                                      int i_flags )
{
    int i_read;

    i_read = readv( dvdcss->i_fd, (struct iovec*)p_iovec, i_blocks );

    return i_read / DVDCSS_BLOCK_SIZE;
}

/*****************************************************************************
 * dvdcss_close: close the DVD device and clean up the library
 *****************************************************************************/
extern int dummy_dvdcss_close ( dvdcss_handle dvdcss )
{
    int i_ret;

    i_ret = close( dvdcss->i_fd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    free( dvdcss );

    return 0;
}

#endif

