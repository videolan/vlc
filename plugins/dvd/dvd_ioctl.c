/*****************************************************************************
 * dvd_ioctl.c: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ioctl.c,v 1.4 2001/04/02 23:30:41 sam Exp $
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

#include <sys/types.h>
#include <netinet/in.h>
#ifdef HAVE_SYS_DVDIO_H
#   include <sys/ioctl.h>
#   include <sys/dvdio.h>
#endif
#ifdef LINUX_DVD
#   include <sys/ioctl.h>
#   include <linux/cdrom.h>
#endif
#ifdef SYS_BEOS
#   include <sys/ioctl.h>
#   include <malloc.h>
#   include <scsi.h>
#endif
#ifdef SYS_DARWIN1_3
#   include <sys/ioctl.h>
#   include <DVDioctl/DVDioctl.h>
#endif

#include "common.h"
#include "intf_msg.h"

#include "dvd_css.h"
#include "dvd_ioctl.h"

/*****************************************************************************
 * Local prototypes - BeOS specific
 *****************************************************************************/
#if defined( SYS_BEOS )
static void InitCommand ( struct cdrom_generic_command *p_cgc,
                          void *buf, int i_len, int i_type );
static int  SendCommand ( int i_fd, struct cdrom_generic_command *p_cgc );
#endif

