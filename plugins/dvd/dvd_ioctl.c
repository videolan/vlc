/*****************************************************************************
 * dvd_ioctl.c: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ioctl.c,v 1.1 2001/02/20 07:49:12 sam Exp $
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
 * Local prototypes
 *****************************************************************************/
#if defined( SYS_BEOS )
static int  dvd_do_auth          ( int i_fd, dvd_authinfo *p_authinfo );
static int  dvd_read_struct      ( int i_fd, dvd_struct *p_dvd );
static int  dvd_read_physical    ( int i_fd, dvd_struct *p_dvd );
static int  dvd_read_copyright   ( int i_fd, dvd_struct *p_dvd );
static int  dvd_read_disckey     ( int i_fd, dvd_struct *p_dvd );
static int  dvd_read_bca         ( int i_fd, dvd_struct *p_dvd );
static int  dvd_read_manufact    ( int i_fd, dvd_struct *p_dvd );
static int  communicate_with_dvd ( int i_fd,
                                   struct cdrom_generic_command *p_cgc );
static void init_cdrom_command   ( struct cdrom_generic_command *p_cgc,
                                   void *buf, int i_len, int i_type );
static void setup_report_key     ( struct cdrom_generic_command *p_cgc,
                                   unsigned i_agid, unsigned i_type );
static void setup_send_key       ( struct cdrom_generic_command *p_cgc,
                                   unsigned i_agid, unsigned i_type );
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
    switch ( i_op )
    {
    case DVD_AUTH:
        return dvd_do_auth( i_fd, (dvd_authinfo *)p_arg );

    case DVD_READ_STRUCT:
        return dvd_read_struct( i_fd, (dvd_struct *)p_arg );

    default:
        intf_ErrMsg( "css error: unknown command 0x%x", i_op );
        return -1;
    }
#else

    return -1;
#endif
}

#if defined( SYS_BEOS )

/*****************************************************************************
 * setup_report_key
 *****************************************************************************/
