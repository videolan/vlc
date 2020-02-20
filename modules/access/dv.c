/*****************************************************************************
 * dv.c: Digital video/Firewire input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2005 M2X
 *
 * Authors: Jean-Paul Saman <jpsaman at m2x dot nl>
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
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
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
static block_t *Block( stream_t *, bool * );
static int Control( stream_t *, int, va_list );

vlc_module_begin ()
    set_description( N_("Digital Video (Firewire/ieee1394) input") )
    set_shortname( N_("DV") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access", 0 )
    add_shortcut( "dv", "raw1394" )
    set_callbacks( Open, Close )
vlc_module_end ()

typedef struct
{
    vlc_thread_t    thread;
    stream_t        *p_access;
    vlc_mutex_t     lock;
    block_t         *p_frame;
    block_t         **pp_last;

} event_thread_t;

static void* Raw1394EventThread( void * );
static enum raw1394_iso_disposition
Raw1394Handler(raw1394handle_t, unsigned char *,
        unsigned int, unsigned char,
        unsigned char, unsigned char, unsigned int,
        unsigned int);

static int Raw1394GetNumPorts( stream_t *p_access );
static raw1394handle_t Raw1394Open( stream_t *, int );
static void Raw1394Close( raw1394handle_t );

static int DiscoverAVC( stream_t *, int *, uint64_t );
static raw1394handle_t AVCOpen( stream_t *, int );
static void AVCClose( stream_t * );
static int AVCResetHandler( raw1394handle_t, unsigned int );
static int AVCPlay( stream_t *, int );
static int AVCPause( stream_t *, int );
static int AVCStop( stream_t *, int );

typedef struct
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
} access_sys_t;

#define ISOCHRONOUS_QUEUE_LENGTH 1000
#define ISOCHRONOUS_MAX_PACKET_SIZE 4096

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *p_sys;

    struct raw1394_portinfo port_inf[ 16 ];

    msg_Dbg( p_access, "opening device" );

    /* Set up p_access */
    ACCESS_SET_CALLBACKS( NULL, Block, Control, NULL );

    p_access->p_sys = p_sys = vlc_obj_malloc( p_this, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_EGENERIC;

    p_sys->i_cards = 0;
    p_sys->i_node = 0;
    p_sys->i_port = 0;
    p_sys->i_guid = 0;
    p_sys->i_channel = 63;
    p_sys->p_raw1394 = NULL;
    p_sys->p_avc1394 = NULL;
    p_sys->p_frame = NULL;
    p_sys->p_ev = NULL;

    vlc_mutex_init( &p_sys->lock );

    p_sys->i_node = DiscoverAVC( p_access, &p_sys->i_port, p_sys->i_guid );
    if( p_sys->i_node < 0 )
    {
        msg_Err( p_access, "failed to open a Firewire (IEEE1394) connection" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    p_sys->p_avc1394 = AVCOpen( p_access, p_sys->i_port );
    if( !p_sys->p_avc1394 )
    {
        msg_Err( p_access, "no Digital Video Control device found" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    p_sys->p_raw1394 = raw1394_new_handle();
    if( !p_sys->p_raw1394 )
    {
        msg_Err( p_access, "no Digital Video device found" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    p_sys->i_cards = raw1394_get_port_info( p_sys->p_raw1394, port_inf, 16 );
    if( p_sys->i_cards < 0 )
    {
        msg_Err( p_access, "failed to get port info" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    if( raw1394_set_port( p_sys->p_raw1394, p_sys->i_port ) < 0 )
    {
        msg_Err( p_access, "failed to set port info" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    if ( raw1394_iso_recv_init( p_sys->p_raw1394, Raw1394Handler,
                ISOCHRONOUS_QUEUE_LENGTH, ISOCHRONOUS_MAX_PACKET_SIZE,
                p_sys->i_channel, RAW1394_DMA_PACKET_PER_BUFFER, -1 ) < 0 )
    {
        msg_Err( p_access, "failed to init isochronous recv" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    raw1394_set_userdata( p_sys->p_raw1394, p_access );
    raw1394_iso_recv_start( p_sys->p_raw1394, -1, -1, 0 );

    p_sys->raw1394_poll.fd = raw1394_get_fd( p_sys->p_raw1394 );
    p_sys->raw1394_poll.events = POLLIN | POLLPRI;

    /* Now create our event thread catcher */
    p_sys->p_ev = calloc( 1, sizeof( *p_sys->p_ev ) );
    if( !p_sys->p_ev )
    {
        msg_Err( p_access, "failed to create event thread struct" );
        Close( p_this );
        return VLC_ENOMEM;
    }

    p_sys->p_ev->p_frame = NULL;
    p_sys->p_ev->pp_last = &p_sys->p_ev->p_frame;
    p_sys->p_ev->p_access = p_access;
    vlc_mutex_init( &p_sys->p_ev->lock );
    if( vlc_clone( &p_sys->p_ev->thread, Raw1394EventThread,
               p_sys->p_ev, VLC_THREAD_PRIORITY_OUTPUT ) )
    {
        msg_Err( p_access, "failed to clone event thread" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_ev )
    {
        /* stop the event handler */
        vlc_cancel( p_sys->p_ev->thread );

        if( p_sys->p_raw1394 )
            raw1394_iso_shutdown( p_sys->p_raw1394 );

        vlc_join( p_sys->p_ev->thread, NULL );

        /* Cleanup frame data */
        if( p_sys->p_ev->p_frame )
        {
            block_ChainRelease( p_sys->p_ev->p_frame );
            p_sys->p_ev->p_frame = NULL;
            p_sys->p_ev->pp_last = &p_sys->p_frame;
        }
        free( p_sys->p_ev );
    }

    if( p_sys->p_frame )
        block_ChainRelease( p_sys->p_frame );
    if( p_sys->p_raw1394 )
        raw1394_destroy_handle( p_sys->p_raw1394 );

    AVCClose( p_access );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( stream_t *p_access, int i_query, va_list args )
{
    access_sys_t *sys = p_access->p_sys;

    switch( i_query )
    {
        /* */
        case STREAM_CAN_PAUSE:
            *va_arg( args, bool* ) = true;
            break;

       case STREAM_CAN_SEEK:
       case STREAM_CAN_FASTSEEK:
       case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS( var_InheritInteger( p_access, "live-caching" ) );
            break;

        /* */
        case STREAM_SET_PAUSE_STATE:
            AVCPause( p_access, sys->i_node );
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static block_t *Block( stream_t *p_access, bool *restrict eof )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t *p_block = NULL;

    vlc_mutex_lock( &p_sys->lock );
    p_block = p_sys->p_frame;
    //msg_Dbg( p_access, "sending frame %p", (void *)p_block );
    p_sys->p_frame = NULL;
    vlc_mutex_unlock( &p_sys->lock );

    (void) eof;
    return p_block;
}

static void Raw1394EventThreadCleanup( void *obj )
{
    event_thread_t *p_ev = (event_thread_t *)obj;
    access_sys_t *sys = p_ev->p_access->p_sys;

    AVCStop( p_ev->p_access, sys->i_node );
}

static void* Raw1394EventThread( void *obj )
{
    event_thread_t *p_ev = (event_thread_t *)obj;
    stream_t *p_access = (stream_t *) p_ev->p_access;
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    int result = 0;
    int canc = vlc_savecancel();

    AVCPlay( p_access, p_sys->i_node );
    vlc_cleanup_push( Raw1394EventThreadCleanup, p_ev );
    vlc_restorecancel( canc );

    for( ;; )
    {
        while( ( result = poll( &p_sys->raw1394_poll, 1, -1 ) ) < 0 )
        {
            if( errno != EINTR )
                msg_Err( p_access, "poll error: %s", vlc_strerror_c(errno) );
        }

        if( result > 0 && ( ( p_sys->raw1394_poll.revents & POLLIN )
                         || ( p_sys->raw1394_poll.revents & POLLPRI ) ) )
        {
            canc = vlc_savecancel();
            result = raw1394_loop_iterate( p_sys->p_raw1394 );
            vlc_restorecancel( canc );
        }
    }

    vlc_cleanup_pop();
    vlc_assert_unreachable();
}

static enum raw1394_iso_disposition
Raw1394Handler(raw1394handle_t handle, unsigned char *data,
        unsigned int length, unsigned char channel,
        unsigned char tag, unsigned char sy, unsigned int cycle,
        unsigned int dropped)
{
    stream_t *p_access = NULL;
    access_sys_t *p_sys = NULL;
    block_t *p_block = NULL;
    VLC_UNUSED(channel); VLC_UNUSED(tag);
    VLC_UNUSED(sy); VLC_UNUSED(cycle); VLC_UNUSED(dropped);

    p_access = (stream_t *) raw1394_get_userdata( handle );
    if( !p_access ) return 0;

    p_sys = p_access->p_sys;

    /* skip empty packets */
    if( length > 16 )
    {
        unsigned char * p = data + 8;
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
                //p_sys->p_ev->p_frame->i_pts = vlc_tick_now();
                block_ChainAppend( &p_sys->p_frame, p_sys->p_ev->p_frame );
            }
            /* reset list */
            p_sys->p_ev->p_frame = block_Alloc( 144000 );
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
static int Raw1394GetNumPorts( stream_t *p_access )
{
    int n_ports;
    struct raw1394_portinfo pinf[ 16 ];
    raw1394handle_t handle;

    /* get a raw1394 handle */
    if( !( handle = raw1394_new_handle() ) )
    {
        msg_Err( p_access, "raw1394 - failed to get handle: %s",
                 vlc_strerror_c(errno) );
        return VLC_EGENERIC;
    }

    if( ( n_ports = raw1394_get_port_info( handle, pinf, 16 ) ) < 0 )
    {
        msg_Err( p_access, "raw1394 - failed to get port info: %s",
                 vlc_strerror_c(errno) );
        raw1394_destroy_handle( handle );
        return VLC_EGENERIC;
    }
    raw1394_destroy_handle( handle );

    return n_ports;
}

static raw1394handle_t Raw1394Open( stream_t *p_access, int port )
{
    int n_ports;
    struct raw1394_portinfo pinf[ 16 ];
    raw1394handle_t handle;

    /* get a raw1394 handle */
    handle = raw1394_new_handle();
    if( !handle )
    {
        msg_Err( p_access, "raw1394 - failed to get handle: %s",
                 vlc_strerror_c(errno) );
        return NULL;
    }

    if( ( n_ports = raw1394_get_port_info( handle, pinf, 16 ) ) < 0 )
    {
        msg_Err( p_access, "raw1394 - failed to get port info: %s",
                 vlc_strerror_c(errno) );
        raw1394_destroy_handle( handle );
        return NULL;
    }

    /* tell raw1394 which host adapter to use */
    if( raw1394_set_port( handle, port ) < 0 )
    {
        msg_Err( p_access, "raw1394 - failed to set set port: %s",
                 vlc_strerror_c(errno) );
        return NULL;
    }

    return handle;
}

static void Raw1394Close( raw1394handle_t handle )
{
    raw1394_destroy_handle( handle );
}

static int DiscoverAVC( stream_t *p_access, int* port, uint64_t guid )
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
                    msg_Err( p_access, "error reading config rom directory for node %d", i );
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
static raw1394handle_t AVCOpen( stream_t *p_access, int port )
{
    access_sys_t *p_sys = p_access->p_sys;
    struct raw1394_portinfo pinf[ 16 ];
    int numcards;

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

static void AVCClose( stream_t *p_access )
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

static int AVCPlay( stream_t *p_access, int phyID )
{
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "send play command over Digital Video control channel" );

    if( p_sys->p_avc1394 && phyID >= 0 )
    {
        if( !avc1394_vcr_is_recording( p_sys->p_avc1394, phyID ) &&
            avc1394_vcr_is_playing( p_sys->p_avc1394, phyID ) != AVC1394_VCR_OPERAND_PLAY_FORWARD )
                avc1394_vcr_play( p_sys->p_avc1394, phyID );
    }
    return 0;
}

static int AVCPause( stream_t *p_access, int phyID )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_avc1394 && phyID >= 0 )
    {
        if( !avc1394_vcr_is_recording( p_sys->p_avc1394, phyID ) &&
            ( avc1394_vcr_is_playing( p_sys->p_avc1394, phyID ) != AVC1394_VCR_OPERAND_PLAY_FORWARD_PAUSE ) )
                avc1394_vcr_pause( p_sys->p_avc1394, phyID );
    }
    return 0;
}


static int AVCStop( stream_t *p_access, int phyID )
{
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "closing Digital Video control channel" );

    if ( p_sys->p_avc1394 && phyID >= 0 )
        avc1394_vcr_stop( p_sys->p_avc1394, phyID );

    return 0;
}
