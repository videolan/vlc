/*****************************************************************************
 * dv.c: Digital video/Firewire input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2005 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman at m2x dot nl>
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
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/input.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#include <sys/poll.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include <libavc1394/rom1394.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static block_t *Block( access_t * );
static int Control( access_t *, int, va_list );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for DV streams. This" \
    "value should be set in milliseconds." )

vlc_module_begin();
    set_description( _("Digital Video (Firewire/ieee1394)  input") );
    set_shortname( _("dv") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "dv-caching", 60000 / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_capability( "access2", 0 );
    add_shortcut( "dv" );
    add_shortcut( "dv1394" );
    add_shortcut( "raw1394" );
    set_callbacks( Open, Close );
vlc_module_end();

typedef struct
{
    VLC_COMMON_MEMBERS

    access_t        *p_access;
    vlc_mutex_t     lock;
    block_t         *p_frame;
    block_t         **pp_last;

} event_thread_t;

static int Raw1394EventThread( vlc_object_t * );
static int Raw1394Handler( raw1394handle_t, int, size_t, quadlet_t * );

static int Raw1394GetNumPorts( access_t *p_access );
static raw1394handle_t Raw1394Open( access_t *, int );
static void Raw1394Close( raw1394handle_t );

static int DiscoverAVC( access_t *, int *, uint64_t );
static raw1394handle_t AVCOpen( access_t *, int );
static void AVCClose( access_t * );
static int AVCResetHandler( raw1394handle_t, unsigned int );
static int AVCPlay( access_t *, int );
static int AVCPause( access_t *, int );
static int AVCStop( access_t *, int );

struct access_sys_t
{
    raw1394handle_t p_avc1394;
    raw1394handle_t p_raw1394;
    struct pollfd   raw1394_poll;

    int i_cards;
    int i_node;
    int i_port;
    int i_channel;
    uint64_t i_guid;

    /* event */
    event_thread_t *p_ev;
    vlc_mutex_t lock;
    block_t *p_frame;
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char *psz_name = strdup( p_access->psz_path );

    struct raw1394_portinfo port_inf[ 16 ];
    iso_handler_t oldhandler;

    msg_Dbg( p_access, "opening device %s", psz_name );

    /* Set up p_access */
    p_access->pf_read = NULL;
    p_access->pf_block = Block;
    p_access->pf_control = Control;
    p_access->pf_seek = NULL;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.b_prebuffered = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;

    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    if( !p_sys )
    {
        free( psz_name );
        return VLC_EGENERIC;
    }

    p_sys->i_cards = 0;
    p_sys->i_node = 0;
    p_sys->i_port = 0;
    p_sys->i_guid = 0;
    p_sys->i_channel = 63;
    p_sys->p_raw1394 = NULL;
    p_sys->p_avc1394 = NULL;
    p_sys->p_frame = NULL;

    vlc_mutex_init( p_access, &p_sys->lock );

    p_sys->i_node = DiscoverAVC( p_access, &p_sys->i_port, p_sys->i_guid );
    if( p_sys->i_node < 0 )
    {
        msg_Err( p_access, "failed to open a Firewire (IEEE1394) connection" );
        Close( p_this );
        free( psz_name );
        return VLC_EGENERIC;
    }

    p_sys->p_avc1394 = AVCOpen( p_access, p_sys->i_port );
    if( !p_sys->p_avc1394 )
    {
        msg_Err( p_access, "no Digital Video Control device found on %s", psz_name );
        Close( p_this );
        free( psz_name );
        return VLC_EGENERIC;
    }

    p_sys->p_raw1394 = raw1394_new_handle();
    if( !p_sys->p_raw1394 )
    {
        msg_Err( p_access, "no Digital Video device found on %s", psz_name );
        Close( p_this );
        free( psz_name );
        return VLC_EGENERIC;
    }

    p_sys->i_cards = raw1394_get_port_info( p_sys->p_raw1394, port_inf, 16 );
    if( p_sys->i_cards < 0 )
    {
        msg_Err( p_access, "failed to get port info for %s", psz_name );
        Close( p_this );
        free( psz_name );
        return VLC_EGENERIC;
    }

    if( raw1394_set_port( p_sys->p_raw1394, p_sys->i_port ) < 0 )
    {
        msg_Err( p_access, "failed to set port info for %s", psz_name );
        Close( p_this );
        free( psz_name );
        return VLC_EGENERIC;
    }

    oldhandler = raw1394_set_iso_handler( p_sys->p_raw1394,
                                          p_sys->i_channel, Raw1394Handler );
    raw1394_set_userdata( p_sys->p_raw1394, p_access );
    raw1394_start_iso_rcv( p_sys->p_raw1394, p_sys->i_channel );

    p_sys->raw1394_poll.fd = raw1394_get_fd( p_sys->p_raw1394 );
    p_sys->raw1394_poll.events = POLLIN | POLLERR | POLLHUP | POLLPRI;

    /* Update default_pts to a suitable value for udp access */
    var_Create( p_access, "dv-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Now create our event thread catcher */
    p_sys->p_ev = vlc_object_create( p_access, sizeof( event_thread_t ) );
    p_sys->p_ev->p_frame = NULL;
    p_sys->p_ev->pp_last = &p_sys->p_ev->p_frame;
    p_sys->p_ev->p_access = p_access;
    vlc_mutex_init( p_access, &p_sys->p_ev->lock );
    vlc_thread_create( p_sys->p_ev, "dv event thread handler", Raw1394EventThread,
                       VLC_THREAD_PRIORITY_OUTPUT, VLC_FALSE );

    free( psz_name );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_ev )
    {
        /* stop the event handler */
        p_sys->p_ev->b_die = VLC_TRUE;

        if( p_sys->p_raw1394 )
            raw1394_stop_iso_rcv( p_sys->p_raw1394, p_sys->i_channel );

        vlc_mutex_destroy( &p_sys->p_ev->lock );
        vlc_thread_join( p_sys->p_ev );

        /* Cleanup frame data */
        if( p_sys->p_ev->p_frame )
        {
            vlc_mutex_lock( &p_sys->p_ev->lock );
            block_ChainRelease( p_sys->p_ev->p_frame );
            p_sys->p_ev->p_frame = NULL;
            p_sys->p_ev->pp_last = &p_sys->p_frame;
            vlc_mutex_unlock( &p_sys->p_ev->lock );
        }
        vlc_object_destroy( p_sys->p_ev );
    }

    if( p_sys->p_frame )
        block_ChainRelease( p_sys->p_frame );
    if( p_sys->p_raw1394 )
        raw1394_destroy_handle( p_sys->p_raw1394 );

    AVCClose( p_access );

    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    vlc_bool_t   *pb_bool;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;

        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, "dv-caching" ) * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            AVCPause( p_access, p_sys->i_node );
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static block_t *Block( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t *p_block = NULL;

//     if( !p_access->psz_demux )
//         p_access->psz_demux = strdup( "rawdv" );

    vlc_mutex_lock( &p_sys->lock );
    p_block = p_sys->p_frame;
    //msg_Dbg( p_access, "sending frame %p",p_block );
    p_sys->p_frame = NULL;
    vlc_mutex_unlock( &p_sys->lock );

    return p_block;
}

