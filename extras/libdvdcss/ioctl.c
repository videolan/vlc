/*****************************************************************************
 * ioctl.c: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ioctl.c,v 1.15 2001/12/09 17:01:35 sam Exp $
 *
 * Authors: Markus Kuespert <ltlBeBoy@beosmail.com>
 *          Samuel Hocevar <sam@zoy.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
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
#include "defs.h"

#include <stdio.h>

#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>

#if defined( WIN32 )
#   include <windows.h>
#   include <winioctl.h>
#else
#   include <netinet/in.h>
#   include <sys/ioctl.h>
#endif

#ifdef DVD_STRUCT_IN_SYS_CDIO_H
#   include <sys/cdio.h>
#endif
#ifdef DVD_STRUCT_IN_SYS_DVDIO_H
#   include <sys/dvdio.h>
#endif
#ifdef DVD_STRUCT_IN_LINUX_CDROM_H
#   include <linux/cdrom.h>
#endif
#ifdef DVD_STRUCT_IN_DVD_H
#   include <dvd.h>
#endif
#ifdef DVD_STRUCT_IN_BSDI_DVDIOCTL_DVD_H
#   include <BSDI_dvdioctl/dvd.h>
#endif
#ifdef SYS_BEOS
#   include <malloc.h>
#   include <scsi.h>
#endif
#ifdef SOLARIS_USCSI
#   include <unistd.h>
#   include <stropts.h>
#   include <sys/scsi/scsi_types.h>
#   include <sys/scsi/impl/uscsi.h>
#endif

#include "common.h"

#ifdef SYS_DARWIN
#   include <IOKit/storage/IODVDMediaBSDClient.h>
/* #   include "DVDioctl/DVDioctl.h" */
#endif

#include "ioctl.h"


/*****************************************************************************
 * Local prototypes, BeOS specific
 *****************************************************************************/
#if defined( SYS_BEOS )
static void BeInitRDC ( raw_device_command *, int );
#endif

/*****************************************************************************
 * Local prototypes, Solaris specific
 *****************************************************************************/
#if defined( SOLARIS_USCSI )
static void SolarisInitUSCSI( struct uscsi_cmd *p_sc, int i_type );
#endif

/*****************************************************************************
 * Local prototypes, win32 (aspi) specific
 *****************************************************************************/
#if defined( WIN32 )
static void WinInitSSC ( struct SRB_ExecSCSICmd *, int );
static int  WinSendSSC ( int, struct SRB_ExecSCSICmd * );
#endif

/*****************************************************************************
 * ioctl_ReadCopyright: check whether the disc is encrypted or not
 *****************************************************************************/
int ioctl_ReadCopyright( int i_fd, int i_layer, int *pi_copyright )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_struct dvd;

    dvd.type = DVD_STRUCT_COPYRIGHT;
    dvd.copyright.layer_num = i_layer;

    i_ret = ioctl( i_fd, DVD_READ_STRUCT, &dvd );

    *pi_copyright = dvd.copyright.cpst;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_struct dvd;

    dvd.format = DVD_STRUCT_COPYRIGHT;
    dvd.layer_num = i_layer;

    i_ret = ioctl( i_fd, DVDIOCREADSTRUCTURE, &dvd );

    *pi_copyright = dvd.cpst;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_READ_DVD_STRUCTURE, 8 );

    rdc.command[ 6 ] = i_layer;
    rdc.command[ 7 ] = DVD_STRUCT_COPYRIGHT;

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    *pi_copyright = p_buffer[ 4 ];

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_READ_DVD_STRUCTURE, 8 );
    
    rs_cdb.cdb_opaque[ 6 ] = i_layer;
    rs_cdb.cdb_opaque[ 7 ] = DVD_STRUCT_COPYRIGHT;

    i_ret = ioctl(i_fd, USCSICMD, &sc);

    if( i_ret < 0 || sc.uscsi_status ) {
        i_ret = -1;
    }
    
    *pi_copyright = p_buffer[ 4 ];
    /* s->copyright.rmi = p_buffer[ 5 ]; */

