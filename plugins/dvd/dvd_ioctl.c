/*****************************************************************************
 * dvd_ioctl.c: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ioctl.c,v 1.3 2001/03/05 11:53:44 sam Exp $
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
#ifdef HAVE_SYS_IOCTL_H
#   include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_DVDIO_H
#   include <sys/dvdio.h>
#endif
#ifdef LINUX_DVD
#   include <linux/cdrom.h>
#endif
#ifdef SYS_BEOS
#   include <malloc.h>
#   include <scsi.h>
#endif

#include "common.h"
#include "intf_msg.h"

#include "dvd_ioctl.h"

/*****************************************************************************
 * Local prototypes - BeOS specific
 *****************************************************************************/
#if defined( SYS_BEOS )
static int  ReadData          ( int i_fd, dvd_struct *p_dvd );
static int  ReadCopyright     ( int i_fd, dvd_struct *p_dvd );
static int  ReadKey           ( int i_fd, dvd_struct *p_dvd );
static int  ReadBCA           ( int i_fd, dvd_struct *p_dvd );
static int  ReadManufacturer  ( int i_fd, dvd_struct *p_dvd );

static void InitGenericCommand( struct cdrom_generic_command *p_cgc,
                                void *buf, int i_len, int i_type );
static void InitReadCommand   ( struct cdrom_generic_command *p_cgc,
                                unsigned i_agid, unsigned i_type );
static void InitWriteCommand  ( struct cdrom_generic_command *p_cgc,
                                unsigned i_agid, unsigned i_type );

static int  SendCommand ( int i_fd, struct cdrom_generic_command *p_cgc );
#endif

/*****************************************************************************
 * dvd_ioctl: DVD ioctl() wrapper
 *****************************************************************************
 * Since the DVD ioctls do not exist on every machine, we provide this wrapper
 * so that it becomes easier to port them to any architecture.
 *****************************************************************************/
int dvd_ioctl( int i_fd, unsigned long i_op, void *p_arg )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    return( ioctl( i_fd, i_op, p_arg ) );