static int Raw1394EventThread( vlc_object_t *p_this )
{
    event_thread_t *p_ev = (event_thread_t *) p_this;
    access_t *p_access = (access_t *) p_ev->p_access;
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    int result = 0;

    AVCPlay( p_access, p_sys->i_node );

    vlc_thread_ready( p_this );

    while( !p_sys->p_ev->b_die )
    {
        while( ( result = poll( &(p_sys->raw1394_poll), 1, 200 ) ) < 0 )
        {
            if( !( errno == EAGAIN || errno == EINTR ) )
            {
                perror( "error: raw1394 poll" );
                msg_Err( p_access, "retrying device raw1394" );
            }
            if( p_sys->p_ev->b_die )
                break;
        }
        if( p_sys->p_ev->b_die )
                break;
        if( result > 0 && ( ( p_sys->raw1394_poll.revents & POLLIN )
                || ( p_sys->raw1394_poll.revents & POLLPRI ) ) )
            result = raw1394_loop_iterate( p_sys->p_raw1394 );
    }

    AVCStop( p_access, p_sys->i_node );
    return VLC_SUCCESS;
}

static int Raw1394Handler( raw1394handle_t handle, int channel, size_t length, quadlet_t *data )
{
    access_t *p_access = NULL;
    access_sys_t *p_sys = NULL;
    block_t *p_block = NULL;

    p_access = (access_t *) raw1394_get_userdata( handle );
    if( !p_access ) return 0;

    p_sys = p_access->p_sys;

    /* skip empty packets */
    if ( length > 16 )
    {
        unsigned char * p = ( unsigned char* ) &data[ 3 ];
        int section_type = p[ 0 ] >> 5;           /* section type is in bits 5 - 7 */
        int dif_sequence = p[ 1 ] >> 4;           /* dif sequence number is in bits 4 - 7 */
        int dif_block = p[ 2 ];

        vlc_mutex_lock( &p_sys->p_ev->lock );

        /* if we are at the beginning of a frame, we put the previous
           frame in our output_queue. */
        if( (section_type == 0) && (dif_sequence == 0) )
        {
            vlc_mutex_lock( &p_sys->lock );
            if( p_sys->p_ev->p_frame )
            {
                /* Push current frame to p_access thread. */
                //p_sys->p_ev->p_frame->i_pts = mdate();
                block_ChainAppend( &p_sys->p_frame, p_sys->p_ev->p_frame );
            }
            /* reset list */
            p_sys->p_ev->p_frame = block_New( p_access, 144000 );
            p_sys->p_ev->pp_last = &p_sys->p_frame;
            vlc_mutex_unlock( &p_sys->lock );
        }

        p_block = p_sys->p_ev->p_frame;
        if( p_block )
        {
            switch ( section_type )
            {
            case 0:    /* 1 Header block */
                /* p[3] |= 0x80; // hack to force PAL data */
                memcpy( p_block->p_buffer + dif_sequence * 150 * 80, p, 480 );
                break;

            case 1:    /* 2 Subcode blocks */
                memcpy( p_block->p_buffer + dif_sequence * 150 * 80 + ( 1 + dif_block ) * 80, p, 480 );
                break;

            case 2:    /* 3 VAUX blocks */
                memcpy( p_block->p_buffer + dif_sequence * 150 * 80 + ( 3 + dif_block ) * 80, p, 480 );
                break;

            case 3:    /* 9 Audio blocks interleaved with video */
                memcpy( p_block->p_buffer + dif_sequence * 150 * 80 + ( 6 + dif_block * 16 ) * 80, p, 480 );
                break;

            case 4:    /* 135 Video blocks interleaved with audio */
                memcpy( p_block->p_buffer + dif_sequence * 150 * 80 + ( 7 + ( dif_block / 15 ) + dif_block ) * 80, p, 480 );
                break;

            default:    /* we can´t handle any other data */
                block_Release( p_block );
                p_block = NULL;
                break;
            }
        }
        vlc_mutex_unlock( &p_sys->p_ev->lock );
    }
    return 0;
}