static void setup_report_key( struct cdrom_generic_command *p_cgc,
                              unsigned i_agid, unsigned i_type )
{
    p_cgc->cmd[0] = GPCMD_REPORT_KEY;
    p_cgc->cmd[10] = i_type | (i_agid << 6);

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
 * setup_send_key
 *****************************************************************************/
static void setup_send_key( struct cdrom_generic_command *p_cgc,
                            unsigned i_agid, unsigned i_type )
{
    p_cgc->cmd[0] = GPCMD_SEND_KEY;
    p_cgc->cmd[10] = i_type | (i_agid << 6);

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
 * init_cdrom_command
 *****************************************************************************/
static void init_cdrom_command( struct cdrom_generic_command *p_cgc,
                                void *buf, int i_len, int i_type )
{
    memset( p_cgc, 0, sizeof(struct cdrom_generic_command) );

    if (buf)
    {
        memset( buf, 0, i_len );
    }

    p_cgc->buffer = (char *) buf;
    p_cgc->buflen = i_len;
    p_cgc->data_direction = i_type;
    p_cgc->timeout = 255;
}

/* DVD handling */

/*****************************************************************************
 * dvd_do_auth
 *****************************************************************************/
static int dvd_do_auth( int i_fd, dvd_authinfo *p_authinfo )
{
    int i_ret;
    unsigned char buf[20];
    struct cdrom_generic_command p_cgc;

#define copy_key(dest,src)  memcpy((dest), (src), sizeof(dvd_key))
#define copy_chal(dest,src) memcpy((dest), (src), sizeof(dvd_challenge))

#if 0
    struct rpc_state_t rpc_state;
#endif

    memset( buf, 0, sizeof(buf) );
    init_cdrom_command( &p_cgc, buf, 0, CGC_DATA_READ );

    switch (p_authinfo->type)
    {
        /* LU data send */
        case DVD_LU_SEND_AGID:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_AGID" );

            setup_report_key(&p_cgc, p_authinfo->lsa.agid, 0);

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            p_authinfo->lsa.agid = buf[7] >> 6;
            /* Returning data, let host change state */

            break;

        case DVD_LU_SEND_KEY1:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_KEY1" );

            setup_report_key(&p_cgc, p_authinfo->lsk.agid, 2);

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            copy_key(p_authinfo->lsk.key, &buf[4]);
            /* Returning data, let host change state */

            break;

        case DVD_LU_SEND_CHALLENGE:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_CHALLENGE" );

            setup_report_key(&p_cgc, p_authinfo->lsc.agid, 1);

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            copy_chal(p_authinfo->lsc.chal, &buf[4]);
            /* Returning data, let host change state */

            break;

        /* Post-auth key */
        case DVD_LU_SEND_TITLE_KEY:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_TITLE_KEY" );

            setup_report_key(&p_cgc, p_authinfo->lstk.agid, 4);
            p_cgc.cmd[5] = p_authinfo->lstk.lba;
            p_cgc.cmd[4] = p_authinfo->lstk.lba >> 8;
            p_cgc.cmd[3] = p_authinfo->lstk.lba >> 16;
            p_cgc.cmd[2] = p_authinfo->lstk.lba >> 24;

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            p_authinfo->lstk.cpm = (buf[4] >> 7) & 1;
            p_authinfo->lstk.cp_sec = (buf[4] >> 6) & 1;
            p_authinfo->lstk.cgms = (buf[4] >> 4) & 3;
            copy_key(p_authinfo->lstk.title_key, &buf[5]);
            /* Returning data, let host change state */

            break;

        case DVD_LU_SEND_ASF:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_ASF" );

            setup_report_key(&p_cgc, p_authinfo->lsasf.agid, 5);

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            p_authinfo->lsasf.asf = buf[7] & 1;

            break;

        /* LU data receive (LU changes state) */
        case DVD_HOST_SEND_CHALLENGE:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_CHALLENGE" );

            setup_send_key(&p_cgc, p_authinfo->hsc.agid, 1);
            buf[1] = 0xe;
            copy_chal(&buf[4], p_authinfo->hsc.chal);

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            p_authinfo->type = DVD_LU_SEND_KEY1;

            break;

        case DVD_HOST_SEND_KEY2:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_KEY2" );

            setup_send_key(&p_cgc, p_authinfo->hsk.agid, 3);
            buf[1] = 0xa;
            copy_key(&buf[4], p_authinfo->hsk.key);

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                p_authinfo->type = DVD_AUTH_FAILURE;
                return i_ret;
            }
            p_authinfo->type = DVD_AUTH_ESTABLISHED;

            break;

        /* Misc */
        case DVD_INVALIDATE_AGID:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_INVALIDATE_AGID" );

            setup_report_key(&p_cgc, p_authinfo->lsa.agid, 0x3f);

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            break;

        /* Get region settings */
        case DVD_LU_SEND_RPC_STATE:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_LU_SEND_RPC_STATE "
                             "(unimplemented)" );

#if 0
            p_dvdetup_report_key(&p_cgc, 0, 8);
            memset(&rpc_state, 0, sizeof(rpc_state_t));
            p_cgc.buffer = (char *) &rpc_state;

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            p_authinfo->lrpcs.type = rpc_state.type_code;
            p_authinfo->lrpcs.vra = rpc_state.vra;
            p_authinfo->lrpcs.ucca = rpc_state.ucca;
            p_authinfo->lrpcs.region_mask = rpc_state.region_mask;
            p_authinfo->lrpcs.rpc_scheme = rpc_state.rpc_scheme;
#endif

            break;

        /* Set region settings */
        case DVD_HOST_SEND_RPC_STATE:

            intf_WarnMsg( 2, "css dvd_do_auth: DVD_HOST_SEND_RPC_STATE" );

            setup_send_key(&p_cgc, 0, 6);
            buf[1] = 6;
            buf[4] = p_authinfo->hrpcs.pdrc;

            /* handle uniform packets for scsi type devices (scsi,atapi) */
            if ((i_ret = communicate_with_dvd(i_fd, &p_cgc)))
            {
                return i_ret;
            }

            break;

        default:
            intf_ErrMsg( "css dvd_do_auth: invalid DVD key ioctl" );
            return -1;

    }

    return 0;
}

/*****************************************************************************
 * dvd_read_struct
 *****************************************************************************/
static int dvd_read_struct( int i_fd, dvd_struct *p_dvd )
{
    switch (p_dvd->type)
    {
        case DVD_STRUCT_PHYSICAL:

            intf_WarnMsg( 2, "css dvd_read_struct: DVD_STRUCT_PHYSICAL" );
            return dvd_read_physical(i_fd, p_dvd);

        case DVD_STRUCT_COPYRIGHT:

            intf_WarnMsg( 2, "css dvd_read_struct: DVD_STRUCT_COPYRIGHT" );
            return dvd_read_copyright(i_fd, p_dvd);

        case DVD_STRUCT_DISCKEY:

            intf_WarnMsg( 2, "css dvd_read_struct: DVD_STRUCT_DISCKEY" );
            return dvd_read_disckey(i_fd, p_dvd);

        case DVD_STRUCT_BCA:

            intf_WarnMsg( 2, "css dvd_read_struct: DVD_STRUCT_BCA" );
            return dvd_read_bca(i_fd, p_dvd);

        case DVD_STRUCT_MANUFACT:

            intf_WarnMsg( 2, "css dvd_read_struct: DVD_STRUCT_MANUFACT" );
            return dvd_read_manufact(i_fd, p_dvd);

        default:
            intf_WarnMsg( 2, "css dvd_read_struct: invalid request (%d)",
                          p_dvd->type );
            return -1;
    }
}

/*****************************************************************************
 * dvd_read_physical
 *****************************************************************************/