/*****************************************************************************
 * dvd_ReadKey: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_ReadKey( css_t *p_css, u8 *p_key )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    int i_ret;
    dvd_struct dvd;

    dvd.type = DVD_STRUCT_DISCKEY;
    dvd.disckey.agid = p_css->i_agid;

    memset( dvd.disckey.value, 0, 2048 );

    i_ret = ioctl( p_css->i_fd, DVD_READ_STRUCT, &dvd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, dvd.disckey.value, 2048 );
    return i_ret;

#elif defined( SYS_BEOS )
    int i_ret, size;
    u8 p_buf[ 2048 + 4];
    struct cdrom_generic_command cgc;

    size = 2048 + 4;

    InitCommand( &cgc, p_buf, size, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;

    cgc.cmd[7] = DVD_STRUCT_DISCKEY;
    cgc.cmd[8] = size >> 8;
    cgc.cmd[9] = size & 0xff;
    cgc.cmd[10] = p_css->i_agid << 6;

    i_ret = SendCommand( p_css->i_fd, &cgc );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    memcpy( p_key, p_buf + 4, 2048 );
    return i_ret;

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_ReadCopyright: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_ReadCopyright( int i_fd, int i_layer, int *pi_copyright )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    int i_ret;
    dvd_struct dvd;

    dvd.type = DVD_STRUCT_COPYRIGHT;
    dvd.copyright.layer_num = i_layer;

    i_ret = ioctl( i_fd, DVD_READ_STRUCT, &dvd );

    *pi_copyright = dvd.copyright.cpst;
    return i_ret;

#elif defined( SYS_BEOS )
    int i_ret;
    u8 p_buf[8];
    struct cdrom_generic_command cgc;

    InitCommand( &cgc, p_buf, sizeof(p_buf), CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;

    cgc.cmd[6] = i_layer;
    cgc.cmd[7] = DVD_STRUCT_COPYRIGHT;
    cgc.cmd[8] = cgc.buflen >> 8;
    cgc.cmd[9] = cgc.buflen & 0xff;

    i_ret = SendCommand( i_fd, &cgc );

    *pi_copyright = p_buf[4];
    return i_ret;

#elif defined( SYS_DARWIN1_3 )
    intf_ErrMsg( "css error: DVD ioctls not fully functional yet" );
    intf_ErrMsg( "css error: assuming disc is unencrypted" );

    *pi_copyright = 0;
    return 0;

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_LUSendAgid: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_LUSendAgid( css_t *p_css )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    int i_ret;
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_AGID;
    auth_info.lsa.agid = p_css->i_agid;

    i_ret = ioctl( p_css->i_fd, DVD_AUTH, &auth_info );

    p_css->i_agid = auth_info.lsa.agid;
    return i_ret;

#elif defined( SYS_BEOS )
    u8 p_buf[8];
    int i_ret;
    struct cdrom_generic_command cgc;

    //memset( p_buf, 0, sizeof( p_buf ) );

    InitCommand( &cgc, p_buf, 0, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_REPORT_KEY;
    cgc.cmd[10] = 0x00 | (p_css->i_agid << 6);
    cgc.buflen = 8;
    cgc.cmd[9] = cgc.buflen;
    cgc.data_direction = CGC_DATA_READ;

    i_ret = SendCommand( p_css->i_fd, &cgc );

    p_css->i_agid = p_buf[7] >> 6;
    return i_ret;

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_InvalidateAgid: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_InvalidateAgid( css_t *p_css )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    int i_ret;
    dvd_authinfo auth_info;

    auth_info.type = DVD_INVALIDATE_AGID;
    auth_info.lsa.agid = p_css->i_agid;

    i_ret = ioctl( p_css->i_fd, DVD_AUTH, &auth_info );

    p_css->i_agid = auth_info.lsa.agid;
    return i_ret;

#elif defined( SYS_BEOS )
    u8 p_buf[0];
    struct cdrom_generic_command cgc;

    //memset( p_buf, 0, sizeof( p_buf ) );

    InitCommand( &cgc, p_buf, 0, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_REPORT_KEY;
    cgc.cmd[10] = 0x3f | (p_css->i_agid << 6);
    cgc.cmd[9] = cgc.buflen = 0;
    cgc.data_direction = CGC_DATA_READ;

    return SendCommand( p_css->i_fd, &cgc );

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_HostSendChallenge: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_HostSendChallenge( css_t *p_css, u8 *p_challenge )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    dvd_authinfo auth_info;

    auth_info.type = DVD_HOST_SEND_CHALLENGE;

    memcpy( auth_info.hsc.chal, p_challenge, sizeof(dvd_challenge) );

    return ioctl( p_css->i_fd, DVD_AUTH, &auth_info );

#elif defined( SYS_BEOS )
    u8 p_buf[16];
    struct cdrom_generic_command cgc;

    //memset( p_buf, 0, sizeof( p_buf ) );

    InitCommand( &cgc, p_buf, 0, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_SEND_KEY;
    cgc.cmd[10] = 0x01 | (p_css->i_agid << 6);
    cgc.buflen = 16;
    cgc.cmd[9] = cgc.buflen;
    cgc.data_direction = CGC_DATA_WRITE;

    p_buf[1] = 0xe;
    memcpy( p_buf + 4, p_challenge, sizeof(dvd_challenge) );

    return SendCommand( p_css->i_fd, &cgc );

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_LUSendASF: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_LUSendASF( css_t *p_css, int *pi_asf )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    int i_ret;
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_ASF;
    auth_info.lsasf.agid = p_css->i_agid;
    auth_info.lsasf.asf = *pi_asf;

    i_ret = ioctl( p_css->i_fd, DVD_AUTH, &auth_info );

    *pi_asf = auth_info.lsasf.asf;
    return i_ret;

#elif defined( SYS_BEOS )
    int i_ret;
    u8 p_buf[8];
    struct cdrom_generic_command cgc;

    //memset( p_buf, 0, sizeof( p_buf ) );

    InitCommand( &cgc, p_buf, 0, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_REPORT_KEY;
    cgc.cmd[10] = 0x05 | (p_css->i_agid << 6);
    cgc.buflen = 8;
    cgc.cmd[9] = cgc.buflen;
    cgc.data_direction = CGC_DATA_READ;

    i_ret = SendCommand( p_css->i_fd, &cgc );

    *pi_asf = p_buf[7] & 1;
    return i_ret;

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_LUSendChallenge: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_LUSendChallenge( css_t *p_css, u8 *p_challenge )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    int i_ret;
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_CHALLENGE;

    i_ret = ioctl( p_css->i_fd, DVD_AUTH, &auth_info );

    memcpy( p_challenge, auth_info.lsc.chal, sizeof(dvd_challenge) );
    return i_ret;

#elif defined( SYS_BEOS )
    int i_ret;
    u8 p_buf[16];
    struct cdrom_generic_command cgc;

    //memset( p_buf, 0, sizeof( p_buf ) );

    InitCommand( &cgc, p_buf, 0, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_REPORT_KEY;
    cgc.cmd[10] = 0x01 | (p_css->i_agid << 6);
    cgc.buflen = 16;
    cgc.cmd[9] = cgc.buflen;
    cgc.data_direction = CGC_DATA_READ;

    i_ret = SendCommand( p_css->i_fd, &cgc );

    memcpy( p_challenge, p_buf + 4, sizeof(dvd_challenge) );
    return i_ret;

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_LUSendKey1: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_LUSendKey1( css_t *p_css, u8 *p_key )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    int i_ret;
    dvd_authinfo auth_info;

    auth_info.type = DVD_LU_SEND_KEY1;
    auth_info.lsk.agid = p_css->i_agid;

    i_ret = ioctl( p_css->i_fd, DVD_AUTH, &auth_info );

    memcpy( p_key, auth_info.lsk.key, sizeof(dvd_key) );
    return i_ret;

#elif defined( SYS_BEOS )
    int i_ret;
    u8 p_buf[12];
    struct cdrom_generic_command cgc;

    //memset( p_buf, 0, sizeof( p_buf ) );

    InitCommand( &cgc, p_buf, 0, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_REPORT_KEY;
    cgc.cmd[10] = 0x02 | (p_css->i_agid << 6);
    cgc.buflen = 12;
    cgc.cmd[9] = cgc.buflen;
    cgc.data_direction = CGC_DATA_READ;

    i_ret = SendCommand( p_css->i_fd, &cgc );

    memcpy( p_key, p_buf + 4, sizeof(dvd_key) );
    return i_ret;

#else
    return -1;

#endif
}

/*****************************************************************************
 * dvd_HostSendKey2: 
 *****************************************************************************
 * 
 *****************************************************************************/