/*
 * Routing borrowed from dvgrab-1.8
 * Copyright by Arne Schirmacher <dvgrab@schirmacher.de>
 * Dan Dennedy <dan@dennedy.org> and others
 */
static int Raw1394GetNumPorts( access_t *p_access )
{
    int n_ports;
    struct raw1394_portinfo pinf[ 16 ];
    raw1394handle_t handle;

    /* get a raw1394 handle */
    if ( !( handle = raw1394_new_handle() ) )
    {
        msg_Err( p_access, "raw1394 - failed to get handle: %s.\n", strerror( errno ) );
        return VLC_EGENERIC;
    }

    if ( ( n_ports = raw1394_get_port_info( handle, pinf, 16 ) ) < 0 )
    {
        msg_Err( p_access, "raw1394 - failed to get port info: %s.\n", strerror( errno ) );
        raw1394_destroy_handle( handle );
        return VLC_EGENERIC;
    }
    raw1394_destroy_handle( handle );

    return n_ports;
}

static raw1394handle_t Raw1394Open( access_t *p_access, int port )
{
    int n_ports;
    struct raw1394_portinfo pinf[ 16 ];
    raw1394handle_t handle;

    /* get a raw1394 handle */
#ifdef RAW1394_V_0_8

    handle = raw1394_get_handle();
#else

    handle = raw1394_new_handle();
#endif

    if ( !handle )
    {
        msg_Err( p_access, "raw1394 - failed to get handle: %s.\n", strerror( errno ) );
        return NULL;
    }

    if ( ( n_ports = raw1394_get_port_info( handle, pinf, 16 ) ) < 0 )
    {
        msg_Err( p_access, "raw1394 - failed to get port info: %s.\n", strerror( errno ) );
        raw1394_destroy_handle( handle );
        return NULL;
    }

    /* tell raw1394 which host adapter to use */
    if ( raw1394_set_port( handle, port ) < 0 )
    {
        msg_Err( p_access, "raw1394 - failed to set set port: %s.\n", strerror( errno ) );
        return NULL;
    }

    return handle;
}

