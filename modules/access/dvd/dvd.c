/*****************************************************************************
 * dvd.c : DVD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: dvd.c,v 1.6 2003/03/30 18:14:35 gbazin Exp $
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

#   include "dvdcss.h"
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
int  E_(DVDOpen)   ( vlc_object_t * );
void E_(DVDClose)  ( vlc_object_t * );

int  E_(DVDInit)   ( vlc_object_t * );
void E_(DVDEnd)    ( vlc_object_t * );

#ifdef GOD_DAMN_DMCA
static void *p_libdvdcss;
static void ProbeLibDVDCSS  ( void );
static void UnprobeLibDVDCSS( void );
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CSSMETHOD_TEXT N_("method to use by libdvdcss for key decryption")
#define CSSMETHOD_LONGTEXT N_( \
    "Set the method used by libdvdcss for key decryption.\n" \
    "title: decrypted title key is guessed from the encrypted sectors of " \
           "the stream. Thus it should work with a file as well as the " \
           "DVD device. But it sometimes takes much time to decrypt a title " \
           "key and may even fail. With this method, the key is only checked "\
           "at the beginning of each title, so it won't work if the key " \
           "changes in the middle of a title.\n" \
    "disc: the disc key is first cracked, then all title keys can be " \
           "decrypted instantly, which allows us to check them often.\n" \
    "key: the same as \"disc\" if you don't have a file with player keys " \
           "at compilation time. If you do, the decryption of the disc key " \
           "will be faster with this method. It is the one that was used by " \
           "libcss.\n" \
    "The default method is: key.")

static char *cssmethod_list[] = { "title", "disc", "key", NULL };

vlc_module_begin();
    int i;
    add_usage_hint( N_("[dvd:][device][@raw_device][@[title][,[chapter][,angle]]]") );
    add_category_hint( N_("dvd"), NULL, VLC_TRUE );
    add_string_from_list( "dvdcss-method", NULL, cssmethod_list, NULL,
                          CSSMETHOD_TEXT, CSSMETHOD_LONGTEXT, VLC_TRUE );
#ifdef GOD_DAMN_DMCA
    set_description( _("DVD input (uses libdvdcss if installed)") );
    i = 90;
#else
    set_description( _("DVD input (uses libdvdcss)") );
    i = 100;
#endif
    add_shortcut( "dvdold" );
    add_shortcut( "dvdsimple" );
    add_submodule();
        set_capability( "access", i );
        set_callbacks( E_(DVDOpen), E_(DVDClose) );
    add_submodule();
        set_capability( "demux", 0 );
        set_callbacks( E_(DVDInit), E_(DVDEnd) );
#ifdef GOD_DAMN_DMCA
    ProbeLibDVDCSS();
#endif
vlc_module_end();

#if 0 /* FIXME */
    UnprobeLibDVDCSS();
#endif

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