#elif defined( SYS_DARWIN )
    dk_dvd_read_structure_t dvd;
    DVDCopyrightInfo dvdcpi;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdcpi, 0, sizeof(dvdcpi));

    dvd.buffer = &dvdcpi;
    dvd.bufferLength = sizeof(dvdcpi);
    dvd.format = kDVDStructureFormatCopyrightInfo;
    dvd.layer = i_layer;

    /* dvdcpi.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    /* dvdcpi.dataLength[1] = 0x06; */
    
    i_ret = ioctl( i_fd, DKIOCDVDREADSTRUCTURE, &dvd );

    *pi_copyright = dvdcpi.copyrightProtectionSystemType;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;
        u8 p_buffer[ 8 ];
        SCSI_PASS_THROUGH_DIRECT sptd;

        memset( &sptd, 0, sizeof( sptd ) );
        memset( &p_buffer, 0, sizeof( p_buffer ) );
   
        /*  When using IOCTL_DVD_READ_STRUCTURE and 
            DVD_COPYRIGHT_DESCRIPTOR, CopyrightProtectionType
            is always 6. So we send a raw scsi command instead. */

        sptd.Length             = sizeof( SCSI_PASS_THROUGH_DIRECT );
        sptd.CdbLength          = 12;
        sptd.DataIn             = SCSI_IOCTL_DATA_IN;
        sptd.DataTransferLength = 8;
        sptd.TimeOutValue       = 2;
        sptd.DataBuffer         = p_buffer;
        sptd.Cdb[ 0 ]           = GPCMD_READ_DVD_STRUCTURE;
        sptd.Cdb[ 6 ]           = i_layer;
        sptd.Cdb[ 7 ]           = DVD_STRUCT_COPYRIGHT;
        sptd.Cdb[ 8 ]           = (8 >> 8) & 0xff;
        sptd.Cdb[ 9 ]           =  8       & 0xff;

        i_ret = DeviceIoControl( (HANDLE) i_fd,
                             IOCTL_SCSI_PASS_THROUGH_DIRECT,
                             &sptd, sizeof( SCSI_PASS_THROUGH_DIRECT ),
                             &sptd, sizeof( SCSI_PASS_THROUGH_DIRECT ),
                             &tmp, NULL ) ? 0 : -1;

        *pi_copyright = p_buffer[4];
    }
    else
    {
        INIT_SSC( GPCMD_READ_DVD_STRUCTURE, 8 );

        ssc.CDBByte[ 6 ] = i_layer;
        ssc.CDBByte[ 7 ] = DVD_STRUCT_COPYRIGHT;

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_copyright = p_buffer[ 4 ];
    }

#elif defined( __QNXNTO__ )
    /*
        QNX RTOS currently doesn't have a CAM
        interface (they're working on it though).
        Assume DVD is not encrypted.
    */

    *pi_copyright = 0;
    i_ret = 0;

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadDiscKey: get the disc key
 *****************************************************************************/
int ioctl_ReadDiscKey( int i_fd, int *pi_agid, u8 *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_struct dvd;

    dvd.type = DVD_STRUCT_DISCKEY;
    dvd.disckey.agid = *pi_agid;
    memset( dvd.disckey.value, 0, 2048 );

    i_ret = ioctl( i_fd, DVD_READ_STRUCT, &dvd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, dvd.disckey.value, 2048 );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_struct dvd;

    dvd.format = DVD_STRUCT_DISCKEY;
    dvd.agid = *pi_agid;
    memset( dvd.data, 0, 2048 );

    i_ret = ioctl( i_fd, DVDIOCREADSTRUCTURE, &dvd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, dvd.data, 2048 );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_READ_DVD_STRUCTURE, 2048 + 4 );

    rdc.command[ 7 ]  = DVD_STRUCT_DISCKEY;
    rdc.command[ 10 ] = *pi_agid << 6;
    
    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, p_buffer + 4, 2048 );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_READ_DVD_STRUCTURE, 2048 + 4 );
    
    rs_cdb.cdb_opaque[ 7 ] = DVD_STRUCT_DISCKEY;
    rs_cdb.cdb_opaque[ 10 ] = *pi_agid << 6;
    
    i_ret = ioctl( i_fd, USCSICMD, &sc );
    
    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
        return i_ret;
    }

    memcpy( p_key, p_buffer + 4, 2048 );