static void Raw1394Close( raw1394handle_t handle )
{
    raw1394_destroy_handle( handle );
}

static int DiscoverAVC( access_t *p_access, int* port, uint64_t guid )
{
    rom1394_directory rom_dir;
    raw1394handle_t handle = NULL;
    int device = -1;
    int i, j = 0;
    int m = Raw1394GetNumPorts( p_access );

    if( *port >= 0 )
    {
        /* search on explicit port */
        j = *port;
        m = *port + 1;
    }

    for( ; j < m && device == -1; j++ )
    {
        handle = Raw1394Open( p_access, j );
        if( !handle )
            return -1;

        for( i = 0; i < raw1394_get_nodecount( handle ); ++i )
        {
            if( guid != 0 )
            {
                /* select explicitly by GUID */
                if( guid == rom1394_get_guid( handle, i ) )
                {
                    device = i;
                    *port = j;
                    break;
                }
            }
            else
            {
                /* select first AV/C Tape Reccorder Player node */
                if( rom1394_get_directory( handle, i, &rom_dir ) < 0 )
                {
                    msg_Err( p_access, "error reading config rom directory for node %d\n", i );
                    continue;
                }
                if( ( rom1394_get_node_type( &rom_dir ) == ROM1394_NODE_TYPE_AVC ) &&
                        avc1394_check_subunit_type( handle, i, AVC1394_SUBUNIT_TYPE_VCR ) )
                {
                    device = i;
                    *port = j;
                    break;
                }
            }
        }
        Raw1394Close( handle );
    }

    return device;
}

/*
 * Handle AVC commands
 */
static raw1394handle_t AVCOpen( access_t *p_access, int port )
{
    access_sys_t *p_sys = p_access->p_sys;
    int numcards;
    struct raw1394_portinfo pinf[ 16 ];

    p_sys->p_avc1394 = raw1394_new_handle();
    if( !p_sys->p_avc1394 )
        return NULL;

    numcards = raw1394_get_port_info( p_sys->p_avc1394, pinf, 16 );
    if( numcards < -1 )
        return NULL;
    if( raw1394_set_port( p_sys->p_avc1394, port ) < 0 )
        return NULL;

    raw1394_set_bus_reset_handler( p_sys->p_avc1394, AVCResetHandler );

    return  p_sys->p_avc1394;
}

static void AVCClose( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_avc1394 )
    {
        raw1394_destroy_handle( p_sys->p_avc1394 );
        p_sys->p_avc1394 = NULL;
    }
}


static int AVCResetHandler( raw1394handle_t handle, unsigned int generation )
{
    raw1394_update_generation( handle, generation );
    return 0;
}

static int AVCPlay( access_t *p_access, int phyID )
{
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "send play command over Digital Video control channel" );
    if( !p_sys->p_avc1394 )
        return 0;

    if( phyID >= 0 )
    {
        if( !avc1394_vcr_is_recording( p_sys->p_avc1394, phyID ) &&
            avc1394_vcr_is_playing( p_sys->p_avc1394, phyID ) != AVC1394_VCR_OPERAND_PLAY_FORWARD )
                avc1394_vcr_play( p_sys->p_avc1394, phyID );
    }
    return 0;
}

static int AVCPause( access_t *p_access, int phyID )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( !p_sys->p_avc1394 )
        return 0;

    if( phyID >= 0 )
    {
        if( !avc1394_vcr_is_recording( p_sys->p_avc1394, phyID ) &&
            ( avc1394_vcr_is_playing( p_sys->p_avc1394, phyID ) != AVC1394_VCR_OPERAND_PLAY_FORWARD_PAUSE ) )
                avc1394_vcr_pause( p_sys->p_avc1394, phyID );
    }
    return 0;
}


static int AVCStop( access_t *p_access, int phyID )
{
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "closing Digital Video control channel" );
    if( !p_sys->p_avc1394 )
        return 0;

    if ( phyID >= 0 )
        avc1394_vcr_stop( p_sys->p_avc1394, phyID );

    return 0;
}
