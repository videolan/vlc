/****************************************************************************
 * cdrom_internals.h: cdrom tools private header
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

/*****************************************************************************
 * The vcddev structure
 *****************************************************************************/
struct vcddev_s
{
    char   *psz_dev;                                      /* vcd device name */

    /* Section used in vcd image mode */
    int    i_vcdimage_handle;                   /* vcd image file descriptor */
    int    i_tracks;                          /* number of tracks of the vcd */
    int    *p_sectors;                           /* tracks layout on the vcd */

    /* Section used in vcd device mode */

#ifdef WIN32
    HANDLE h_device_handle;                         /* vcd device descriptor */
    long  hASPI;
    short i_sid;
    long  (*lpSendCommand)( void* );

#else
    int    i_device_handle;                         /* vcd device descriptor */
#endif

};


/*****************************************************************************
 * Misc. Macros
 *****************************************************************************/
/* LBA = msf.frame + 75 * ( msf.second + 60 * msf.minute ) */
#define MSF_TO_LBA(min, sec, frame) ((int)frame + 75 * (sec + 60 * min))
/* LBA = msf.frame + 75 * ( msf.second - 2 + 60 * msf.minute ) */
#define MSF_TO_LBA2(min, sec, frame) ((int)frame + 75 * (sec -2 + 60 * min))

#ifndef O_BINARY
#   define O_BINARY 0
#endif

#define VCDDEV_T 1

/*****************************************************************************
 * Platform specifics
 *****************************************************************************/
#if defined( __APPLE__ )
#define darwin_freeTOC( p ) free( (void*)p )
#define CD_MIN_TRACK_NO 01
#define CD_MAX_TRACK_NO 99
#endif

#if defined( WIN32 )

/* Win32 DeviceIoControl specifics */
#ifndef MAXIMUM_NUMBER_TRACKS
#    define MAXIMUM_NUMBER_TRACKS 100
#endif
typedef struct _TRACK_DATA {
    UCHAR Reserved;
    UCHAR Control : 4;
    UCHAR Adr : 4;
    UCHAR TrackNumber;
    UCHAR Reserved1;
    UCHAR Address[4];
} TRACK_DATA, *PTRACK_DATA;
typedef struct _CDROM_TOC {
    UCHAR Length[2];
    UCHAR FirstTrack;
    UCHAR LastTrack;
    TRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS];
} CDROM_TOC, *PCDROM_TOC;
typedef enum _TRACK_MODE_TYPE {
    YellowMode2,
    XAForm2,
    CDDA
} TRACK_MODE_TYPE, *PTRACK_MODE_TYPE;
typedef struct __RAW_READ_INFO {
    LARGE_INTEGER DiskOffset;
    ULONG SectorCount;
    TRACK_MODE_TYPE TrackMode;
} RAW_READ_INFO, *PRAW_READ_INFO;

#ifndef IOCTL_CDROM_BASE
#    define IOCTL_CDROM_BASE FILE_DEVICE_CD_ROM
#endif
#ifndef IOCTL_CDROM_READ_TOC
#    define IOCTL_CDROM_READ_TOC CTL_CODE(IOCTL_CDROM_BASE, 0x0000, \
                                          METHOD_BUFFERED, FILE_READ_ACCESS)
#endif
#ifndef IOCTL_CDROM_RAW_READ
#define IOCTL_CDROM_RAW_READ CTL_CODE(IOCTL_CDROM_BASE, 0x000F, \
                                      METHOD_OUT_DIRECT, FILE_READ_ACCESS)
#endif

/* Win32 aspi specific */
#define WIN_NT               ( GetVersion() < 0x80000000 )
#define ASPI_HAID           0
#define ASPI_TARGET         0
#define DTYPE_CDROM         0x05

#define SENSE_LEN           0x0E
#define SC_GET_DEV_TYPE     0x01
#define SC_EXEC_SCSI_CMD    0x02
#define SC_GET_DISK_INFO    0x06
#define SS_COMP             0x01
#define SS_PENDING          0x00
#define SS_NO_ADAPTERS      0xE8
#define SRB_DIR_IN          0x08
#define SRB_DIR_OUT         0x10
#define SRB_EVENT_NOTIFY    0x40

#define READ_CD 0xbe
#define SECTOR_TYPE_MODE2_FORM2 0x14
#define SECTOR_TYPE_CDDA 0x04
#define READ_CD_RAW_MODE2 0xF0
#define READ_CD_USERDATA 0x10

#define READ_TOC 0x43
#define READ_TOC_FORMAT_TOC 0x0

#pragma pack(1)

struct SRB_GetDiskInfo
{
    unsigned char   SRB_Cmd;
    unsigned char   SRB_Status;
    unsigned char   SRB_HaId;
    unsigned char   SRB_Flags;
    unsigned long   SRB_Hdr_Rsvd;
    unsigned char   SRB_Target;
    unsigned char   SRB_Lun;
    unsigned char   SRB_DriveFlags;
    unsigned char   SRB_Int13HDriveInfo;
    unsigned char   SRB_Heads;
    unsigned char   SRB_Sectors;
    unsigned char   SRB_Rsvd1[22];
};

struct SRB_GDEVBlock
{
    unsigned char SRB_Cmd;
    unsigned char SRB_Status;
    unsigned char SRB_HaId;
    unsigned char SRB_Flags;
    unsigned long SRB_Hdr_Rsvd;
    unsigned char SRB_Target;
    unsigned char SRB_Lun;
    unsigned char SRB_DeviceType;
    unsigned char SRB_Rsvd1;
};

struct SRB_ExecSCSICmd
{
    unsigned char   SRB_Cmd;
    unsigned char   SRB_Status;
    unsigned char   SRB_HaId;
    unsigned char   SRB_Flags;
    unsigned long   SRB_Hdr_Rsvd;
    unsigned char   SRB_Target;
    unsigned char   SRB_Lun;
    unsigned short  SRB_Rsvd1;
    unsigned long   SRB_BufLen;
    unsigned char   *SRB_BufPointer;
    unsigned char   SRB_SenseLen;
    unsigned char   SRB_CDBLen;
    unsigned char   SRB_HaStat;
    unsigned char   SRB_TargStat;
    unsigned long   *SRB_PostProc;
    unsigned char   SRB_Rsvd2[20];
    unsigned char   CDBByte[16];
    unsigned char   SenseArea[SENSE_LEN+2];
};

#pragma pack()
#endif /* WIN32 */


/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/
static int    OpenVCDImage( vlc_object_t *, const char *, struct vcddev_s * );
static void   CloseVCDImage( vlc_object_t *, struct vcddev_s * );

#if defined( __APPLE__ )
static CDTOC *darwin_getTOC( vlc_object_t *, const struct vcddev_s * );
static int    darwin_getNumberOfTracks( CDTOC *, int );

#elif defined( WIN32 )
static int    win32_vcd_open( vlc_object_t *, const char *, struct vcddev_s *);
#endif