#elif defined( SYS_DARWIN )
    dk_dvd_read_structure_t dvd;
    DVDDiscKeyInfo dvddki;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvddki, 0, sizeof(dvddki));

    dvd.buffer = &dvddki;
    dvd.bufferLength = sizeof(dvddki);
    dvd.format = kDVDStructureFormatDiscKeyInfo;
    dvd.grantID = *pi_agid;

    /* 2048+2 ; maybe we should do bit shifts to value of (sizeof(dvddki)-2) */
    dvddki.dataLength[0] = 0x04;
    dvddki.dataLength[1] = 0x02;

    i_ret = ioctl( i_fd, DKIOCDVDREADSTRUCTURE, &dvd );

    memcpy( p_key, dvddki.discKeyStructures, sizeof(dvddki.discKeyStructures) );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;
        u8 buffer[DVD_DISK_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_DISK_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdDiskKey;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key, 
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {   
            return i_ret;
        }

        memcpy( p_key, key->KeyData, 2048 );
    }
    else
    {
        INIT_SSC( GPCMD_READ_DVD_STRUCTURE, 2048 + 4 );

        ssc.CDBByte[ 7 ]  = DVD_STRUCT_DISCKEY;
        ssc.CDBByte[ 10 ] = *pi_agid << 6;
    
        i_ret = WinSendSSC( i_fd, &ssc );

        if( i_ret < 0 )
        {
            return i_ret;
        }

        memcpy( p_key, p_buffer + 4, 2048 );
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadTitleKey: get the title key
 *****************************************************************************/
int ioctl_ReadTitleKey( int i_fd, int *pi_agid, int i_pos, u8 *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo dvd_ai;

    memset( &dvd_ai, 0, sizeof(dvd_ai) );
    dvd_ai.type = DVD_LU_SEND_TITLE_KEY;
    dvd_ai.lstk.agid = *pi_agid;
    dvd_ai.lstk.lba = i_pos;

    i_ret = ioctl( i_fd, DVD_AUTH, &dvd_ai );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, dvd_ai.lstk.title_key, 5 );

#elif defined( HAVE_BSD_DVD_STRUCT )
    i_ret = -1;

#elif defined( SYS_BEOS )
    i_ret = -1;

#elif defined( SOLARIS_USCSI )
    i_ret = -1;

#elif defined( SYS_DARWIN )
    dk_dvd_report_key_t dvd;
    DVDTitleKeyInfo dvdtki;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdtki, 0, sizeof(dvdtki));

    dvd.buffer = &dvdtki;
    dvd.bufferLength = sizeof(dvdtki);
    dvd.format = kDVDKeyFormatTitleKey;
    dvd.grantID = *pi_agid;
    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM; /* or this - this is memset 0x00 anyways */

    /* dvdtki.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    dvdtki.dataLength[1] = 0x0a;
    
    /* What are DVDTitleKeyInfo.{CP_MOD,CGMS,CP_SEC,CPM} and do they need to be set? */

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    memcpy( p_key, dvdtki.titleKeyValue, sizeof(dvdtki.titleKeyValue) );

#elif defined( WIN32 )
    i_ret = -1;

#else

    i_ret = -1;

#endif

    return i_ret;
}


/*****************************************************************************
 * ioctl_ReportAgid: get AGID from the drive
 *****************************************************************************/
int ioctl_ReportAgid( int i_fd, int *pi_agid )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_AGID;
    auth_info.lsa.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *pi_agid = auth_info.lsa.agid;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    auth_info.format = DVD_REPORT_AGID;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    *pi_agid = auth_info.agid;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 8 );

    rdc.command[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    *pi_agid = p_buffer[ 7 ] >> 6;

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 8 );
    
    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);
    
    i_ret = ioctl( i_fd, USCSICMD, &sc );
    
    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    *pi_agid = p_buffer[ 7 ] >> 6;
    
#elif defined( SYS_DARWIN )
    dk_dvd_report_key_t dvd;
    DVDAuthenticationGrantIDInfo dvdagid;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdagid, 0, sizeof(dvdagid));

    dvd.buffer = &dvdagid;
    dvd.bufferLength = sizeof(dvdagid);
    dvd.format = kDVDKeyFormatAGID_CSS;
    dvd.grantID = *pi_agid;
    dvdagid.grantID = *pi_agid; /* do we need this? */
    dvd.keyClass = kDVDKeyClassCSS_CPPM_CPRM; /* or this - this is memset 0x00 anyways */

    /* dvdagid.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    /* dvdagid.dataLength[1] = 0x06; */

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    *pi_agid = dvdagid.grantID;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        ULONG id;
        DWORD tmp;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_START_SESSION, 
                        &tmp, 4, &id, sizeof( id ), &tmp, NULL ) ? 0 : -1;

        *pi_agid = id;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_agid = p_buffer[ 7 ] >> 6;
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportChallenge: get challenge from the drive
 *****************************************************************************/