#elif defined( SYS_BEOS )

    int           i_ret;
    unsigned char buf[20];

    struct cdrom_generic_command p_cgc;

    dvd_struct *p_dvd = (dvd_struct *)p_arg;
    dvd_authinfo *p_authinfo = (dvd_authinfo *)p_arg;

    switch ( i_op )
    {
        case DVD_AUTH: /* Request type is "authentication" */
        {
            memset( buf, 0, sizeof( buf ) );
            InitGenericCommand( &p_cgc, buf, 0, CGC_DATA_READ );

            switch( p_authinfo->type )
            {
                case DVD_LU_SEND_AGID: /* LU data send */

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_AGID" );

                    InitReadCommand( &p_cgc, p_authinfo->lsa.agid, 0 );

                    i_ret = SendCommand( i_fd, &p_cgc );

                    p_authinfo->lsa.agid = buf[7] >> 6;

                    return i_ret;

                case DVD_LU_SEND_KEY1:

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_KEY1" );

                    InitReadCommand( &p_cgc, p_authinfo->lsk.agid, 2 );

                    i_ret = SendCommand( i_fd, &p_cgc );

                    /* Copy the key */
                    memcpy( p_authinfo->lsk.key, &buf[4], sizeof(dvd_key) );

                    return i_ret;

                case DVD_LU_SEND_CHALLENGE:

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_CHALLENGE" );

                    InitReadCommand( &p_cgc, p_authinfo->lsc.agid, 1 );

                    i_ret = SendCommand( i_fd, &p_cgc );

                    /* Copy the challenge */
                    memcpy( p_authinfo->lsc.chal, &buf[4],
                            sizeof(dvd_challenge) );

                    return i_ret;

                case DVD_LU_SEND_TITLE_KEY: /* Post-auth key */

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_TITLE_KEY" );

                    InitReadCommand( &p_cgc, p_authinfo->lstk.agid, 4 );

                    p_cgc.cmd[5] = p_authinfo->lstk.lba;
                    p_cgc.cmd[4] = p_authinfo->lstk.lba >> 8;
                    p_cgc.cmd[3] = p_authinfo->lstk.lba >> 16;
                    p_cgc.cmd[2] = p_authinfo->lstk.lba >> 24;

                    i_ret = SendCommand( i_fd, &p_cgc );

                    p_authinfo->lstk.cpm = (buf[4] >> 7) & 1;
                    p_authinfo->lstk.cp_sec = (buf[4] >> 6) & 1;
                    p_authinfo->lstk.cgms = (buf[4] >> 4) & 3;

                    /* Copy the key */
                    memcpy( p_authinfo->lstk.title_key, &buf[5],
                            sizeof(dvd_key) );

                    return i_ret;

                case DVD_LU_SEND_ASF:

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_ASF" );

                    InitReadCommand( &p_cgc, p_authinfo->lsasf.agid, 5 );

                    i_ret = SendCommand( i_fd, &p_cgc );

                    p_authinfo->lsasf.asf = buf[7] & 1;

                    return i_ret;

                case DVD_HOST_SEND_CHALLENGE: /* LU data receive */

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_CHALLENGE" );

                    InitWriteCommand( &p_cgc, p_authinfo->hsc.agid, 1 );
                    buf[1] = 0xe;

                    /* Copy the challenge */
                    memcpy( &buf[4], p_authinfo->hsc.chal,
                            sizeof(dvd_challenge) );

                    if( (i_ret = SendCommand(i_fd, &p_cgc)) )
                    {
                        return i_ret;
                    }

                    p_authinfo->type = DVD_LU_SEND_KEY1;

                    return 0;

                case DVD_HOST_SEND_KEY2:

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_KEY2" );

                    InitWriteCommand( &p_cgc, p_authinfo->hsk.agid, 3 );
                    buf[1] = 0xa;

                    /* Copy the key */
                    memcpy( &buf[4], p_authinfo->hsk.key, sizeof(dvd_key) );

                    if( (i_ret = SendCommand(i_fd, &p_cgc)) )
                    {
                        p_authinfo->type = DVD_AUTH_FAILURE;
                        return i_ret;
                    }

                    p_authinfo->type = DVD_AUTH_ESTABLISHED;

                    return 0;

                case DVD_INVALIDATE_AGID: /* Misc */

                    intf_WarnMsg( 2, "css DoAuth: DVD_INVALIDATE_AGID" );

                    InitReadCommand( &p_cgc, p_authinfo->lsa.agid, 0x3f );

        	    return SendCommand( i_fd, &p_cgc );

                case DVD_LU_SEND_RPC_STATE: /* Get region settings */

                    intf_WarnMsg( 2, "css DoAuth: DVD_LU_SEND_RPC_STATE "
                                     "(unimplemented)" );

        #if 0
                    p_dvdetup_report_key( &p_cgc, 0, 8 );
                    memset( &rpc_state, 0, sizeof(rpc_state_t) );
                    p_cgc.buffer = (char *) &rpc_state;

                    if( (i_ret = SendCommand(i_fd, &p_cgc)) )
                    {
                        return i_ret;
                    }

                    p_authinfo->lrpcs.type = rpc_state.type_code;
                    p_authinfo->lrpcs.vra = rpc_state.vra;
                    p_authinfo->lrpcs.ucca = rpc_state.ucca;
                    p_authinfo->lrpcs.region_mask = rpc_state.region_mask;
                    p_authinfo->lrpcs.rpc_scheme = rpc_state.rpc_scheme;
        #endif

                    return 0;

                case DVD_HOST_SEND_RPC_STATE: /* Set region settings */

                    intf_WarnMsg( 2, "css DoAuth: DVD_HOST_SEND_RPC_STATE" );

                    InitWriteCommand( &p_cgc, 0, 6 );
                    buf[1] = 6;
                    buf[4] = p_authinfo->hrpcs.pdrc;

                    return SendCommand( i_fd, &p_cgc );

                default:
                    intf_ErrMsg( "css DoAuth: invalid DVD key ioctl" );
                    return -1;

            }
        }

        case DVD_READ_STRUCT: /* Request type is "read structure" */
        {
            switch( p_dvd->type )
            {
                case DVD_STRUCT_PHYSICAL:

                    intf_WarnMsg( 2, "css ReadStruct: DVD_STRUCT_PHYSICAL" );

                    return ReadData( i_fd, p_dvd );

                case DVD_STRUCT_COPYRIGHT:

                    intf_WarnMsg( 2, "css ReadStruct: DVD_STRUCT_COPYRIGHT" );

                    return ReadCopyright( i_fd, p_dvd );

                case DVD_STRUCT_DISCKEY:

                    intf_WarnMsg( 2, "css ReadStruct: DVD_STRUCT_DISCKEY" );

                    return ReadKey( i_fd, p_dvd );

                case DVD_STRUCT_BCA:

                    intf_WarnMsg( 2, "css ReadStruct: DVD_STRUCT_BCA" );

                    return ReadBCA( i_fd, p_dvd );

                case DVD_STRUCT_MANUFACT:

                    intf_WarnMsg( 2, "css ReadStruct: DVD_STRUCT_MANUFACT" );

                    return ReadManufacturer( i_fd, p_dvd );

                default:
                    intf_WarnMsg( 2, "css ReadStruct: invalid request (%d)",
                                  p_dvd->type );

                    return -1;
            }
        }

        default: /* Unknown request type */
        {
            intf_ErrMsg( "css error: unknown command 0x%x", i_op );
            return -1;
        }
    }
