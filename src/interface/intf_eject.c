/*****************************************************************************
 * intf_eject.c: CD/DVD-ROM ejection handling functions
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: intf_eject.c,v 1.3 2002/01/11 03:07:36 sam Exp $
 *
 * Author: Julien Blache <jb@technologeek.org> for the Linux part
 *               with code taken from the Linux "eject" command
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <videolan/vlc.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_DVD_H
#   include <dvd.h>
#endif

#ifdef SYS_LINUX
#   include <linux/version.h>
    /* handy macro found in 2.1 kernels, but not in older ones */
#   ifndef KERNEL_VERSION
#       define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#   endif

#   include <sys/types.h>
#   include <sys/stat.h>
#   include <sys/ioctl.h>

#   include <sys/ioctl.h>
#   include <linux/cdrom.h>
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)
#       include <linux/ucdrom.h>
#   endif

#   include <sys/mount.h>
#   include <scsi/scsi.h>
#   include <scsi/sg.h>
#   include <scsi/scsi_ioctl.h>
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef SYS_LINUX
static int EjectSCSI ( int i_fd );
#endif

/*****************************************************************************
 * intf_Eject: eject the CDRom
 *****************************************************************************
 * returns 0 on success
 * returns 1 on failure
 * returns -1 if not implemented
 *****************************************************************************/
int intf_Eject( const char *psz_device )
{
    int i_ret;

    /* This code could be extended to support CD/DVD-ROM chargers */
    int i_fd = 0;

    i_fd = open( psz_device, O_RDONLY | O_NONBLOCK );
   
    if( i_fd == -1 )
    {
        intf_ErrMsg( "intf error: couldn't open device %s", psz_device );
        return 1;
    }

#ifdef SYS_LINUX
    /* Try a simple ATAPI eject */
    i_ret = ioctl( i_fd, CDROMEJECT, 0 );

    if( i_ret != 0 )
    {
        i_ret = EjectSCSI( i_fd );
    }

    if( i_ret != 0 )
    {
        intf_ErrMsg( "intf error: couldn't eject %s", psz_device );
    }

#elif defined (HAVE_DVD_H)
    i_ret = ioctl( i_fd, CDROMEJECT, 0 );

#else
    intf_ErrMsg( "intf error: CD-Rom ejection unsupported on this platform" );
    i_ret = -1;

#endif
    close( i_fd );

    return i_ret;
}

/* The following functions are local */

#ifdef SYS_LINUX
/*****************************************************************************
 * Eject using SCSI commands. Return 0 if successful
 *****************************************************************************/
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
        return 1;
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
        return 1;
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
        return 1;
    }
  
    /* Force kernel to reread partition table when new disc inserted */
    i_status = ioctl( i_fd, BLKRRPART );
  
    return i_status;
}
#endif

