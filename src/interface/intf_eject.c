/*****************************************************************************
 * intf_eject.c: CD/DVD-ROM ejection handling functions
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Julien Blache <jb@technologeek.org> for the Linux part
 *                with code taken from the Linux "eject" command
 *          Jon Lech Johansen <jon-vl@nanocrew.net> for Darwin
 *          Gildas Bazin <gbazin@netcourrier.com> for Win32
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

/**
 *  \file
 *  This file contain functions to eject CD and DVD drives
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_DVD_H
#   include <dvd.h>
#endif

#if defined(__linux__) && defined(HAVE_LINUX_VERSION_H)
#   include <linux/version.h>
    /* handy macro found in 2.1 kernels, but not in older ones */
#   ifndef KERNEL_VERSION
#       define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#   endif

#   include <sys/types.h>
#   include <sys/stat.h>
#   include <sys/ioctl.h>

#   include <sys/ioctl.h>
#   include <sys/mount.h>

#   include <linux/cdrom.h>
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)
#       include <linux/ucdrom.h>
#   endif

#   include <scsi/scsi.h>
#   include <scsi/sg.h>
#   include <scsi/scsi_ioctl.h>
#endif

#if defined( WIN32 ) && !defined( UNDER_CE )
#   include <mmsystem.h>
#endif

#include <vlc_interface.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#if defined(__linux__) && defined(HAVE_LINUX_VERSION_H)
static int EjectSCSI ( int i_fd );
#endif

/*****************************************************************************
 * intf_Eject: eject the CDRom
 *****************************************************************************
 * returns 0 on success
 * returns 1 on failure
 * returns -1 if not implemented
 *****************************************************************************/
/**
 * \brief Ejects the CD /DVD
 * \ingroup vlc_interface
 * \param p_this the calling vlc_object_t
 * \param psz_device the CD/DVD to eject
 * \return 0 on success, 1 on failure, -1 if not implemented
 */
int __intf_Eject( vlc_object_t *p_this, const char *psz_device )
{
    VLC_UNUSED(p_this);
    int i_ret = VLC_SUCCESS;

#ifdef __APPLE__
    FILE *p_eject;
    char *psz_disk;
    char sz_cmd[32];

    /*
     * The only way to cleanly unmount the disc under MacOS X
     * is to use the 'disktool' command line utility. It uses
     * the non-public Disk Arbitration API, which can not be
     * used by Cocoa or Carbon applications.
     */

    if( ( psz_disk = (char *)strstr( psz_device, "disk" ) ) != NULL &&
        strlen( psz_disk ) > 4 )
    {
#define EJECT_CMD "/usr/sbin/disktool -e %s 0"
        snprintf( sz_cmd, sizeof(sz_cmd), EJECT_CMD, psz_disk );
#undef EJECT_CMD

        if( ( p_eject = popen( sz_cmd, "r" ) ) != NULL )
        {
            char psz_result[0x200];
            i_ret = fread( psz_result, 1, sizeof(psz_result) - 1, p_eject );

            if( i_ret == 0 && ferror( p_eject ) != 0 )
            {
                pclose( p_eject );
                return VLC_EGENERIC;
            }

            pclose( p_eject );

            psz_result[ i_ret ] = 0;

            if( strstr( psz_result, "Disk Ejected" ) != NULL )
            {
                return VLC_SUCCESS;
            }
        }
    }

    return VLC_EGENERIC;

#elif defined(UNDER_CE)
    msg_Warn( p_this, "CD-Rom ejection unsupported on this platform" );
    return i_ret;

#elif defined(WIN32)
    MCI_OPEN_PARMS op;
    MCI_STATUS_PARMS st;
    DWORD i_flags;
    char psz_drive[4];

    memset( &op, 0, sizeof(MCI_OPEN_PARMS) );
    op.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_CD_AUDIO;

    strcpy( psz_drive, "X:" );
    psz_drive[0] = psz_device[0];
    op.lpstrElementName = psz_drive;

    /* Set the flags for the device type */
    i_flags = MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID |
              MCI_OPEN_ELEMENT | MCI_OPEN_SHAREABLE;

    if( !mciSendCommand( 0, MCI_OPEN, i_flags, (unsigned long)&op ) )
    {
        st.dwItem = MCI_STATUS_READY;
        /* Eject disc */
        i_ret = mciSendCommand( op.wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, 0 );
        /* Release access to the device */
        mciSendCommand( op.wDeviceID, MCI_CLOSE, MCI_WAIT, 0 );
    }
    else i_ret = VLC_EGENERIC;

    return i_ret;
#else   /* WIN32 */

    int i_fd;

    /* This code could be extended to support CD/DVD-ROM chargers */

    i_fd = open( psz_device, O_RDONLY | O_NONBLOCK );

    if( i_fd == -1 )
    {
        msg_Err( p_this, "could not open device %s", psz_device );
        return VLC_EGENERIC;
    }

#if defined(__linux__) && defined(HAVE_LINUX_VERSION_H)
    /* Try a simple ATAPI eject */
    i_ret = ioctl( i_fd, CDROMEJECT, 0 );

    if( i_ret != 0 )
    {
        i_ret = EjectSCSI( i_fd );
    }

    if( i_ret != 0 )
    {
        msg_Err( p_this, "could not eject %s", psz_device );
    }

#elif defined (HAVE_DVD_H)
    i_ret = ioctl( i_fd, CDROMEJECT, 0 );

#else
    msg_Warn( p_this, "CD-ROM ejection unsupported on this platform" );
    i_ret = -1;

#endif
    close( i_fd );

    return i_ret;
#endif
}