#else

    return -1;
#endif
}

/* Local prototypes */

#if defined( SYS_BEOS )
/*****************************************************************************
 * ReadData: Get data structure information from the DVD.
 *****************************************************************************/
static int ReadData( int i_fd, dvd_struct *p_dvd )
{
    int i_ret, i;
    u_char buf[4 + 4 * 20], *base;
    struct dvd_layer *layer;
    struct cdrom_generic_command cgc;

    InitGenericCommand( &cgc, buf, sizeof(buf), CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[6] = p_dvd->physical.layer_num;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[9] = cgc.buflen & 0xff;

    if( (i_ret = SendCommand(i_fd, &cgc)) )
    {
        return i_ret;
    }

    base = &buf[4];
    layer = &p_dvd->physical.layer[0];

    /* place the data... really ugly, but at least we won't have to
     * worry about endianess in userspace or here. */
    for( i = 0; i < 4; ++i, base += 20, ++layer )
    {
        memset( layer, 0, sizeof(*layer) );

        layer->book_version = base[0] & 0xf;
        layer->book_type = base[0] >> 4;
        layer->min_rate = base[1] & 0xf;
        layer->disc_size = base[1] >> 4;
        layer->layer_type = base[2] & 0xf;
        layer->track_path = (base[2] >> 4) & 1;
        layer->nlayers = (base[2] >> 5) & 3;
        layer->track_density = base[3] & 0xf;
        layer->linear_density = base[3] >> 4;
        layer->start_sector = base[5] << 16 | base[6] << 8 | base[7];
        layer->end_sector = base[9] << 16 | base[10] << 8 | base[11];
        layer->end_sector_l0 = base[13] << 16 | base[14] << 8 | base[15];
        layer->bca = base[16] >> 7;
    }

    return 0;
}

/*****************************************************************************
 * ReadCopyright: get copyright information from the DVD.
 *****************************************************************************/
static int ReadCopyright( int i_fd, dvd_struct *p_dvd )
{
    int i_ret;
    u_char buf[8];
    struct cdrom_generic_command cgc;

    InitGenericCommand( &cgc, buf, sizeof(buf), CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[6] = p_dvd->copyright.layer_num;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[8] = cgc.buflen >> 8;
    cgc.cmd[9] = cgc.buflen & 0xff;

    if( (i_ret = SendCommand(i_fd, &cgc)) )
    {
        return i_ret;
    }

    p_dvd->copyright.cpst = buf[4];
    p_dvd->copyright.rmi = buf[5];

    return 0;
}

/*****************************************************************************
 * ReadKey: get a key from the DVD.
 *****************************************************************************/
static int ReadKey( int i_fd, dvd_struct *p_dvd )
{
    int i_ret, size;
    u_char *buf;
    struct cdrom_generic_command cgc;

    size = sizeof( p_dvd->disckey.value ) + 4;

#if 0
    if ((buf = (u_char *) kmalloc(size, GFP_KERNEL)) == NULL)
    {
        return -ENOMEM;
    }
#endif
    buf = (u_char *) malloc( size );

    InitGenericCommand( &cgc, buf, size, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[8] = size >> 8;
    cgc.cmd[9] = size & 0xff;
    cgc.cmd[10] = p_dvd->disckey.agid << 6;

    if( !(i_ret = SendCommand(i_fd, &cgc)) )
    {
        memcpy( p_dvd->disckey.value, &buf[4], sizeof(p_dvd->disckey.value) );
    }

    free( buf );
    return i_ret;
}

/*****************************************************************************
 * ReadBCA: read the Burst Cutting Area of a DVD.
 *****************************************************************************
 * The BCA is a special part of the DVD which is used to burn additional
 * data after it has been manufactured. DIVX is an exemple.
 *****************************************************************************/
static int ReadBCA( int i_fd, dvd_struct *p_dvd )
{
    int i_ret;
    u_char buf[4 + 188];
    struct cdrom_generic_command cgc;

    InitGenericCommand( &cgc, buf, sizeof(buf), CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[9] = cgc.buflen = 0xff;

    if( (i_ret = SendCommand(i_fd, &cgc)) )
    {
        return i_ret;
    }

    p_dvd->bca.len = buf[0] << 8 | buf[1];
    if( p_dvd->bca.len < 12 || p_dvd->bca.len > 188 )
    {
        intf_ErrMsg( "css error: invalid BCA length (%d)", p_dvd->bca.len );
        return -1;
    }

    memcpy( p_dvd->bca.value, &buf[4], p_dvd->bca.len );

    return 0;
}

/*****************************************************************************
 * ReadManufacturer: get manufacturer information from the DVD.
 *****************************************************************************/
static int ReadManufacturer( int i_fd, dvd_struct *p_dvd )
{
    int i_ret = 0, size;
    u_char *buf;
    struct cdrom_generic_command cgc;

    size = sizeof( p_dvd->manufact.value ) + 4;

#if 0
    if( (buf = (u_char *) kmalloc(size, GFP_KERNEL)) == NULL )
    {
        return -ENOMEM;
    }
#endif
    buf = (u_char *) malloc(size);

    InitGenericCommand( &cgc, buf, size, CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[8] = size >> 8;
    cgc.cmd[9] = size & 0xff;

    if( (i_ret = SendCommand(i_fd, &cgc)) )
    {
        return i_ret;
    }

    p_dvd->manufact.len = buf[0] << 8 | buf[1];
    if( p_dvd->manufact.len < 0 || p_dvd->manufact.len > 2048 )
    {
        intf_ErrMsg( "css error: invalid manufacturer info length (%d)",
                     p_dvd->bca.len );
        i_ret = -1;
    }
    else
    {
        memcpy( p_dvd->manufact.value, &buf[4], p_dvd->manufact.len );
    }

    free( buf );
    return i_ret;
}

/*****************************************************************************
 * InitGenericCommand: initialize a CGC structure
 *****************************************************************************
 * This function initializes a CDRom Generic Command structure for
 * future use, either a read command or a write command.
 *****************************************************************************/
static void InitGenericCommand( struct cdrom_generic_command *p_cgc,
                                void *buf, int i_len, int i_type )
{
    memset( p_cgc, 0, sizeof( struct cdrom_generic_command ) );

    if( buf != NULL )
    {
        memset( buf, 0, i_len );
    }

    p_cgc->buffer = ( char * )buf;
    p_cgc->buflen = i_len;
    p_cgc->data_direction = i_type;
    p_cgc->timeout = 255;
}

/*****************************************************************************
 * InitReadCommand: fill a CGC structure for reading purposes.
 *****************************************************************************
 * This function fills a CDRom Generic Command for a command which will
 * read data from the DVD.
 *****************************************************************************/
static void InitReadCommand( struct cdrom_generic_command *p_cgc,
                             unsigned i_agid, unsigned i_type )
{
    p_cgc->cmd[0] = GPCMD_REPORT_KEY;
    p_cgc->cmd[10] = i_type | (i_agid << 6);

    /* FIXME: check what i_type means */
    switch( i_type )
    {
        case 0:
        case 8:
        case 5:
            p_cgc->buflen = 8;
            break;

        case 1:
            p_cgc->buflen = 16;
            break;

        case 2:
        case 4:
            p_cgc->buflen = 12;
            break;
    }

    p_cgc->cmd[9] = p_cgc->buflen;
    p_cgc->data_direction = CGC_DATA_READ;
}

/*****************************************************************************
 * InitWriteCommand: fill a CGC structure for writing purposes.
 *****************************************************************************
 * This function fills a CDRom Generic Command for a command which will
 * send data to the DVD.
 *****************************************************************************/
static void InitWriteCommand( struct cdrom_generic_command *p_cgc,
                              unsigned i_agid, unsigned i_type )
{
    p_cgc->cmd[0] = GPCMD_SEND_KEY;
    p_cgc->cmd[10] = i_type | (i_agid << 6);

    /* FIXME: check what i_type means */
    switch( i_type )
    {
        case 1:
            p_cgc->buflen = 16;
            break;

        case 3:
            p_cgc->buflen = 12;
            break;

        case 6:
            p_cgc->buflen = 8;
            break;
    }

    p_cgc->cmd[9] = p_cgc->buflen;
    p_cgc->data_direction = CGC_DATA_WRITE;
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