static int dvd_read_physical( int i_fd, dvd_struct *p_dvd )
{
    int i_ret, i;
    u_char buf[4 + 4 * 20], *base;
    struct dvd_layer *layer;
    struct cdrom_generic_command cgc;

    init_cdrom_command( &cgc, buf, sizeof(buf), CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[6] = p_dvd->physical.layer_num;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[9] = cgc.buflen & 0xff;

    /* handle uniform packets for scsi type devices (scsi,atapi) */
    if ((i_ret = communicate_with_dvd(i_fd, &cgc)))
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
 * dvd_read_copyright
 *****************************************************************************/
static int dvd_read_copyright( int i_fd, dvd_struct *p_dvd )
{
    int i_ret;
    u_char buf[8];
    struct cdrom_generic_command cgc;

    init_cdrom_command( &cgc, buf, sizeof(buf), CGC_DATA_READ );

    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[6] = p_dvd->copyright.layer_num;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[8] = cgc.buflen >> 8;
    cgc.cmd[9] = cgc.buflen & 0xff;

    /* handle uniform packets for scsi type devices (scsi,atapi) */
    if( (i_ret = communicate_with_dvd(i_fd, &cgc)) )
    {
        return i_ret;
    }

    p_dvd->copyright.cpst = buf[4];
    p_dvd->copyright.rmi = buf[5];

    return 0;
}

/*****************************************************************************
 * dvd_read_disckey
 *****************************************************************************/
static int dvd_read_disckey( int i_fd, dvd_struct *p_dvd )
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
    buf = (u_char *) malloc(size);

    init_cdrom_command(&cgc, buf, size, CGC_DATA_READ);
    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[8] = size >> 8;
    cgc.cmd[9] = size & 0xff;
    cgc.cmd[10] = p_dvd->disckey.agid << 6;

    /* handle uniform packets for scsi type devices (scsi,atapi) */
    if( !(i_ret = communicate_with_dvd(i_fd, &cgc)) )
    {
        memcpy( p_dvd->disckey.value, &buf[4], sizeof(p_dvd->disckey.value) );
    }

    free( buf );
    return i_ret;
}

/*****************************************************************************
 * dvd_read_bca
 *****************************************************************************/
static int dvd_read_bca( int i_fd, dvd_struct *p_dvd )
{
    int i_ret;
    u_char buf[4 + 188];
    struct cdrom_generic_command cgc;

    init_cdrom_command( &cgc, buf, sizeof(buf), CGC_DATA_READ );
    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[9] = cgc.buflen = 0xff;

    /* handle uniform packets for scsi type devices (scsi,atapi) */
    if( (i_ret = communicate_with_dvd(i_fd, &cgc)) )
    {
        return i_ret;
    }

    p_dvd->bca.len = buf[0] << 8 | buf[1];
    if( p_dvd->bca.len < 12 || p_dvd->bca.len > 188 )
    {
        intf_ErrMsg("css error: invalid BCA length (%d)", p_dvd->bca.len );
        return -1;
    }

    memcpy( p_dvd->bca.value, &buf[4], p_dvd->bca.len );

    return 0;
}

/*****************************************************************************
 * dvd_read_manufact
 *****************************************************************************/
static int dvd_read_manufact( int i_fd, dvd_struct *p_dvd )
{
    int i_ret = 0, size;
    u_char *buf;
    struct cdrom_generic_command cgc;

    size = sizeof(p_dvd->manufact.value) + 4;

#if 0
    if( (buf = (u_char *) kmalloc(size, GFP_KERNEL)) == NULL )
    {
        return -ENOMEM;
    }
#endif
    buf = (u_char *) malloc(size);

    init_cdrom_command( &cgc, buf, size, CGC_DATA_READ );
    cgc.cmd[0] = GPCMD_READ_DVD_STRUCTURE;
    cgc.cmd[7] = p_dvd->type;
    cgc.cmd[8] = size >> 8;
    cgc.cmd[9] = size & 0xff;

    /* handle uniform packets for scsi type devices (scsi,atapi) */
    if ((i_ret = communicate_with_dvd(i_fd, &cgc)))
    {
        return i_ret;
    }

    p_dvd->manufact.len = buf[0] << 8 | buf[1];
    if( p_dvd->manufact.len < 0 || p_dvd->manufact.len > 2048 )
    {
        intf_ErrMsg( "css error: invalid manufacturer info length "
                     "(%d)\n", p_dvd->bca.len );
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
 * communicate_with_dvd
 *****************************************************************************/
static int communicate_with_dvd( int i_fd,
                                 struct cdrom_generic_command *p_cgc )
{
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
    rdc.command[0] = p_cgc->cmd[0];
    rdc.command[1] = p_cgc->cmd[1];
    rdc.command[2] = p_cgc->cmd[2];
    rdc.command[3] = p_cgc->cmd[3];
    rdc.command[4] = p_cgc->cmd[4];
    rdc.command[5] = p_cgc->cmd[5];
    rdc.command[6] = p_cgc->cmd[6];
    rdc.command[7] = p_cgc->cmd[7];
    rdc.command[8] = p_cgc->cmd[8];
    rdc.command[9] = p_cgc->cmd[9];
    rdc.command[10] = p_cgc->cmd[10];
    rdc.command[11] = p_cgc->cmd[11];
    rdc.command[12] = p_cgc->cmd[12];

    return ioctl( i_fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc) );
}

#endif

