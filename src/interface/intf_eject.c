/*****************************************************************************
 * intf_eject.c: CD/DVD-ROM ejection handling functions
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: intf_eject.c,v 1.14 2002/06/01 16:45:35 sam Exp $
 *
 * Author: Julien Blache <jb@technologeek.org> for the Linux part
 *               with code taken from the Linux "eject" command
 *         Jon Lech Johansen <jon-vl@nanocrew.net> for Darwin
 *         Xavier Marchesini <xav@alarue.net> for Win32
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

#include <vlc/vlc.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#include <string.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_DVD_H
#   include <dvd.h>
#endif

#if defined(SYS_LINUX) && defined(HAVE_LINUX_VERSION_H)
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

#ifdef WIN32 
#   include <windows.h>
#   include <stdio.h>
#   include <winioctl.h>
#   include <ctype.h>
#   include <tchar.h>

/* define the structures to eject under Win95/98/Me */

#if !defined (VWIN32_DIOC_DOS_IOCTL)
#define VWIN32_DIOC_DOS_IOCTL      1

typedef struct _DIOC_REGISTERS {
    DWORD reg_EBX;
    DWORD reg_EDX;
    DWORD reg_ECX;
    DWORD reg_EAX;
    DWORD reg_EDI;
    DWORD reg_ESI;
    DWORD reg_Flags;
} DIOC_REGISTERS, *PDIOC_REGISTERS;
#endif    /* VWIN32_DIOC_DOS_IOCTL */

#endif


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#if defined(SYS_LINUX) && defined(HAVE_LINUX_VERSION_H)
static int EjectSCSI ( int i_fd );
#endif

/*****************************************************************************
 * intf_Eject: eject the CDRom
 *****************************************************************************
 * returns 0 on success
 * returns 1 on failure
 * returns -1 if not implemented
 *****************************************************************************/
int intf_Eject( vlc_object_t *p_this, const char *psz_device )
{
    int i_ret;

#ifdef SYS_DARWIN
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

    return 1;

#elif defined(WIN32) 
    
    HANDLE h_drive ;
    TCHAR  psz_drive_id[8] ;
    DWORD  dw_access_flags = GENERIC_READ ;
    DWORD  dw_result ;
    LPTSTR psz_volume_format = TEXT("\\\\.\\%s") ;
    BYTE   by_drive ;
    DIOC_REGISTERS regs = {0} ;
    
    /* Win2K ejection code */
    if ( GetVersion() < 0x80000000 )
    {
        wsprintf(psz_drive_id, psz_volume_format, psz_device) ;
         
        msg_Dbg( p_this, "ejecting drive %s", psz_drive_id );
        
        /* Create the file handle */ 
        h_drive = CreateFile(  psz_drive_id, 
                               dw_access_flags, 
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               0,
                               NULL );

        if (h_drive == INVALID_HANDLE_VALUE )
        {
            msg_Err( p_this, "could not create handle for device %s",
                             psz_device );
        }

        i_ret = DeviceIoControl ( h_drive, 
                                 IOCTL_STORAGE_EJECT_MEDIA,
                                 NULL, 0,
                                 NULL, 0, 
                                 &dw_result, 
                                 NULL);
        return (i_ret) ;
    }
    else        /* Win95/98/ME */
    {
        /* Create the handle to VWIN32 */
        h_drive = CreateFile ("\\\\.\\vwin32", 0, 0, NULL, 0,
                              FILE_FLAG_DELETE_ON_CLOSE, NULL ) ;
        
        /* Convert logical disk name to DOS-like disk name */
        by_drive = (toupper (*psz_device) - 'A') + 1;

        /* Let's eject now : Int 21H function 440DH minor code 49h*/
        regs.reg_EAX = 0x440D ;                        
        regs.reg_EBX = by_drive ;
        regs.reg_ECX = MAKEWORD(0x49 , 0x08) ;        // minor code

        i_ret = DeviceIoControl (h_drive, VWIN32_DIOC_DOS_IOCTL,
                                 &regs, sizeof(regs), &regs, sizeof(regs),
                                 &dw_result, 0) ;

        CloseHandle (h_drive) ;
        return (i_ret) ;
    }
#else   /* WIN32 */
    
    int i_fd;

    /* This code could be extended to support CD/DVD-ROM chargers */

    i_fd = open( psz_device, O_RDONLY | O_NONBLOCK );
   
    if( i_fd == -1 )
    {
        msg_Err( p_this, "could not open device %s", psz_device );
        return VLC_EGENERIC;
    }

#if defined(SYS_LINUX) && defined(HAVE_LINUX_VERSION_H)
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
    msg_Warn( p_this, "CD-Rom ejection unsupported on this platform" );
    i_ret = -1;

#endif
    close( i_fd );

    return i_ret;
#endif
}

/* The following functions are local */

#if defined(SYS_LINUX) && defined(HAVE_LINUX_VERSION_H)
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

