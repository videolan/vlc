/*****************************************************************************
 * ioctl.h: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ioctl.h,v 1.9 2001/11/25 22:52:21 gbazin Exp $
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

int ioctl_ReadCopyright     ( int, int, int * );
int ioctl_ReadDiscKey       ( int, int *, u8 * );
int ioctl_ReadTitleKey      ( int, int *, int, u8 * );
int ioctl_ReportAgid        ( int, int * );
int ioctl_ReportChallenge   ( int, int *, u8 * );
int ioctl_ReportKey1        ( int, int *, u8 * );
int ioctl_ReportASF         ( int, int *, int * );
int ioctl_InvalidateAgid    ( int, int * );
int ioctl_SendChallenge     ( int, int *, u8 * );
int ioctl_SendKey2          ( int, int *, u8 * );

/*****************************************************************************
 * Common macro, BeOS specific
 *****************************************************************************/
#if defined( SYS_BEOS )
#define INIT_RDC( TYPE, SIZE ) \
    raw_device_command rdc; \
    u8 p_buffer[ (SIZE) ]; \
    memset( &rdc, 0, sizeof( raw_device_command ) ); \
    rdc.data = (char *)p_buffer; \
    rdc.data_length = (SIZE); \
    BeInitRDC( &rdc, (TYPE) );
#endif

/*****************************************************************************
 * Common macro, Solaris specific
 *****************************************************************************/
#if defined( SOLARIS_USCSI )
#define USCSI_TIMEOUT( SC, TO ) ( (SC)->uscsi_timeout = (TO) )
#define USCSI_RESID( SC )       ( (SC)->uscsi_resid )
#define INIT_USCSI( TYPE, SIZE ) \
    struct uscsi_cmd sc; \
    union scsi_cdb rs_cdb; \
    u8 p_buffer[ (SIZE) ]; \
    memset( &sc, 0, sizeof( struct uscsi_cmd ) ); \
    sc.uscsi_cdb = (caddr_t)&rs_cdb; \
    sc.uscsi_bufaddr = p_buffer; \
    sc.uscsi_buflen = (SIZE); \
    SolarisInitUSCSI( &sc, (TYPE) );
#endif

/*****************************************************************************
 * Common macro, Darwin specific
 *****************************************************************************/
#if defined( SYS_DARWIN )
#define INIT_DVDIOCTL( SIZE ) \
    dvdioctl_data_t dvdioctl; \
    u8 p_buffer[ (SIZE) ]; \
    dvdioctl.p_buffer = p_buffer; \
    dvdioctl.i_size = (SIZE); \
    dvdioctl.i_keyclass = kCSS_CSS2_CPRM; \
    memset( p_buffer, 0, (SIZE) );
#endif

/*****************************************************************************
 * Common macro, win32 (ASPI) specific
 *****************************************************************************/
#if defined( WIN32 )
#define INIT_SSC( TYPE, SIZE ) \
    struct SRB_ExecSCSICmd ssc; \
    u8 p_buffer[ (SIZE) ]; \
    memset( &ssc, 0, sizeof( struct SRB_ExecSCSICmd ) ); \
    ssc.SRB_BufPointer = (char *)p_buffer; \
    ssc.SRB_BufLen = (SIZE); \
    WinInitSSC( &ssc, (TYPE) );
#endif

/*****************************************************************************
 * Various DVD I/O tables
 *****************************************************************************/

#if defined( SYS_BEOS ) || defined( WIN32 ) || defined ( SOLARIS_USCSI )
    /* The generic packet command opcodes for CD/DVD Logical Units,
     * From Table 57 of the SFF8090 Ver. 3 (Mt. Fuji) draft standard. */
#   define GPCMD_READ_DVD_STRUCTURE 0xad
#   define GPCMD_REPORT_KEY         0xa4
#   define GPCMD_SEND_KEY           0xa3
    /* DVD struct types */