int ioctl_ReportChallenge( int i_fd, int *pi_agid, u8 *p_challenge )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_CHALLENGE;
    auth_info.lsc.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    memcpy( p_challenge, auth_info.lsc.chal, sizeof(dvd_challenge) );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    auth_info.format = DVD_REPORT_CHALLENGE;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    memcpy( p_challenge, auth_info.keychal, 10 );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 16 );

    rdc.command[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    memcpy( p_challenge, p_buffer + 4, 12 );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 16 );
    
    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);
    
    i_ret = ioctl( i_fd, USCSICMD, &sc );
    
    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    memcpy( p_challenge, p_buffer + 4, 12 );
    
#elif defined( SYS_DARWIN )
    dk_dvd_report_key_t dvd;
    DVDChallengeKeyInfo dvdcki;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdcki, 0, sizeof(dvdcki));

    dvd.buffer = &dvdcki;
    dvd.bufferLength = sizeof(dvdcki);
    dvd.format = kDVDKeyFormatChallengeKey;
    dvd.grantID = *pi_agid;

    /* dvdcki.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    dvdcki.dataLength[1] = 0x0e;

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    memcpy( p_challenge, dvdcki.challengeKeyValue, sizeof(dvdcki.challengeKeyValue) );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;
        u8 buffer[DVD_CHALLENGE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_CHALLENGE_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdChallengeKey;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key, 
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        memcpy( p_challenge, key->KeyData, 10 );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 16 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_challenge, p_buffer + 4, 12 );
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportASF: get ASF from the drive
 *****************************************************************************/
int ioctl_ReportASF( int i_fd, int *pi_agid, int *pi_asf )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_ASF;
    auth_info.lsasf.agid = *pi_agid;
    auth_info.lsasf.asf = *pi_asf;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *pi_asf = auth_info.lsasf.asf;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    auth_info.format = DVD_REPORT_ASF;
    auth_info.agid = *pi_agid;
    auth_info.asf = *pi_asf;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    *pi_asf = auth_info.asf;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 8 );

    rdc.command[ 10 ] = DVD_REPORT_ASF | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    *pi_asf = p_buffer[ 7 ] & 1;

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 8 );
    
    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_ASF | (*pi_agid << 6);
    
    i_ret = ioctl( i_fd, USCSICMD, &sc );
    
    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    *pi_asf = p_buffer[ 7 ] & 1;
    
#elif defined( SYS_DARWIN )
    dk_dvd_report_key_t dvd;
    DVDAuthenticationSuccessFlagInfo dvdasfi;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdasfi, 0, sizeof(dvdasfi));

    dvd.buffer = &dvdasfi;
    dvd.bufferLength = sizeof(dvdasfi);
    dvd.format = kDVDKeyFormatASF;
    dvd.grantID = *pi_agid;
    dvdasfi.successFlag = *pi_asf;

    /* dvdasfi.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    dvdasfi.dataLength[1] = 0x06;
    
    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    *pi_asf = dvdasfi.successFlag;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;
        u8 buffer[DVD_ASF_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_ASF_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdAsf;
        key->KeyFlags   = 0;

        ((PDVD_ASF)key->KeyData)->SuccessFlag = *pi_asf;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key, 
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        *pi_asf = ((PDVD_ASF)key->KeyData)->SuccessFlag;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_ASF | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_asf = p_buffer[ 7 ] & 1;
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportKey1: get the first key from the drive
 *****************************************************************************/
