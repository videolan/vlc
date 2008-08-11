/*****************************************************************************
 * rtmp.c: RTMP output.
 *****************************************************************************
 * Copyright (C) URJC - LADyR - Luis Lopez Fernandez
 *
 * Author: Miguel Angel Cabrera Moya
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

#include <vlc_network.h> /* DOWN: #include <network.h> */
#include <vlc_url.h>
#include <vlc_block.h>

#include "../access/rtmp/rtmp_amf_flv.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define RTMP_CONNECT_TEXT N_( "Active TCP connection" )
#define RTMP_CONNECT_LONGTEXT N_( \
    "If enabled, VLC will connect to a remote destination instead of " \
    "waiting for an incoming connection." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-rtmp-"

vlc_module_begin();
    set_description( N_("RTMP stream output") );
    set_shortname( N_("RTMP" ) );
    set_capability( "sout access", 50 );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_STREAM );
    add_shortcut( "rtmp" );
    set_callbacks( Open, Close );
    add_bool( "rtmp-connect", false, NULL, RTMP_CONNECT_TEXT,
              RTMP_CONNECT_LONGTEXT, false );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *, block_t * );
static int     Seek ( sout_access_out_t *, off_t  );
static void* ThreadControl( vlc_object_t * );

struct sout_access_out_sys_t
{
    int active;

    /* thread for filtering and handling control messages */
    rtmp_control_thread_t *p_thread;
};

/*****************************************************************************
 * Open: open the rtmp connection
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t *) p_this;
    sout_access_out_sys_t *p_sys;
    char *psz, *p;
    int length_path, length_media_name;
    int i;

    if( !( p_sys = calloc ( 1, sizeof( sout_access_out_sys_t ) ) ) )
    {
        msg_Err( p_access, "not enough memory" );
        return VLC_ENOMEM;
    }
    p_access->p_sys = p_sys;

    p_sys->p_thread =
        vlc_object_create( p_access, sizeof( rtmp_control_thread_t ) );
    if( !p_sys->p_thread )
    {
        msg_Err( p_access, "out of memory" );
        return VLC_ENOMEM;
    }
    vlc_object_attach( p_sys->p_thread, p_access );

    /* Parse URI - remove spaces */
    p = psz = strdup( p_access->psz_path );
    while( ( p = strchr( p, ' ' ) ) != NULL )
        *p = '+';
    vlc_UrlParse( &p_sys->p_thread->url, psz, 0 );
    free( psz );

    if( p_sys->p_thread->url.psz_host == NULL
        || *p_sys->p_thread->url.psz_host == '\0' )
    {
         msg_Warn( p_access, "invalid host" );
         goto error;
    }

    if( p_sys->p_thread->url.i_port <= 0 )
        p_sys->p_thread->url.i_port = 1935;

    if ( p_sys->p_thread->url.psz_path == NULL )
    {
        msg_Warn( p_access, "invalid path" );
        goto error;
    }

    length_path = strlen( p_sys->p_thread->url.psz_path );
    length_media_name = strlen( strrchr( p_sys->p_thread->url.psz_path, '/' ) ) - 1;

    p_sys->p_thread->psz_application = strndup( p_sys->p_thread->url.psz_path + 1, length_path - length_media_name - 2 );
    p_sys->p_thread->psz_media = strdup( p_sys->p_thread->url.psz_path + ( length_path - length_media_name ) );

    msg_Dbg( p_access, "rtmp: host='%s' port=%d path='%s'",
             p_sys->p_thread->url.psz_host, p_sys->p_thread->url.i_port, p_sys->p_thread->url.psz_path );

    if( p_sys->p_thread->url.psz_username && *p_sys->p_thread->url.psz_username )
    {
        msg_Dbg( p_access, "      user='%s', pwd='%s'",
                 p_sys->p_thread->url.psz_username, p_sys->p_thread->url.psz_password );
    }

    /* Initialize thread variables */
    p_sys->p_thread->b_die = 0;
    p_sys->p_thread->b_error= 0;
    p_sys->p_thread->p_fifo_input = block_FifoNew();
    p_sys->p_thread->p_empty_blocks = block_FifoNew();
    p_sys->p_thread->has_audio = 0;
    p_sys->p_thread->has_video = 0;
    p_sys->p_thread->metadata_received = 0;
    p_sys->p_thread->first_media_packet = 1;
    p_sys->p_thread->flv_tag_previous_tag_size = 0x00000000; /* FLV_TAG_FIRST_PREVIOUS_TAG_SIZE */

    p_sys->p_thread->flv_body = rtmp_body_new( -1 );
    p_sys->p_thread->flv_length_body = 0;

    p_sys->p_thread->chunk_size_recv = 128; /* RTMP_DEFAULT_CHUNK_SIZE */
    p_sys->p_thread->chunk_size_send = 128; /* RTMP_DEFAULT_CHUNK_SIZE */
    for(i = 0; i < 64; i++)
    {
        memset( &p_sys->p_thread->rtmp_headers_recv[i], 0, sizeof( rtmp_packet_t ) );
        p_sys->p_thread->rtmp_headers_send[i].length_header = -1;
        p_sys->p_thread->rtmp_headers_send[i].stream_index = -1;
        p_sys->p_thread->rtmp_headers_send[i].timestamp = -1;
        p_sys->p_thread->rtmp_headers_send[i].timestamp_relative = -1;
        p_sys->p_thread->rtmp_headers_send[i].length_encoded = -1;
        p_sys->p_thread->rtmp_headers_send[i].length_body = -1;
        p_sys->p_thread->rtmp_headers_send[i].content_type = -1;
        p_sys->p_thread->rtmp_headers_send[i].src_dst = -1;
        p_sys->p_thread->rtmp_headers_send[i].body = NULL;
    }

    vlc_cond_init( p_sys->p_thread, &p_sys->p_thread->wait );
    vlc_mutex_init( &p_sys->p_thread->lock );

    p_sys->p_thread->result_connect = 1;
    /* p_sys->p_thread->result_publish = only used on access */
    p_sys->p_thread->result_play = 1;
    p_sys->p_thread->result_stop = 0;
    p_sys->p_thread->fd = -1;

    /* Open connection */
    if( var_CreateGetBool( p_access, "rtmp-connect" ) > 0 )
    {
#if 0
        p_sys->p_thread->fd = net_ConnectTCP( p_access,
                                              p_sys->p_thread->url.psz_host,
                                              p_sys->p_thread->url.i_port );
#endif
        msg_Err( p_access, "to be implemented" );
        goto error2;
    }
    else
    {
        int *p_fd_listen;

        p_sys->active = 0;
        p_fd_listen = net_ListenTCP( p_access, p_sys->p_thread->url.psz_host,
                                     p_sys->p_thread->url.i_port );
        if( p_fd_listen == NULL )
        {
            msg_Warn( p_access, "cannot listen to %s port %i",
                      p_sys->p_thread->url.psz_host,
                      p_sys->p_thread->url.i_port );
            goto error2;
        }

        do
            p_sys->p_thread->fd = net_Accept( p_access, p_fd_listen, -1 );
        while( p_sys->p_thread->fd == -1 );
        net_ListenClose( p_fd_listen );

        if( rtmp_handshake_passive( p_this, p_sys->p_thread->fd ) < 0 )
        {
            msg_Err( p_access, "handshake passive failed");
            goto error2;
        }
    }

    if( vlc_thread_create( p_sys->p_thread, "rtmp control thread", ThreadControl,
                           VLC_THREAD_PRIORITY_INPUT, false ) )
    {
        msg_Err( p_access, "cannot spawn rtmp control thread" );
        goto error2;
    }

    if( !p_sys->active )
    {
        if( rtmp_connect_passive( p_sys->p_thread ) < 0 )
        {
            msg_Err( p_access, "connect passive failed");
            goto error2;
        }
    }

    p_access->pf_write = Write;
    p_access->pf_seek = Seek;

    return VLC_SUCCESS;

