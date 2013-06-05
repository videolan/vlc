/****************************************************************************
 * cdrom_internals.h: cdrom tools private header
 *****************************************************************************
 * Copyright (C) 1998-2001 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#ifdef _WIN32
    HANDLE h_device_handle;                         /* vcd device descriptor */
#elif defined( __OS2__ )
    HFILE  hcd;                                     /* vcd device descriptor */
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

#if defined( _WIN32 )

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
typedef struct _CDROM_READ_TOC_EX {
  UCHAR  Format : 4;
  UCHAR  Reserved1 : 3;
  UCHAR  Msf : 1;
  UCHAR  SessionTrack;
  UCHAR  Reserved2;
  UCHAR  Reserved3;
} CDROM_READ_TOC_EX, *PCDROM_READ_TOC_EX;

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
#define IOCTL_CDROM_READ_TOC_EX CTL_CODE(IOCTL_CDROM_BASE, 0x0015, \
                                         METHOD_BUFFERED, FILE_READ_ACCESS)


#define MINIMUM_CDROM_READ_TOC_EX_SIZE    2
#define CDROM_READ_TOC_EX_FORMAT_CDTEXT   0x05

#endif /* _WIN32 */

#ifdef __OS2__
#pragma pack( push, 1 )
typedef struct os2_msf_s
{
    unsigned char frame;
    unsigned char second;
    unsigned char minute;
    unsigned char reserved;
} os2_msf_t;

typedef struct cdrom_get_tochdr_s
{
    unsigned char sign[4];
} cdrom_get_tochdr_t;

typedef struct cdrom_tochdr_s
{
    unsigned char first_track;
    unsigned char last_track;
    os2_msf_t     lead_out;
} cdrom_tochdr_t;

typedef struct cdrom_get_track_s
{
    unsigned char sign[4];
    unsigned char track;
} cdrom_get_track_t;

typedef struct cdrom_track_s
{
    os2_msf_t     start;
    unsigned char adr:4;
    unsigned char control:4;
} cdrom_track_t;

typedef struct cdrom_readlong_s
{
    unsigned char  sign[4];
    unsigned char  addr_mode;
    unsigned short sectors;
    unsigned long  start;
    unsigned char  reserved;
    unsigned char  interleaved_size;
} cdrom_readlong_t;

#pragma pack( pop )
#endif

#define SECTOR_TYPE_MODE2_FORM2 0x14
#define SECTOR_TYPE_CDDA 0x04
#define READ_CD_RAW_MODE2 0xF0
#define READ_CD_USERDATA 0x10

/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/
static int    OpenVCDImage( vlc_object_t *, const char *, struct vcddev_s * );
static void   CloseVCDImage( vlc_object_t *, struct vcddev_s * );

#if defined( __APPLE__ )
static CDTOC *darwin_getTOC( vlc_object_t *, const struct vcddev_s * );
static int    darwin_getNumberOfTracks( CDTOC *, int );

#elif defined( _WIN32 )
static int    win32_vcd_open( vlc_object_t *, const char *, struct vcddev_s *);

#elif defined( __OS2__ )
static int    os2_vcd_open( vlc_object_t *, const char *, struct vcddev_s *);
#endif