int ioctl_ReportKey1( int i_fd, int *pi_agid, u8 *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_KEY1;
    auth_info.lsk.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    memcpy( p_key, auth_info.lsk.key, sizeof(dvd_key) );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    auth_info.format = DVD_REPORT_KEY1;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    memcpy( p_key, auth_info.keychal, 8 );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 12 );

    rdc.command[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

    memcpy( p_key, p_buffer + 4, 8 );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 12 );
    
    rs_cdb.cdb_opaque[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);
    
    i_ret = ioctl( i_fd, USCSICMD, &sc );
    
    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

    memcpy( p_key, p_buffer + 4, 8 );;
    
#elif defined( SYS_DARWIN )
    dk_dvd_report_key_t dvd;
    DVDKey1Info dvdk1i;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdk1i, 0, sizeof(dvdk1i));

    dvd.buffer = &dvdk1i;
    dvd.bufferLength = sizeof(dvdk1i);
    dvd.format = kDVDKeyFormatKey1;
    dvd.grantID = *pi_agid;
    
    /* dvdk1i.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    dvdk1i.dataLength[1] = 0x0a;

    i_ret = ioctl( i_fd, DKIOCDVDREPORTKEY, &dvd );

    memcpy( p_key, dvdk1i.key1Value, sizeof(dvdk1i.key1Value) );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;
        u8 buffer[DVD_BUS_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_BUS_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdBusKey1;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key, 
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        memcpy( p_key, key->KeyData, 8 );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 12 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_key, p_buffer + 4, 8 );
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_InvalidateAgid: invalidate the current AGID
 *****************************************************************************/
int ioctl_InvalidateAgid( int i_fd, int *pi_agid )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    auth_info.type = DVD_INVALIDATE_AGID;
    auth_info.lsa.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *pi_agid = auth_info.lsa.agid;

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    auth_info.format = DVD_INVALIDATE_AGID;
    auth_info.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVDIOCREPORTKEY, &auth_info );

    *pi_agid = auth_info.agid;

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_REPORT_KEY, 0 );

    rdc.command[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

    i_ret = ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_REPORT_KEY, 0 );
    
    rs_cdb.cdb_opaque[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);
    
    i_ret = ioctl( i_fd, USCSICMD, &sc );
    
    if( i_ret < 0 || sc.uscsi_status )
    {
        i_ret = -1;
    }

#elif defined( SYS_DARWIN )
    dk_dvd_send_key_t dvd;
    DVDAuthenticationGrantIDInfo dvdagid;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdagid, 0, sizeof(dvdagid));

    dvd.buffer = &dvdagid;
    dvd.bufferLength = sizeof(dvdagid);
    dvd.format = kDVDKeyFormatAGID_Invalidate;
    dvd.grantID = *pi_agid;
    dvdagid.grantID = *pi_agid; /* we need this? */
    
    /* dvdagid.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    /* dvdagid.dataLength[1] = 0x06; */

    i_ret = ioctl( i_fd, DKIOCDVDSENDKEY, &dvd );

    *pi_agid = dvdagid.grantID;

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_END_SESSION, 
                    pi_agid, sizeof( *pi_agid ), NULL, 0, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
#if defined( __MINGW32__ )
        INIT_SSC( GPCMD_REPORT_KEY, 0 );
#else
        INIT_SSC( GPCMD_REPORT_KEY, 1 );

        ssc.SRB_BufLen    = 0;
        ssc.CDBByte[ 8 ]  = 0;
        ssc.CDBByte[ 9 ]  = 0;
#endif

        ssc.CDBByte[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendChallenge: send challenge to the drive
 *****************************************************************************/
int ioctl_SendChallenge( int i_fd, int *pi_agid, u8 *p_challenge )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    auth_info.type = DVD_HOST_SEND_CHALLENGE;
    auth_info.hsc.agid = *pi_agid;

    memcpy( auth_info.hsc.chal, p_challenge, sizeof(dvd_challenge) );

    return ioctl( i_fd, DVD_AUTH, &auth_info );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    auth_info.format = DVD_SEND_CHALLENGE;
    auth_info.agid = *pi_agid;

    memcpy( auth_info.keychal, p_challenge, 12 );

    return ioctl( i_fd, DVDIOCSENDKEY, &auth_info );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_SEND_KEY, 16 );

    rdc.command[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, 12 );

    return ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_SEND_KEY, 16 );
    
    rs_cdb.cdb_opaque[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);
    
    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, 12 );
    
    if( ioctl( i_fd, USCSICMD, &sc ) < 0 || sc.uscsi_status )
    {
        return -1;
    }

    return 0;
    