error2:
    vlc_cond_destroy( &p_sys->p_thread->wait );
    vlc_mutex_destroy( &p_sys->p_thread->lock );

    free( p_sys->p_thread->psz_application );
    free( p_sys->p_thread->psz_media );

    if( p_sys->p_thread->fd != -1 )
        net_Close( p_sys->p_thread->fd );
error:
    vlc_object_detach( p_sys->p_thread );
    vlc_object_release( p_sys->p_thread );

    vlc_UrlClean( &p_sys->p_thread->url );
    free( p_sys );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t *) p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i;

//    p_sys->p_thread->b_die = true;
    vlc_object_kill( p_sys->p_thread );
    block_FifoWake( p_sys->p_thread->p_fifo_input );
    block_FifoWake( p_sys->p_thread->p_empty_blocks );

    vlc_thread_join( p_sys->p_thread );

    vlc_cond_destroy( &p_sys->p_thread->wait );
    vlc_mutex_destroy( &p_sys->p_thread->lock );

    block_FifoRelease( p_sys->p_thread->p_fifo_input );
    block_FifoRelease( p_sys->p_thread->p_empty_blocks );

    for( i = 0; i < 64; i++ ) /* RTMP_HEADER_STREAM_INDEX_MASK */
    {
        if( p_sys->p_thread->rtmp_headers_recv[i].body != NULL )
        {
            free( p_sys->p_thread->rtmp_headers_recv[i].body->body );
            free( p_sys->p_thread->rtmp_headers_recv[i].body );
        }
    }

    net_Close( p_sys->p_thread->fd );

    vlc_object_detach( p_sys->p_thread );
    vlc_object_release( p_sys->p_thread );

    vlc_UrlClean( &p_sys->p_thread->url );
    free( p_sys->p_thread->psz_application );
    free( p_sys->p_thread->psz_media );
    free( p_sys );
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    rtmp_packet_t *rtmp_packet;
    uint8_t *tmp_buffer;
    ssize_t i_ret;
    ssize_t i_write = 0;

    if( p_access->p_sys->p_thread->first_media_packet )
    {
        /* 13 == FLV_HEADER_SIZE + PreviousTagSize*/
        memmove( p_buffer->p_buffer, p_buffer->p_buffer + 13, p_buffer->i_buffer - 13 );
        p_buffer = block_Realloc( p_buffer, 0, p_buffer->i_buffer - 13 );

        p_access->p_sys->p_thread->first_media_packet = 0;
    }

    while( p_buffer )
    {
        block_t *p_next = p_buffer->p_next;
//////////////////////////////
/*msg_Warn(p_access, "XXXXXXXXXXXXXXXXX");
int i;
for(i = 0; i < p_buffer->i_buffer; i += 16)
{
    msg_Warn(p_access,"%.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x",
p_buffer->p_buffer[i], p_buffer->p_buffer[i+1], p_buffer->p_buffer[i+2], p_buffer->p_buffer[i+3], p_buffer->p_buffer[i+4], p_buffer->p_buffer[i+5], p_buffer->p_buffer[i+6], p_buffer->p_buffer[i+7],
p_buffer->p_buffer[i+8], p_buffer->p_buffer[i+9], p_buffer->p_buffer[i+10], p_buffer->p_buffer[i+11], p_buffer->p_buffer[i+12], p_buffer->p_buffer[i+13], p_buffer->p_buffer[i+14], p_buffer->p_buffer[i+15]);
}*/
////////////////////////
        msg_Warn(p_access, "rtmp.c:360 i_dts %"PRIu64" i_pts %"PRIu64,
                 p_buffer->i_dts, p_buffer->i_pts);
        rtmp_packet = rtmp_build_flv_over_rtmp( p_access->p_sys->p_thread, p_buffer );

        if( rtmp_packet )
        {
            tmp_buffer = rtmp_encode_packet( p_access->p_sys->p_thread, rtmp_packet );

            i_ret = net_Write( p_access->p_sys->p_thread, p_access->p_sys->p_thread->fd, NULL, tmp_buffer, rtmp_packet->length_encoded );
            if( i_ret != rtmp_packet->length_encoded )
            {
                free( rtmp_packet->body->body );
                free( rtmp_packet->body );
                free( rtmp_packet );
                free( tmp_buffer );
                msg_Err( p_access->p_sys->p_thread, "failed send flv packet" );
                return -1;
            }
            free( rtmp_packet->body->body );
            free( rtmp_packet->body );
            free( rtmp_packet );
            free( tmp_buffer );
        }

        i_write += p_buffer->i_buffer;

        p_buffer = p_next;
    }

    return i_write;
}

