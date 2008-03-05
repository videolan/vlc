/*****************************************************************************
 * access.c: RTMP input.
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

#include <vlc/vlc.h>
#include <vlc_access.h>

#include <vlc_network.h> /* DOWN: #include <network.h> */
#include <vlc_url.h>
#include <vlc_block.h>

#include "rtmp_amf_flv.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for RTMP streams. This " \
    "value should be set in milliseconds." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("RTMP input") );
    set_shortname( _("RTMP") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "rtmp-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, VLC_TRUE );

    set_capability( "access2", 10 );
    set_callbacks( Open, Close );
    add_shortcut( "rtmp" );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Read( access_t *, uint8_t *, size_t ); /*DOWN: last parameter int */
static int Seek( access_t *, int64_t );
static int Control( access_t *, int, va_list );

static void ThreadControl( vlc_object_t * );

/*****************************************************************************
 * Open: open the rtmp connection
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t *) p_this;
    access_sys_t *p_sys;
    char *psz, *p; 
    int length_path, length_media_name;
    int i;

    /*DOWN:
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->pf_read = Read;
    p_access->pf_block = Block;
    p_access->pf_control = Control;
    p_access->pf_seek = Seek;
    do
    {
        p_access->p_sys = (access_sys_t *) malloc( sizeof( access_sys_t ) );
        if( !p_access->p_sys )
            return VLC_ENOMEM;
    } while(0);
    p_sys = p_access->p_sys;
    memset( p_sys, 0, sizeof( access_sys_t ) );
    */
    STANDARD_READ_ACCESS_INIT

    /* Parse URI - remove spaces */
    p = psz = strdup( p_access->psz_path );
    while( (p = strchr( p, ' ' )) != NULL )
        *p = '+';
    vlc_UrlParse( &p_sys->url, psz, 0 );
    free( psz );

    if( !p_access->psz_access ||
        strncmp( p_access->psz_access, "rtmp", 4 ))
    {   
        msg_Warn( p_access, "invalid protocol" );
        goto error;
    }

    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
         msg_Warn( p_access, "invalid host" );
         goto error;
    }

    if( p_sys->url.i_port <= 0 )
        p_sys->url.i_port = 1935;

    if ( p_sys->url.psz_path == NULL ) {
        msg_Warn( p_access, "invalid path" );
        goto error;
    }

    length_path = strlen( p_sys->url.psz_path );
    length_media_name = strlen( strrchr( p_sys->url.psz_path, '/' ) ) - 1;

    p_sys->psz_application = strndup( p_sys->url.psz_path + 1, length_path - length_media_name - 2 );
    p_sys->psz_media = strdup( p_sys->url.psz_path + ( length_path - length_media_name ) );

    msg_Dbg( p_access, "rtmp: host='%s' port=%d path='%s'",
             p_sys->url.psz_host, p_sys->url.i_port, p_sys->url.psz_path );

    if( p_sys->url.psz_username && *p_sys->url.psz_username )
    {
        msg_Dbg( p_access, "      user='%s', pwd='%s'",
                 p_sys->url.psz_username, p_sys->url.psz_password );
    }

    /* Open connection */
    p_sys->fd = net_ConnectTCP( p_access, p_sys->url.psz_host, p_sys->url.i_port );
    if( p_sys->fd == -1 )
    {
        int *p_fd_listen;

        msg_Warn( p_access, "cannot connect to %s:%d", p_sys->url.psz_host, p_sys->url.i_port );
        msg_Dbg( p_access, "switching to passive mode" );

        p_sys->active = 0;

        p_fd_listen = net_ListenTCP( p_access, p_sys->url.psz_host, p_sys->url.i_port );
        if( p_fd_listen == NULL )
        {
            msg_Warn( p_access, "cannot listen to %s port %i", p_sys->url.psz_host, p_sys->url.i_port );
            goto error;
        }

        p_sys->fd = net_Accept( p_access, p_fd_listen, -1 );

        net_ListenClose( p_fd_listen );

        switch( rtmp_handshake_passive( p_this ) )
        {
            case -1:
                goto error;
            case 0:
                break;
            default:
                msg_Err( p_access, "You should not be here" );
                abort();
        }

        p_sys->p_thread =
            vlc_object_create( p_access, sizeof( rtmp_control_thread_t ) );
        if( !p_sys->p_thread )
        {
            msg_Err( p_access, "out of memory" );
            goto error;
        }

        vlc_object_attach( p_sys->p_thread, p_access );
        p_sys->p_thread->b_die = 0;
        p_sys->p_thread->b_error= 0;
        p_sys->p_thread->fd = p_sys->fd;
        p_sys->p_thread->p_fifo_media = block_FifoNew( p_access );
        p_sys->p_thread->p_empty_blocks = block_FifoNew( p_access );
        p_sys->p_thread->has_audio = 0;
        p_sys->p_thread->has_video = 0;
        p_sys->p_thread->metadata_received = 0;
        p_sys->p_thread->first_media_packet = 1;
        p_sys->p_thread->flv_tag_previous_tag_size = 0x00000000; /* FLV_TAG_FIRST_PREVIOUS_TAG_SIZE */
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
        vlc_mutex_init( p_sys->p_thread, &p_sys->p_thread->lock );

        p_sys->p_thread->result_connect = 1;
        p_sys->p_thread->result_play = 1;
        p_sys->p_thread->result_publish = 1;

        if( vlc_thread_create( p_sys->p_thread, "rtmp control thread", ThreadControl,
                               VLC_THREAD_PRIORITY_INPUT, VLC_FALSE ) )
        {
            msg_Err( p_access, "cannot spawn rtmp control thread" );
            goto error2;
        }
    }
    else
    {
        p_sys->active = 1;

        switch( rtmp_handshake_active( p_this ) )
        {
            case -1:
                goto error;
            case 0:
                break;
            default:
                msg_Err( p_access, "You should not be here" );
                abort();
        }

        p_sys->p_thread =
            vlc_object_create( p_access, sizeof( rtmp_control_thread_t ) );
        if( !p_sys->p_thread )
        {
            msg_Err( p_access, "out of memory" );
            goto error;
        }

        vlc_object_attach( p_sys->p_thread, p_access );
        p_sys->p_thread->b_die = 0;
        p_sys->p_thread->b_error= 0;
        p_sys->p_thread->fd = p_sys->fd;
        p_sys->p_thread->p_fifo_media = block_FifoNew( p_access );
        p_sys->p_thread->p_empty_blocks = block_FifoNew( p_access );
        p_sys->p_thread->has_audio = 0;
        p_sys->p_thread->has_video = 0;
        p_sys->p_thread->metadata_received = 0;
        p_sys->p_thread->first_media_packet = 1;
        p_sys->p_thread->flv_tag_previous_tag_size = 0x00000000; /* FLV_TAG_FIRST_PREVIOUS_TAG_SIZE */
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
        vlc_mutex_init( p_sys->p_thread, &p_sys->p_thread->lock );    

        p_sys->p_thread->result_connect = 1;
        p_sys->p_thread->result_play = 1;
        p_sys->p_thread->result_publish = 0;

        if( vlc_thread_create( p_sys->p_thread, "rtmp control thread", ThreadControl,
                               VLC_THREAD_PRIORITY_INPUT, VLC_FALSE ) )
        {
            msg_Err( p_access, "cannot spawn rtmp control thread" );
            goto error2;
        }

        switch( rtmp_connect_active( p_this ) )
        {
            case -1:
                goto error2;
            case 0:
                break;
            default:
                msg_Err( p_access, "You should not be here" );
                abort();
        }
    }

    /* Set vars for reading from fifo */
    p_access->p_sys->flv_packet = NULL;
    p_access->p_sys->read_packet = 1;

    /* Wait until enough data is received for extracting metadata */
    while( block_FifoCount( p_access->p_sys->p_thread->p_fifo_media ) < 10 )
    {
        msleep(1000);
        continue;
    }

    /* Update default_pts to a suitable value for rtmp access */
    var_Create( p_access, "rtmp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;

error2:

error:
    vlc_UrlClean( &p_sys->url );
    net_Close( p_sys-> fd );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the rtmp connection
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t     *p_access = (access_t *) p_this;
    access_sys_t *p_sys = p_access->p_sys;
    int i;

msg_Warn(p_access, "Close");

//    p_sys->p_thread->b_die = VLC_TRUE;
vlc_object_kill( p_sys->p_thread );
block_FifoWake( p_sys->p_thread->p_fifo_media );
block_FifoWake( p_sys->p_thread->p_empty_blocks );
    for( i = 0; i < 5; i++ )
    {
        block_t *p_dummy = block_New( p_access, 256 );
        p_dummy->i_dts = 0;
        p_dummy->i_pts = 0;
        p_dummy->i_length = 0;
        memset( p_dummy->p_buffer, 0, p_dummy->i_buffer );
        block_FifoPut( p_sys->p_thread->p_fifo_media, p_dummy );
    }
    for( i = 0; i < 5; i++ )
    {
        block_t *p_dummy = block_New( p_access, 256 );
        p_dummy->i_dts = 0;
        p_dummy->i_pts = 0;
        p_dummy->i_length = 0;
        memset( p_dummy->p_buffer, 0, p_dummy->i_buffer );
        block_FifoPut( p_sys->p_thread->p_empty_blocks, p_dummy );
    }
    vlc_thread_join( p_sys->p_thread );

    vlc_cond_destroy( &p_sys->p_thread->wait );
    vlc_mutex_destroy( &p_sys->p_thread->lock );

    block_FifoRelease( p_sys->p_thread->p_fifo_media );
    block_FifoRelease( p_sys->p_thread->p_empty_blocks );

    net_Close( p_sys->fd );

    var_Destroy( p_access, "rtmp-caching" );

    vlc_object_detach( p_sys->p_thread );

    vlc_UrlClean( &p_sys->url );
    free( p_sys->psz_application );
    free( p_sys->psz_media );
    free( p_sys );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_len_tmp;

    if( p_sys->fd < 0 )
    {
        p_access->info.b_eof = VLC_TRUE;
        return 0;
    }

    i_len_tmp = 0;

    while( i_len_tmp < i_len )
    {
        if( p_sys->read_packet )
        {
            if( !p_sys->p_thread->metadata_received )
            {
                p_sys->flv_packet = flv_get_metadata( p_access );

                p_sys->p_thread->metadata_received = 1;
            }
            else
            {
                if( p_sys->active && block_FifoCount( p_sys->p_thread->p_fifo_media ) == 0 )
                {
                    p_access->info.b_eof = VLC_TRUE;
                    break;
                }

                p_sys->flv_packet = block_FifoGet( p_sys->p_thread->p_fifo_media );
                if( p_sys->flv_packet == NULL )
                    continue; /* Forced wake-up */
            }

            if( p_sys->p_thread->first_media_packet )
            {
                p_sys->flv_packet = flv_insert_header( p_access, p_sys->flv_packet );

                p_sys->p_thread->first_media_packet = 0;
            }
        }
        if( i_len - i_len_tmp >= p_sys->flv_packet->i_buffer )
        {
            p_sys->read_packet = 1;

            memcpy( p_buffer + i_len_tmp, p_sys->flv_packet->p_buffer, p_sys->flv_packet->i_buffer );
            block_FifoPut( p_sys->p_thread->p_empty_blocks, p_sys->flv_packet );

            i_len_tmp += p_sys->flv_packet->i_buffer;
        }
        else
        {
            p_sys->read_packet = 0;

            memcpy( p_buffer + i_len_tmp, p_sys->flv_packet->p_buffer, i_len - i_len_tmp);
            p_sys->flv_packet->i_buffer -= i_len - i_len_tmp;
            memmove( p_sys->flv_packet->p_buffer, p_sys->flv_packet->p_buffer + i_len - i_len_tmp, p_sys->flv_packet->i_buffer );

            i_len_tmp += i_len - i_len_tmp;
        }
    }
/*int i;
for(i = 0; i < i_len_tmp; i += 16)
{
    msg_Warn(p_access,"%.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x",
p_buffer[i], p_buffer[i+1], p_buffer[i+2], p_buffer[i+3], p_buffer[i+4], p_buffer[i+5], p_buffer[i+6], p_buffer[i+7],
p_buffer[i+8], p_buffer[i+9], p_buffer[i+10], p_buffer[i+11], p_buffer[i+12], p_buffer[i+13], p_buffer[i+14], p_buffer[i+15]);
}*/
    if( i_len_tmp > 0 ) {
        if( p_sys->p_thread->result_publish )
        {
            /* Send publish onStatus event only once */
            p_sys->p_thread->result_publish = 0;

            rtmp_send_publish_start( p_access );
        }

        p_access->info.i_pos += i_len_tmp;

        rtmp_send_bytes_read( p_access, p_access->info.i_pos );
    }

    return i_len_tmp;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
/*msg_Warn ( p_access, "Seek to %lld", i_pos);
    switch( rtmp_seek( p_access, i_pos ) )
    {
        case -1:
            return VLC_EGENERIC;
        case 0:
            break;
        default:
            msg_Err( p_access, "You should not be here" );
            abort();
    }
*/
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE; /* TODO */
            break;

        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE; /* TODO */
            break;

        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, "rtmp-caching" ) * I64C(1000);
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_META:
        case ACCESS_GET_CONTENT_TYPE: /* DOWN: comment this line */
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ThreadControl: manage control messages and pipe media to Read
 *****************************************************************************/
static void ThreadControl( vlc_object_t *p_this )
{
    rtmp_control_thread_t *p_thread = (rtmp_control_thread_t *) p_this;
    rtmp_packet_t *rtmp_packet;

    rtmp_init_handler( p_thread->rtmp_handler );

    while( !p_thread->b_die )
    {

        rtmp_packet = rtmp_read_net_packet( p_thread );
        if( rtmp_packet != NULL )
        {
            if( rtmp_packet->content_type < 0x01 /* RTMP_CONTENT_TYPE_CHUNK_SIZE */
                || rtmp_packet->content_type > 0x14 ) /* RTMP_CONTENT_TYPE_INVOKE */
                msg_Warn( p_thread, "unknown content type received" );
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
}