/* The following functions are local */

#if defined(__linux__) && defined(HAVE_LINUX_VERSION_H)
/*****************************************************************************
 * Eject using SCSI commands. Return 0 if successful
 *****************************************************************************/
/**
 * \brief Ejects the CD /DVD using SCSI commands
 * \ingroup vlc_interface
 * This function is local
 * \param i_fd a device nummber
 * \return 0 on success, VLC_EGENERIC on failure
 */
static int EjectSCSI( int i_fd )
{
    int i_status;

    struct sdata
    {
        int  inlen;
        int  outlen;
        char cmd[256];
    } scsi_cmd;

    scsi_cmd.inlen  = 0;
    scsi_cmd.outlen = 0;
    scsi_cmd.cmd[0] = ALLOW_MEDIUM_REMOVAL;
    scsi_cmd.cmd[1] = 0;
    scsi_cmd.cmd[2] = 0;
    scsi_cmd.cmd[3] = 0;
    scsi_cmd.cmd[4] = 0;
    scsi_cmd.cmd[5] = 0;
    i_status = ioctl( i_fd, SCSI_IOCTL_SEND_COMMAND, (void *)&scsi_cmd );
    if( i_status != 0 )
    {
        return VLC_EGENERIC;
    }

    scsi_cmd.inlen  = 0;
    scsi_cmd.outlen = 0;
    scsi_cmd.cmd[0] = START_STOP;
    scsi_cmd.cmd[1] = 0;
    scsi_cmd.cmd[2] = 0;
    scsi_cmd.cmd[3] = 0;
    scsi_cmd.cmd[4] = 1;
    scsi_cmd.cmd[5] = 0;
    i_status = ioctl( i_fd, SCSI_IOCTL_SEND_COMMAND, (void *)&scsi_cmd );
    if( i_status != 0 )
    {
        return VLC_EGENERIC;
    }

    scsi_cmd.inlen  = 0;
    scsi_cmd.outlen = 0;
    scsi_cmd.cmd[0] = START_STOP;
    scsi_cmd.cmd[1] = 0;
    scsi_cmd.cmd[2] = 0;
    scsi_cmd.cmd[3] = 0;
    scsi_cmd.cmd[4] = 2;
    scsi_cmd.cmd[5] = 0;
    i_status = ioctl( i_fd, SCSI_IOCTL_SEND_COMMAND, (void *)&scsi_cmd );
    if( i_status != 0 )
    {
        return VLC_EGENERIC;
    }

    /* Force kernel to reread partition table when new disc inserted */
    i_status = ioctl( i_fd, BLKRRPART );

    return i_status;
}
#endif