int dvd_HostSendKey2( css_t *p_css, u8 *p_key )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    dvd_authinfo auth_info;

    auth_info.type = DVD_HOST_SEND_KEY2;
    auth_info.hsk.agid = p_css->i_agid;

    memcpy( auth_info.hsk.key, p_key, sizeof(dvd_key) );

    return ioctl( p_css->i_fd, DVD_AUTH, &auth_info );

#elif defined( SYS_BEOS )
    u8 p_buf[12];
    struct cdrom_generic_command cgc;

    //memset( p_buf, 0, sizeof( p_buf ) );

    InitCommand( &cgc, p_buf, 0, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_SEND_KEY;
    cgc.cmd[10] = 0x3 | (p_css->i_agid << 6);
    cgc.buflen = 12;
    cgc.cmd[9] = cgc.buflen;
    cgc.data_direction = CGC_DATA_WRITE;

    p_buf[1] = 0xa;
    memcpy( p_buf + 4, p_key, sizeof(dvd_key) );

    return SendCommand( p_css->i_fd, &cgc );

#else
    return -1;

#endif
}

/* Local prototypes */

#if defined( SYS_BEOS )
/*****************************************************************************
 * InitCommand: initialize a CGC structure
 *****************************************************************************
 * This function initializes a CDRom Generic Command structure for
 * future use, either a read command or a write command.
 *****************************************************************************/
static void InitCommand( struct cdrom_generic_command *p_cgc,
                         void *p_buf, int i_len, int i_type )
{
    memset( p_cgc, 0, sizeof( struct cdrom_generic_command ) );

    if( p_buf != NULL )
    {
        memset( p_buf, 0, i_len );
    }

    p_cgc->buffer = ( char * )p_buf;
    p_cgc->buflen = i_len;
    p_cgc->data_direction = i_type;
    p_cgc->timeout = 255;
}

/*****************************************************************************
 * SendCommand: send a raw device command to the DVD drive.
 *****************************************************************************
 * This is the most important part of the ioctl emulation, the place where
 * data is really sent to the DVD.
 *****************************************************************************/
static int SendCommand( int i_fd, struct cdrom_generic_command *p_cgc )
{
    int i;

    raw_device_command rdc;
    memset( &rdc, 0, sizeof( rdc ) );

    /* fill out our raw device command data */
    rdc.data = p_cgc->buffer;
    rdc.data_length = p_cgc->buflen;
    rdc.sense_data = p_cgc->sense;
    rdc.sense_data_length = 0;
    rdc.timeout = 1000000;

    if( p_cgc->data_direction == CGC_DATA_READ )
    {
        intf_WarnMsg( 2, "css: data_direction == CGC_DATA_READ" );
        rdc.flags = B_RAW_DEVICE_DATA_IN;
    }

    rdc.command_length = 12;

    /* FIXME: check if this _really_ should go up to [12] */
    for( i = 0 ; i < 13 ; i++ )
    {
        rdc.command[i] = p_cgc->cmd[i];
    }

    return ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );
}
#endif

