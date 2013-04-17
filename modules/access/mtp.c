/*****************************************************************************
 * mtp.c: mtp input (mtp: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2006-2008 Rémi Denis-Courmont
 *
 * Authors: Fabio Ritrovato <exsephiroth87@gmail.com>
 * Original file.c: Christophe Massiot <massiot@via.ecp.fr>
 *                  Rémi Denis-Courmont <rem # videolan # org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_fs.h>

#include "libmtp.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("MTP input") )
    set_shortname( N_("MTP") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access", 0 )
    add_shortcut( "mtp" )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

static int  Seek( access_t *, uint64_t );
static ssize_t Read( access_t *, uint8_t *, size_t );
static int  Control( access_t *, int, va_list );

static int  open_file( access_t *, const char * );

struct access_sys_t
{
    unsigned int i_nb_reads;
    int fd;
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = ( access_t* )p_this;
    access_sys_t *p_sys;
    uint32_t i_bus;
    uint8_t i_dev;
    uint16_t i_product_id;
    int i_track_id;
    LIBMTP_raw_device_t *p_rawdevices;
    LIBMTP_mtpdevice_t *p_device;
    int i_numrawdevices;
    int i_ret;

    if( sscanf( p_access->psz_location, "%"SCNu32":%"SCNu8":%"SCNu16":%d",
                &i_bus, &i_dev, &i_product_id, &i_track_id ) != 4 )
        return VLC_EGENERIC;
    i_ret = LIBMTP_Detect_Raw_Devices( &p_rawdevices, &i_numrawdevices );
    if( i_ret != 0 || i_numrawdevices <= 0 || !p_rawdevices )
        return VLC_EGENERIC;

    for( int i = 0; i < i_numrawdevices; i++ )
    {
        if( i_bus == p_rawdevices[i].bus_location &&
            i_dev == p_rawdevices[i].devnum &&
            i_product_id == p_rawdevices[i].device_entry.product_id )
        {
            if( ( p_device = LIBMTP_Open_Raw_Device( &p_rawdevices[i] )
                ) != NULL )
            {
                free( p_access->psz_filepath );
#warning Oooh no! Not tempnam()!
                p_access->psz_filepath = tempnam( NULL, "vlc" );
                if( p_access->psz_filepath == NULL )
                {
                    LIBMTP_Release_Device( p_device );
                    free( p_rawdevices );
                    return VLC_ENOMEM;
                }
                else
                {
                    msg_Dbg( p_access, "About to write %s",
                             p_access->psz_filepath );
                    LIBMTP_Get_File_To_File( p_device, i_track_id,
                                             p_access->psz_filepath, NULL,
                                             NULL );
                    LIBMTP_Release_Device( p_device );
                    i = i_numrawdevices;
                }
            }
            else
            {
                free( p_rawdevices );
                return VLC_EGENERIC;
            }
        }
    }
    free( p_rawdevices );

    STANDARD_READ_ACCESS_INIT;
    p_sys->i_nb_reads = 0;
    int fd = p_sys->fd = -1;

    /* Open file */
    msg_Dbg( p_access, "opening file `%s'", p_access->psz_filepath );
    fd = open_file( p_access, p_access->psz_filepath );

    if( fd == -1 )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->fd = fd;

    struct stat st;
    if( fstat( fd, &st ) )
        msg_Err( p_access, "fstat(%d): %m", fd );
    p_access->info.i_size = st.st_size;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t     *p_access = ( access_t* )p_this;
    access_sys_t *p_sys = p_access->p_sys;

    close ( p_sys->fd );
    if(	vlc_unlink( p_access->psz_filepath ) != 0 )
        msg_Err( p_access, "Error deleting file %s, %m",
                 p_access->psz_filepath );
    free( p_sys );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    ssize_t i_ret;
    int fd = p_sys->fd;

    i_ret = read( fd, p_buffer, i_len );

    if( i_ret < 0 )
    {
        switch( errno )
        {
            case EINTR:
            case EAGAIN:
                break;

            default:
                msg_Err( p_access, "read failed (%m)" );
                dialog_Fatal( p_access, _( "File reading failed" ), "%s (%m)",
                                _( "VLC could not read the file." ) );
                p_access->info.b_eof = true;
                return 0;
        }
    }
    else if( i_ret > 0 )
        p_access->info.i_pos += i_ret;
    else
        p_access->info.b_eof = true;

    p_sys->i_nb_reads++;

    return i_ret;
}


/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = false;

    lseek( p_access->p_sys->fd, i_pos, SEEK_SET );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool   *pb_bool;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = ( bool* )va_arg( args, bool* );
            *pb_bool = true;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = ( bool* )va_arg( args, bool* );
            *pb_bool = true;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = ( int64_t* )va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                   * var_InheritInteger( p_access, "file-caching" );
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_META:
        case ACCESS_GET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query %d in control", i_query );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * open_file: Opens a specific file
 *****************************************************************************/
static int open_file( access_t *p_access, const char *path )
{
    int fd = vlc_open( path, O_RDONLY | O_NONBLOCK );
    if( fd == -1 )
    {
        msg_Err( p_access, "cannot open file %s (%m)", path );
        dialog_Fatal( p_access, _( "File reading failed" ),
                        _( "VLC could not open the file \"%s\". (%m)" ), path );
        return -1;
    }
#ifdef F_RDAHEAD
    fcntl( fd, F_RDAHEAD, 1 );
#endif
#ifdef F_NOCACHE
    fcntl( fd, F_NOCACHE, 0 );
#endif
    return fd;
}