#elif defined( SYS_DARWIN )
    dk_dvd_send_key_t dvd;
    DVDChallengeKeyInfo dvdcki;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdcki, 0, sizeof(dvdcki));

    dvd.buffer = &dvdcki;
    dvd.bufferLength = sizeof(dvdcki);
    dvd.format = kDVDKeyFormatChallengeKey;
    dvd.grantID = *pi_agid;

    /* dvdcki.dataLength[0] = 0x00; */ /* dataLength[0] is already memset to 0 */
    dvdcki.dataLength[1] = 0x0e;

    memcpy( dvdcki.challengeKeyValue, p_challenge,  sizeof(dvdcki.challengeKeyValue) );

    i_ret = ioctl( i_fd, DKIOCDVDSENDKEY, &dvd );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;
        u8 buffer[DVD_CHALLENGE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_CHALLENGE_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdChallengeKey;
        key->KeyFlags   = 0;

        memcpy( key->KeyData, p_challenge, 10 );

        return DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_SEND_KEY, key, 
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 16 );

        ssc.CDBByte[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

        p_buffer[ 1 ] = 0xe;
        memcpy( p_buffer + 4, p_challenge, 12 );

        return WinSendSSC( i_fd, &ssc );
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    return -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendKey2: send the second key to the drive
 *****************************************************************************/
int ioctl_SendKey2( int i_fd, int *pi_agid, u8 *p_key )
{
    int i_ret;

#if defined( HAVE_LINUX_DVD_STRUCT )
    dvd_authinfo auth_info;

    auth_info.type = DVD_HOST_SEND_KEY2;
    auth_info.hsk.agid = *pi_agid;

    memcpy( auth_info.hsk.key, p_key, sizeof(dvd_key) );

    return ioctl( i_fd, DVD_AUTH, &auth_info );

#elif defined( HAVE_BSD_DVD_STRUCT )
    struct dvd_authinfo auth_info;

    auth_info.format = DVD_SEND_KEY2;
    auth_info.agid = *pi_agid;

    memcpy( auth_info.keychal, p_key, 8 );

    return ioctl( i_fd, DVDIOCSENDKEY, &auth_info );

#elif defined( SYS_BEOS )
    INIT_RDC( GPCMD_SEND_KEY, 12 );

    rdc.command[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, 8 );

    return ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );

#elif defined( SOLARIS_USCSI )
    INIT_USCSI( GPCMD_SEND_KEY, 12 );
    
    rs_cdb.cdb_opaque[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);
    
    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, 8 );
    
    if( ioctl( i_fd, USCSICMD, &sc ) < 0 || sc.uscsi_status )
    {
        return -1;
    }

    return 0;
    
#elif defined( SYS_DARWIN )
    dk_dvd_send_key_t dvd;
    DVDKey2Info dvdk2i;
    
    memset(&dvd, 0, sizeof(dvd));
    memset(&dvdk2i, 0, sizeof(dvdk2i));

    dvd.buffer = &dvdk2i;
    dvd.bufferLength = sizeof(dvdk2i);
    dvd.format = kDVDKeyFormatKey2;
    dvd.grantID = *pi_agid;

    /* dvdk2i.dataLength[0] = 0x00; */ /*dataLength[0] is already memset to 0 */
    dvdk2i.dataLength[1] = 0x0a;
    
    memcpy( dvdk2i.key2Value, p_key, sizeof(dvdk2i.key2Value) );

    i_ret = ioctl( i_fd, DKIOCDVDSENDKEY, &dvd );

#elif defined( WIN32 )
    if( WIN2K ) /* NT/Win2000/Whistler */
    {
        DWORD tmp;
        u8 buffer[DVD_BUS_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_BUS_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdBusKey2;
        key->KeyFlags   = 0;

        memcpy( key->KeyData, p_key, 8 );

        return DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_SEND_KEY, key, 
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 12 );

        ssc.CDBByte[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

        p_buffer[ 1 ] = 0xa;
        memcpy( p_buffer + 4, p_key, 8 );

        return WinSendSSC( i_fd, &ssc );
    }

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    return -1;

#endif
    return i_ret;
}

/* Local prototypes */