/********************a*********************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    (void)i_pos;
    msg_Err( p_access, "RTMP sout access cannot seek" );
    return -1;
}

/*****************************************************************************
 * ThreadControl: manage control messages and pipe media to Read
 *****************************************************************************/
static void* ThreadControl( vlc_object_t *p_this )
{
    rtmp_control_thread_t *p_thread = (rtmp_control_thread_t *) p_this;
    rtmp_packet_t *rtmp_packet;

    rtmp_init_handler( p_thread->rtmp_handler );

    while( vlc_object_alive (p_thread) )
    {
        rtmp_packet = rtmp_read_net_packet( p_thread );
        if( rtmp_packet != NULL )
        {
            if( rtmp_packet->content_type < 0x01 /* RTMP_CONTENT_TYPE_CHUNK_SIZE */
                || rtmp_packet->content_type > 0x14 ) /* RTMP_CONTENT_TYPE_INVOKE */
            {
                free( rtmp_packet->body->body );
                free( rtmp_packet->body );
                free( rtmp_packet );

                msg_Warn( p_thread, "unknown content type received" );
            }
            else
                p_thread->rtmp_handler[rtmp_packet->content_type]( p_thread, rtmp_packet );
        }
        else
        {
            /* Sometimes server close connection too soon */
            if( p_thread->result_connect )
            {
                vlc_mutex_lock( &p_thread->lock );
                vlc_cond_signal( &p_thread->wait );
                vlc_mutex_unlock( &p_thread->lock );
            }

            p_thread->b_die = 1;
        }
    }
    return NULL;
}
