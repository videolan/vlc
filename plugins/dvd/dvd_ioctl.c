/*****************************************************************************
 * dvd_ioctl.c: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ioctl.c,v 1.13 2001/05/25 13:20:09 sam Exp $
 *
 * Authors: Markus Kuespert <ltlBeBoy@beosmail.com>
 *          Samuel Hocevar <sam@zoy.org>
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

#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>
#include <netinet/in.h>

#include <sys/ioctl.h>

#ifdef DVD_STRUCT_IN_SYS_CDIO_H
#   include <sys/cdio.h>
#endif
#ifdef DVD_STRUCT_IN_SYS_DVDIO_H
#   include <sys/dvdio.h>
#endif
#ifdef DVD_STRUCT_IN_LINUX_CDROM_H
#   include <linux/cdrom.h>
#endif
#ifdef SYS_BEOS
#   include <malloc.h>
#   include <scsi.h>
#endif

#include "common.h"

#include "intf_msg.h"

#ifdef SYS_DARWIN1_3
#   include "DVDioctl/DVDioctl.h"
#endif

#include "dvd_css.h"
#include "dvd_ioctl.h"

/*****************************************************************************
 * Local prototypes, BeOS specific
 *****************************************************************************/
#if defined( SYS_BEOS )
static void BeInitRDC ( raw_device_command *, int );
#define INIT_RDC( TYPE, SIZE ) \
    raw_device_command rdc; \
    u8 p_buffer[ (SIZE) ]; \
    rdc.data = (char *)p_buffer; \
    rdc.data_length = (SIZE); \
    BeInitRDC( &rdc, (TYPE) );
#endif

/*****************************************************************************
 * Local prototypes, Darwin specific
 *****************************************************************************/
#if defined( SYS_DARWIN1_3 )
#define INIT_DVDIOCTL( SIZE ) \
    dvdioctl_data_t dvdioctl; \
    u8 p_buffer[ (SIZE) ]; \
    dvdioctl.p_buffer = p_buffer; \
    dvdioctl.i_size = (SIZE); \
    dvdioctl.i_keyclass = kCSS_CSS2_CPRM; \
    memset( p_buffer, 0, (SIZE) );
#endif

/*****************************************************************************
 * ioctl_ReadCopyright: check whether the disc is encrypted or not
 *****************************************************************************/
int ioctl_ReadCopyright( int i_fd, int i_layer, int *pi_copyright )
{
    int i_ret;

#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    intf_ErrMsg( "css error: DVD ioctls not fully functional yet" );
    intf_ErrMsg( "css error: assuming disc is encrypted" );

    *pi_copyright = 1;

    i_ret = 0;

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    i_ret = -1;

#endif
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadKey: get the disc key
 *****************************************************************************/
int ioctl_ReadKey( int i_fd, int *pi_agid, u8 *p_key )
{
    int i_ret;

#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    intf_ErrMsg( "css error: DVD ioctls not fully functional yet" );
    intf_ErrMsg( "css error: sending an empty key" );

    i_ret = 0;

    memset( p_key, 0x00, 2048 );

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
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

#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    INIT_DVDIOCTL( 8 );

    dvdioctl.i_keyformat = kCSSAGID;
    dvdioctl.i_agid = *pi_agid;
    dvdioctl.i_lba = 0;

    i_ret = ioctl( i_fd, IODVD_REPORT_KEY, &dvdioctl );

    *pi_agid = p_buffer[ 7 ] >> 6;

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

#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    INIT_DVDIOCTL( 16 );

    dvdioctl.i_keyformat = kChallengeKey;
    dvdioctl.i_agid = *pi_agid;
    dvdioctl.i_lba = 0;

    i_ret = ioctl( i_fd, IODVD_REPORT_KEY, &dvdioctl );

    memcpy( p_challenge, p_buffer + 4, 12 );

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

#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    INIT_DVDIOCTL( 8 );

    dvdioctl.i_keyformat = kASF;
    dvdioctl.i_agid = *pi_agid;
    dvdioctl.i_lba = 0;

    i_ret = ioctl( i_fd, IODVD_REPORT_KEY, &dvdioctl );

    *pi_asf = p_buffer[ 7 ] & 1;

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

#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    INIT_DVDIOCTL( 12 );

    dvdioctl.i_keyformat = kKey1;
    dvdioctl.i_agid = *pi_agid;

    i_ret = ioctl( i_fd, IODVD_SEND_KEY, &dvdioctl );

    memcpy( p_key, p_buffer + 4, 8 );

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

#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    INIT_DVDIOCTL( 0 );

    dvdioctl.i_keyformat = kInvalidateAGID;
    dvdioctl.i_agid = *pi_agid;

    i_ret = ioctl( i_fd, IODVD_SEND_KEY, &dvdioctl );

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
#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    INIT_DVDIOCTL( 16 );

    dvdioctl.i_keyformat = kChallengeKey;
    dvdioctl.i_agid = *pi_agid;

    p_buffer[ 1 ] = 0xe;
    memcpy( p_buffer + 4, p_challenge, 12 );

    return ioctl( i_fd, IODVD_SEND_KEY, &dvdioctl );

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    return -1;

#endif
}

/*****************************************************************************
 * ioctl_SendKey2: send the second key to the drive
 *****************************************************************************/
int ioctl_SendKey2( int i_fd, int *pi_agid, u8 *p_key )
{
#if defined( DVD_STRUCT_IN_LINUX_CDROM_H )
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

#elif defined( SYS_DARWIN1_3 )
    INIT_DVDIOCTL( 12 );

    dvdioctl.i_keyformat = kKey2;
    dvdioctl.i_agid = *pi_agid;

    p_buffer[ 1 ] = 0xa;
    memcpy( p_buffer + 4, p_key, 8 );

    return ioctl( i_fd, IODVD_SEND_KEY, &dvdioctl );

#else
    /* DVD ioctls unavailable - do as if the ioctl failed */
    return -1;

#endif
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
    memset( p_rdc, 0, sizeof( raw_device_command ) );
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