#if defined( SYS_BEOS )
/*****************************************************************************
 * BeInitRDC: initialize a RDC structure for the BeOS kernel
 *****************************************************************************
 * This function initializes a BeOS raw device command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void BeInitRDC( raw_device_command *p_rdc, int i_type )
{
    memset( p_rdc->data, 0, p_rdc->data_length );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            /* leave the flags to 0 */
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_rdc->flags = B_RAW_DEVICE_DATA_IN;
            break;
    }

    p_rdc->command[ 0 ]      = i_type;

    p_rdc->command[ 8 ]      = (p_rdc->data_length >> 8) & 0xff;
    p_rdc->command[ 9 ]      =  p_rdc->data_length       & 0xff;
    p_rdc->command_length    = 12;

    p_rdc->sense_data        = NULL;
    p_rdc->sense_data_length = 0;

    p_rdc->timeout           = 1000000;
}
#endif

#if defined( SOLARIS_USCSI )
/*****************************************************************************
 * SolarisInitUSCSI: initialize a USCSICMD structure for the Solaris kernel
 *****************************************************************************
 * This function initializes a Solaris userspace scsi command structure for 
 * future use, either a read command or a write command.
 *****************************************************************************/
static void SolarisInitUSCSI( struct uscsi_cmd *p_sc, int i_type )
{   
    union scsi_cdb *rs_cdb;
    memset( p_sc->uscsi_cdb, 0, sizeof( union scsi_cdb ) );
    memset( p_sc->uscsi_bufaddr, 0, p_sc->uscsi_buflen );
    
    switch( i_type )
    {
        case GPCMD_SEND_KEY:
	    p_sc->uscsi_flags = USCSI_ISOLATE | USCSI_WRITE;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
	    p_sc->uscsi_flags = USCSI_ISOLATE | USCSI_READ;
            break;
    }
    
    rs_cdb = (union scsi_cdb *)p_sc->uscsi_cdb;
    
    rs_cdb->scc_cmd = i_type;

    rs_cdb->cdb_opaque[ 8 ] = (p_sc->uscsi_buflen >> 8) & 0xff;
    rs_cdb->cdb_opaque[ 9 ] =  p_sc->uscsi_buflen       & 0xff;
    p_sc->uscsi_cdblen = 12;

    USCSI_TIMEOUT( p_sc, 15 );
}
#endif

#if defined( WIN32 )
/*****************************************************************************
 * WinInitSSC: initialize a ssc structure for the win32 aspi layer
 *****************************************************************************
 * This function initializes a ssc raw device command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void WinInitSSC( struct SRB_ExecSCSICmd *p_ssc, int i_type )
{
    memset( p_ssc->SRB_BufPointer, 0, p_ssc->SRB_BufLen );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_ssc->SRB_Flags = SRB_DIR_OUT;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_ssc->SRB_Flags = SRB_DIR_IN;
            break;
    }

    p_ssc->SRB_Cmd      = SC_EXEC_SCSI_CMD;
    p_ssc->SRB_Flags    |= SRB_EVENT_NOTIFY;

    p_ssc->CDBByte[ 0 ] = i_type;

    p_ssc->CDBByte[ 8 ] = (u8)(p_ssc->SRB_BufLen >> 8) & 0xff;
    p_ssc->CDBByte[ 9 ] = (u8) p_ssc->SRB_BufLen       & 0xff;
    p_ssc->SRB_CDBLen   = 12;

    p_ssc->SRB_SenseLen = SENSE_LEN;
}

/*****************************************************************************
 * WinSendSSC: send a ssc structure to the aspi layer
 *****************************************************************************/
static int WinSendSSC( int i_fd, struct SRB_ExecSCSICmd *p_ssc )
{
    HANDLE hEvent = NULL;
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;

    hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( hEvent == NULL )
    {
        return -1;
    }

    p_ssc->SRB_PostProc  = hEvent;
    p_ssc->SRB_HaId      = LOBYTE( fd->i_sid );
    p_ssc->SRB_Target    = HIBYTE( fd->i_sid );

    ResetEvent( hEvent );
    if( fd->lpSendCommand( (void*) p_ssc ) == SS_PENDING )
        WaitForSingleObject( hEvent, INFINITE );

    CloseHandle( hEvent );

    return p_ssc->SRB_Status == SS_COMP ? 0 : -1;
}
#endif