#   define DVD_STRUCT_PHYSICAL      0x00
#   define DVD_STRUCT_COPYRIGHT     0x01
#   define DVD_STRUCT_DISCKEY       0x02
#   define DVD_STRUCT_BCA           0x03
#   define DVD_STRUCT_MANUFACT      0x04
    /* Key formats */
#   define DVD_REPORT_AGID          0x00
#   define DVD_REPORT_CHALLENGE     0x01
#   define DVD_SEND_CHALLENGE       0x01
#   define DVD_REPORT_KEY1          0x02
#   define DVD_SEND_KEY2            0x03
#   define DVD_REPORT_ASF           0x05
#   define DVD_INVALIDATE_AGID      0x3f
#endif

#if defined( HAVE_OPENBSD_DVD_STRUCT )

/*****************************************************************************
 * OpenBSD ioctl specific
 *****************************************************************************/
typedef union dvd_struct dvd_struct;
typedef union dvd_authinfo dvd_authinfo;
typedef u_int8_t dvd_key[5];
typedef u_int8_t dvd_challenge[10];
#endif


#if defined( WIN32 )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*****************************************************************************
 * win32 ioctl specific
 *****************************************************************************/

#define IOCTL_DVD_START_SESSION         CTL_CODE(FILE_DEVICE_DVD, 0x0400, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_READ_KEY              CTL_CODE(FILE_DEVICE_DVD, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SEND_KEY              CTL_CODE(FILE_DEVICE_DVD, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_END_SESSION           CTL_CODE(FILE_DEVICE_DVD, 0x0403, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(FILE_DEVICE_CONTROLLER, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define DVD_CHALLENGE_KEY_LENGTH        (12 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_BUS_KEY_LENGTH              (8 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_DISK_KEY_LENGTH             (2048 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_ASF_LENGTH                  (sizeof(DVD_ASF) + sizeof(DVD_COPY_PROTECT_KEY))

#define SCSI_IOCTL_DATA_OUT             0
#define SCSI_IOCTL_DATA_IN              1

typedef ULONG DVD_SESSION_ID, *PDVD_SESSION_ID;

typedef enum
{
    DvdChallengeKey = 0x01,
    DvdBusKey1,
    DvdBusKey2,
    DvdTitleKey,
    DvdAsf,
    DvdSetRpcKey = 0x6,
    DvdGetRpcKey = 0x8,
    DvdDiskKey = 0x80,
    DvdInvalidateAGID = 0x3f
} DVD_KEY_TYPE;

typedef struct _DVD_COPY_PROTECT_KEY
{
    ULONG KeyLength;
    DVD_SESSION_ID SessionId;
    DVD_KEY_TYPE KeyType;
    ULONG KeyFlags;
    union
    {
        struct
        {
            ULONG FileHandle;
            ULONG Reserved;   // used for NT alignment
        };
        LARGE_INTEGER TitleOffset;
    } Parameters;
    UCHAR KeyData[0];
} DVD_COPY_PROTECT_KEY, *PDVD_COPY_PROTECT_KEY;

typedef struct _DVD_ASF
{
    UCHAR Reserved0[3];
    UCHAR SuccessFlag:1;
    UCHAR Reserved1:7;
} DVD_ASF, * PDVD_ASF;

typedef struct _SCSI_PASS_THROUGH_DIRECT
{
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    PVOID DataBuffer;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

/*****************************************************************************
 * win32 aspi specific
 *****************************************************************************/

#define WIN2K               ( GetVersion() < 0x80000000 )
#define ASPI_HAID           0
#define ASPI_TARGET         0

#define SENSE_LEN           0x0E
#define SC_EXEC_SCSI_CMD    0x02
#define SC_GET_DISK_INFO    0x06
#define SS_COMP             0x01
#define SS_PENDING          0x00
#define SS_NO_ADAPTERS      0xE8
#define SRB_DIR_IN          0x08
#define SRB_DIR_OUT         0x10
#define SRB_EVENT_NOTIFY    0x40

struct w32_aspidev
{
    long  hASPI;
    short i_sid;
    int   i_blocks;
    long  (*lpSendCommand)( void* );
};

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

#endif

