/*****************************************************************************
 * rtmp_amf_flv.c: RTMP, AMF and FLV over RTMP implementation.
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
 * RTMP header:
 ******************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>

#include <vlc_network.h> /* DOWN: #include <network.h> */
#include <vlc_url.h>
#include <vlc_block.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "rtmp_amf_flv.h"

/* header length (including itself) */
const uint8_t RTMP_HEADER_SIZE_MASK = 0xC0;
const uint8_t RTMP_HEADER_SIZE_12 = 0x00; 
const uint8_t RTMP_HEADER_SIZE_8 = 0x40;
const uint8_t RTMP_HEADER_SIZE_4 = 0x80;
const uint8_t RTMP_HEADER_SIZE_1 = 0xC0;

/* streams */
const uint8_t RTMP_HEADER_STREAM_MAX = 64;
const uint8_t RTMP_HEADER_STREAM_INDEX_MASK = 0x3F;

/* handshake */
const uint8_t RTMP_HANDSHAKE = 0x03;
const uint16_t RTMP_HANDSHAKE_BODY_SIZE = 1536;

/* content types */
const uint8_t RTMP_CONTENT_TYPE_CHUNK_SIZE = 0x01;
const uint8_t RTMP_CONTENT_TYPE_UNKNOWN_02 = 0x02;
const uint8_t RTMP_CONTENT_TYPE_BYTES_READ = 0x03;
const uint8_t RTMP_CONTENT_TYPE_PING = 0x04;
const uint8_t RTMP_CONTENT_TYPE_SERVER_BW = 0x05;
const uint8_t RTMP_CONTENT_TYPE_CLIENT_BW = 0x06;
const uint8_t RTMP_CONTENT_TYPE_UNKNOWN_07 = 0x07;
const uint8_t RTMP_CONTENT_TYPE_AUDIO_DATA = 0x08;
const uint8_t RTMP_CONTENT_TYPE_VIDEO_DATA = 0x09;
const uint8_t RTMP_CONTENT_TYPE_UNKNOWN_0A_0E = 0x0A;
const uint8_t RTMP_CONTENT_TYPE_FLEX_STREAM = 0x0F;
const uint8_t RTMP_CONTENT_TYPE_FLEX_SHARED_OBJECT = 0x10;
const uint8_t RTMP_CONTENT_TYPE_MESSAGE = 0x11;
const uint8_t RTMP_CONTENT_TYPE_NOTIFY = 0x12;
const uint8_t RTMP_CONTENT_TYPE_SHARED_OBJECT = 0x13;
const uint8_t RTMP_CONTENT_TYPE_INVOKE = 0x14;

/* shared object datatypes */
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_CONNECT = 0x01;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_DISCONNECT = 0x02;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_SET_ATTRIBUTE = 0x03;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_UPDATE_DATA = 0x04;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_UPDATE_ATTRIBUTE = 0x05;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_SEND_MESSAGE = 0x06;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_STATUS = 0x07;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_CLEAR_DATA = 0x08;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_DELETE_DATA = 0x09;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_DELETE_ATTRIBUTE = 0x0A;
const uint8_t RTMP_SHARED_OBJECT_DATATYPE_INITIAL_DATA = 0x0B;

/* pings */
const uint16_t RTMP_PING_CLEAR_STREAM = 0x0000;
const uint16_t RTMP_PING_CLEAR_PLAYING_BUFFER = 0x0001;
const uint16_t RTMP_PING_BUFFER_TIME_CLIENT = 0x0003;
const uint16_t RTMP_PING_RESET_STREAM = 0x0004;
const uint16_t RTMP_PING_CLIENT_FROM_SERVER = 0x0006;
const uint16_t RTMP_PING_PONG_FROM_CLIENT = 0x0007;

/* pings sizes */
const uint8_t RTMP_PING_SIZE_CLEAR_STREAM = 6;
const uint8_t RTMP_PING_SIZE_CLEAR_PLAYING_BUFFER = 6;
const uint8_t RTMP_PING_SIZE_BUFFER_TIME_CLIENT = 10;
const uint8_t RTMP_PING_SIZE_RESET_STREAM = 6;
/*const uint8_t RTMP_PING_SIZE_CLIENT_FROM_SERVER = 0x0006; TODO
const uint8_t RTMP_PING_SIZE_PONG_FROM_CLIENT = 0x0007;
*/

/* default values */
const uint8_t RTMP_DEFAULT_STREAM_INDEX_CONTROL = 0x02;
const uint8_t RTMP_DEFAULT_STREAM_INDEX_INVOKE = 0x03;
const uint8_t RTMP_DEFAULT_STREAM_INDEX_NOTIFY = 0x04;
const uint8_t RTMP_DEFAULT_STREAM_INDEX_VIDEO_DATA = 0x05;
const uint8_t RTMP_DEFAULT_STREAM_INDEX_AUDIO_DATA = 0x06;
const uint32_t RTMP_DEFAULT_CHUNK_SIZE = 128;
const double RTMP_DEFAULT_STREAM_CLIENT_ID = 1.0;
const double RTMP_DEFAULT_STREAM_SERVER_ID = 1.0;

/* misc */
const uint16_t MAX_EMPTY_BLOCKS = 200; /* empty blocks in fifo for media*/
const uint16_t RTMP_BODY_SIZE_ALLOC = 1024;
const uint32_t RTMP_TIME_CLIENT_BUFFER = 2000; /* miliseconds */
const uint32_t RTMP_SERVER_BW = 0x00000200;
const uint32_t RTMP_SRC_DST_CONNECT_OBJECT = 0x00000000;
const uint32_t RTMP_SRC_DST_CONNECT_OBJECT2 = 0x00000001;
const uint32_t RTMP_SRC_DST_DEFAULT = 0x01000000;
const uint64_t RTMP_AUDIOCODECS = 0x4083380000000000;
const uint64_t RTMP_VIDEOCODECS = 0x405f000000000000;
const uint64_t RTMP_VIDEOFUNCTION = 0x3ff0000000000000;
/*****************************************************************************
 * AMF header:
 ******************************************************************************/

/* boolean constants */
const uint8_t AMF_BOOLEAN_FALSE = 0x00;
const uint8_t AMF_BOOLEAN_TRUE = 0x01;

/* datatypes */
const uint8_t AMF_DATATYPE_NUMBER = 0x00;
const uint8_t AMF_DATATYPE_BOOLEAN = 0x01;
const uint8_t AMF_DATATYPE_STRING = 0x02;
const uint8_t AMF_DATATYPE_OBJECT = 0x03;
const uint8_t AMF_DATATYPE_MOVIE_CLIP = 0x04;
const uint8_t AMF_DATATYPE_NULL = 0x05;
const uint8_t AMF_DATATYPE_UNDEFINED = 0x06;
const uint8_t AMF_DATATYPE_REFERENCE = 0x07;
const uint8_t AMF_DATATYPE_MIXED_ARRAY = 0x08;
const uint8_t AMF_DATATYPE_END_OF_OBJECT = 0x09;
const uint8_t AMF_DATATYPE_ARRAY = 0x0A;
const uint8_t AMF_DATATYPE_DATE = 0x0B;
const uint8_t AMF_DATATYPE_LONG_STRING = 0x0C;
const uint8_t AMF_DATATYPE_UNSUPPORTED = 0x0D;
const uint8_t AMF_DATATYPE_RECORDSET = 0x0E;
const uint8_t AMF_DATATYPE_XML = 0x0F;
const uint8_t AMF_DATATYPE_TYPED_OBJECT = 0x10;
const uint8_t AMF_DATATYPE_AMF3_DATA = 0x11;

/* datatypes sizes */
const uint8_t AMF_DATATYPE_SIZE_NUMBER = 9;
const uint8_t AMF_DATATYPE_SIZE_BOOLEAN = 2;
const uint8_t AMF_DATATYPE_SIZE_STRING = 3;
const uint8_t AMF_DATATYPE_SIZE_OBJECT = 1;
const uint8_t AMF_DATATYPE_SIZE_NULL = 1;
const uint8_t AMF_DATATYPE_SIZE_OBJECT_VARIABLE = 2;
const uint8_t AMF_DATATYPE_SIZE_MIXED_ARRAY = 5;
const uint8_t AMF_DATATYPE_SIZE_END_OF_OBJECT = 3;

/* amf remote calls */
const uint64_t AMF_CALL_NETCONNECTION_CONNECT = 0x3FF0000000000000;
const uint64_t AMF_CALL_NETCONNECTION_CONNECT_AUDIOCODECS = 0x4083380000000000;
const uint64_t AMF_CALL_NETCONNECTION_CONNECT_VIDEOCODECS = 0x405F000000000000;
const uint64_t AMF_CALL_NETCONNECTION_CONNECT_VIDEOFUNCTION = 0x3FF0000000000000;
const uint64_t AMF_CALL_NETCONNECTION_CONNECT_OBJECTENCODING = 0x0;
const double AMF_CALL_STREAM_CLIENT_NUMBER = 3.0;
const double AMF_CALL_ONBWDONE = 2.0; 
const uint64_t AMF_CALL_NETSTREAM_PLAY = 0x0;

/*****************************************************************************
 * FLV header:
 ******************************************************************************/
const uint8_t FLV_HEADER_SIGNATURE[3] = { 0x46, 0x4C, 0x56 }; /* always "FLV" */
const uint8_t FLV_HEADER_VERSION = 0x01;
const uint8_t FLV_HEADER_AUDIO = 0x04;
const uint8_t FLV_HEADER_VIDEO = 0x01;
const uint32_t FLV_HEADER_SIZE = 0x00000009; /* always 9 for known FLV files */

const uint32_t FLV_TAG_FIRST_PREVIOUS_TAG_SIZE = 0x00000000;
const uint8_t FLV_TAG_PREVIOUS_TAG_SIZE = 4;
const uint8_t FLV_TAG_SIZE = 11;

/* audio stereo types */
const uint8_t FLV_AUDIO_STEREO_MASK = 0x01;
const uint8_t FLV_AUDIO_STEREO_MONO = 0x00;
const uint8_t FLV_AUDIO_STEREO_STEREO = 0x01;

/* audio size */
const uint8_t FLV_AUDIO_SIZE_MASK = 0x02;
const uint8_t FLV_AUDIO_SIZE_8_BIT = 0x00;
const uint8_t FLV_AUDIO_SIZE_16_BIT = 0x02;

/* audio rate */
const uint8_t FLV_AUDIO_RATE_MASK = 0x0C;
const uint8_t FLV_AUDIO_RATE_5_5_KHZ = 0x00;
const uint8_t FLV_AUDIO_RATE_11_KHZ = 0x04;
const uint8_t FLV_AUDIO_RATE_22_KHZ = 0x08;
const uint8_t FLV_AUDIO_RATE_44_KHZ = 0x0C;

/* audio codec types */
const uint8_t FLV_AUDIO_CODEC_ID_MASK = 0xF0;
const uint8_t FLV_AUDIO_CODEC_ID_UNCOMPRESSED = 0x00;
const uint8_t FLV_AUDIO_CODEC_ID_ADPCM = 0x10;
const uint8_t FLV_AUDIO_CODEC_ID_MP3 = 0x20;
const uint8_t FLV_AUDIO_CODEC_ID_NELLYMOSER_8KHZ_MONO = 0x50;
const uint8_t FLV_AUDIO_CODEC_ID_NELLYMOSER = 0x60;

/* video codec types */
const uint8_t FLV_VIDEO_CODEC_ID_MASK = 0x0F;
const uint8_t FLV_VIDEO_CODEC_ID_SORENSEN_H263 = 0x02;
const uint8_t FLV_VIDEO_CODEC_ID_SCREEN_VIDEO = 0x03;
const uint8_t FLV_VIDEO_CODEC_ID_ON2_VP6 = 0x04;
const uint8_t FLV_VIDEO_CODEC_ID_ON2_VP6_ALPHA = 0x05;
const uint8_t FLV_VIDEO_CODEC_ID_SCREEN_VIDEO_2 = 0x06;

/* video frame types */
const uint8_t FLV_VIDEO_FRAME_TYPE_MASK = 0xF0;
const uint8_t FLV_VIDEO_FRAME_TYPE_KEYFRAME = 0x10;
const uint8_t FLV_VIDEO_FRAME_TYPE_INTER_FRAME = 0x20;
const uint8_t FLV_VIDEO_FRAME_TYPE_DISPOSABLE_INTER_FRAME = 0x30;

/*****************************************************************************
 * static RTMP functions:
 ******************************************************************************/
static void rtmp_handler_null       ( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet );
static void rtmp_handler_chunk_size ( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet );
static void rtmp_handler_invoke     ( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet );
static void rtmp_handler_audio_data ( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet );
static void rtmp_handler_video_data ( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet );
static void rtmp_handler_notify     ( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet );

static rtmp_packet_t *rtmp_new_packet( rtmp_control_thread_t *p_thread, uint8_t stream_index, uint32_t timestamp, uint8_t content_type, uint32_t src_dst, rtmp_body_t *body );
static block_t *rtmp_new_block( rtmp_control_thread_t *p_thread, uint8_t *buffer, int32_t length_buffer );

static rtmp_packet_t *rtmp_encode_onBWDone( rtmp_control_thread_t *p_thread, double number );
static rtmp_packet_t *rtmp_encode_server_bw( rtmp_control_thread_t *p_thread, uint32_t number );
static rtmp_packet_t *rtmp_encode_NetConnection_connect_result( rtmp_control_thread_t *p_thread, double number );
static rtmp_packet_t *rtmp_encode_createStream_result( rtmp_control_thread_t *p_thread, double stream_client, double stream_server );
static rtmp_packet_t *rtmp_encode_ping_reset_stream( rtmp_control_thread_t *p_thread );
static rtmp_packet_t *rtmp_encode_ping_clear_stream( rtmp_control_thread_t *p_thread, uint32_t src_dst );
static rtmp_packet_t *rtmp_encode_NetStream_play_reset_onStatus( rtmp_control_thread_t *p_thread, char *psz_media );
static rtmp_packet_t *rtmp_encode_NetStream_play_start_onStatus( rtmp_control_thread_t *p_thread, char *psz_media );
static uint8_t rtmp_encode_header_size( vlc_object_t *p_this, uint8_t header_size );
static uint8_t rtmp_decode_header_size( vlc_object_t *p_this, uint8_t header_size );
static uint8_t rtmp_get_stream_index( uint8_t content_type );

static void rtmp_body_append( rtmp_body_t *rtmp_body, uint8_t *buffer, uint32_t length );

static uint8_t *rtmp_encode_ping( uint16_t type, uint32_t src_dst, uint32_t third_arg, uint32_t fourth_arg );

/*****************************************************************************
 * static AMF functions:
 ******************************************************************************/
static uint8_t *amf_encode_element( uint8_t element, const void *value );
static uint8_t *amf_encode_object_variable( const char *key, uint8_t element, const void *value );
static double amf_decode_number( uint8_t **buffer );
static int amf_decode_boolean( uint8_t **buffer );
static char *amf_decode_string( uint8_t **buffer );
static char *amf_decode_object( uint8_t **buffer );

/*****************************************************************************
 * static FLV functions:
 ******************************************************************************/
static void flv_rebuild( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet );
static void flv_get_metadata_audio( rtmp_control_thread_t *p_thread, rtmp_packet_t *packet_audio, uint8_t *stereo, uint8_t *audiosamplesize, uint32_t *audiosamplerate, uint8_t *audiocodecid );
static void flv_get_metadata_video( rtmp_control_thread_t *p_thread, rtmp_packet_t *packet_video, uint8_t *videocodecid, uint8_t *frametype );
static rtmp_packet_t *flv_build_onMetaData( access_t *p_access, uint64_t duration, uint8_t stereo, uint8_t audiosamplesize, uint32_t audiosamplerate, uint8_t audiocodecid, uint8_t videocodecid );

/*****************************************************************************
 * RTMP implementation:
 ******************************************************************************/
int
rtmp_handshake_passive( vlc_object_t *p_this, int fd )
{
    uint8_t p_read[RTMP_HANDSHAKE_BODY_SIZE + 1];
    uint8_t p_write[RTMP_HANDSHAKE_BODY_SIZE * 2 + 1];
    ssize_t i_ret;
    int i;

    /* Receive handshake */
    i_ret = net_Read( p_this, fd, NULL, p_read, RTMP_HANDSHAKE_BODY_SIZE + 1, true );
    if( i_ret != RTMP_HANDSHAKE_BODY_SIZE + 1 )
    {
        msg_Err( p_this, "failed to receive handshake" );
        return -1;
    }

    /* Check handshake */
    if ( p_read[0] != RTMP_HANDSHAKE )
    {
        msg_Err( p_this, "first byte in handshake corrupt" );
        return -1;
    }

    /* Answer handshake */
    p_write[0] = RTMP_HANDSHAKE;
    memset( p_write + 1, 0, RTMP_HANDSHAKE_BODY_SIZE );
    memcpy( p_write + 1 + RTMP_HANDSHAKE_BODY_SIZE, p_read + 1, RTMP_HANDSHAKE_BODY_SIZE );

    /* Send handshake*/
    i_ret = net_Write( p_this, fd, NULL, p_write, RTMP_HANDSHAKE_BODY_SIZE * 2 + 1 );
    if( i_ret != RTMP_HANDSHAKE_BODY_SIZE * 2 + 1 )
    {
        msg_Err( p_this, "failed to send handshake" );
        return -1;
    }

    /* Receive acknowledge */
    i_ret = net_Read( p_this, fd, NULL, p_read, RTMP_HANDSHAKE_BODY_SIZE, true );
    if( i_ret != RTMP_HANDSHAKE_BODY_SIZE )
    {
        msg_Err( p_this, "failed to receive acknowledge" );
        return -1;
    }

    /* Check acknowledge */
    for(i = 8; i < RTMP_HANDSHAKE_BODY_SIZE; i++ )
        if( p_write[i + 1] != p_read[i] )
        {
            msg_Err( p_this, "body acknowledge received corrupt" );
            return -1;
        }

    return 0;
}

int
rtmp_handshake_active( vlc_object_t *p_this, int fd )
{
    uint8_t p_read[RTMP_HANDSHAKE_BODY_SIZE * 2 + 1];
    uint8_t p_write[RTMP_HANDSHAKE_BODY_SIZE + 1];
    ssize_t i_ret;
    int i;

    p_write[0] = RTMP_HANDSHAKE;
    for( i = 0; i < RTMP_HANDSHAKE_BODY_SIZE; i++ )
        p_write[i + 1] = i & 0xFF;

    /* Send handshake*/
    i_ret = net_Write( p_this, fd, NULL, p_write, RTMP_HANDSHAKE_BODY_SIZE + 1 );
    if( i_ret != RTMP_HANDSHAKE_BODY_SIZE + 1 )
    {
        msg_Err( p_this, "failed to send handshake" );
        return -1;
    }

    /* Receive handshake */
    i_ret = net_Read( p_this, fd, NULL, p_read, RTMP_HANDSHAKE_BODY_SIZE * 2 + 1, true );
    if( i_ret != RTMP_HANDSHAKE_BODY_SIZE * 2 + 1 )
    {
        msg_Err( p_this, "failed to receive handshake" );
        return -1;
    }

    /* Check handshake */
    if( p_read[0] != RTMP_HANDSHAKE )
    {
        msg_Err( p_this, "first byte in handshake received corrupt" );
        return -1;
    }

    for(i = 8; i < RTMP_HANDSHAKE_BODY_SIZE; i++ )
        if( p_write[i + 1] != p_read[i + 1 + RTMP_HANDSHAKE_BODY_SIZE] )
        {
            msg_Err( p_this, "body handshake received corrupt" );
            return -1;
        }

    /* Acknowledge handshake */
    i_ret = net_Write( p_this, fd, NULL, p_read + 1, RTMP_HANDSHAKE_BODY_SIZE );
    if( i_ret != RTMP_HANDSHAKE_BODY_SIZE )
    {
        msg_Err( p_this, "failed to acknowledge handshake" );
        return -1;
    }

    return 0;
}

int
rtmp_connect_active( rtmp_control_thread_t *p_thread )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;
    char *tmp_url;
    ssize_t i_ret;

    /* Build NetConnection.connect call */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "connect" );
    rtmp_body_append( rtmp_body, tmp_buffer, 
        AMF_DATATYPE_SIZE_STRING + strlen( "connect" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER,
        &AMF_CALL_NETCONNECTION_CONNECT );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_OBJECT );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "app",
        AMF_DATATYPE_STRING, p_thread->psz_application );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "app" ) + 
        AMF_DATATYPE_SIZE_STRING + strlen( p_thread->psz_application ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "flashVer",
        AMF_DATATYPE_STRING, "LNX 9,0,48,0" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "flashVer" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "LNX 9,0,48,0" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "swfUrl",
         AMF_DATATYPE_STRING, "file:///mac.flv" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "swfUrl" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "file:///mac.flv" ) );
    free( tmp_buffer );

    tmp_url = (char *) malloc( strlen( "rtmp://") + strlen( p_thread->url.psz_buffer ) + 1 );
    if( !tmp_url )
    {
        free( rtmp_body->body );
        free( rtmp_body );
        return -1;
    }
    sprintf( tmp_url, "rtmp://%s", p_thread->url.psz_buffer );
    tmp_buffer = amf_encode_object_variable( "tcUrl",
        AMF_DATATYPE_STRING, tmp_url );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "tcUrl" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( tmp_url ) );
    free( tmp_url );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "fpad",
        AMF_DATATYPE_BOOLEAN, &AMF_BOOLEAN_FALSE );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "fpad" ) +
        AMF_DATATYPE_SIZE_BOOLEAN );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "audioCodecs",
        AMF_DATATYPE_NUMBER, &AMF_CALL_NETCONNECTION_CONNECT_AUDIOCODECS );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "audioCodecs" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "videoCodecs",
        AMF_DATATYPE_NUMBER, &AMF_CALL_NETCONNECTION_CONNECT_VIDEOCODECS );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "videoCodecs" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "videoFunction",
        AMF_DATATYPE_NUMBER, &AMF_CALL_NETCONNECTION_CONNECT_VIDEOFUNCTION );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "videoFunction" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "pageUrl",
        AMF_DATATYPE_STRING, "file:///mac.html" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "pageUrl" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "file:///mac.html" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "objectEncoding",
        AMF_DATATYPE_NUMBER, &AMF_CALL_NETCONNECTION_CONNECT_OBJECTENCODING );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "objectEncoding" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element ( AMF_DATATYPE_END_OF_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_END_OF_OBJECT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_CONTENT_TYPE_INVOKE, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    tmp_buffer = rtmp_encode_packet( p_thread, rtmp_packet );

    /* Call NetConnection.connect */
    i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, rtmp_packet->length_encoded );
    if( i_ret != rtmp_packet->length_encoded )
    {
        free( rtmp_packet->body->body );
        free( rtmp_packet->body );
        free( rtmp_packet );
        free( tmp_buffer );
        msg_Err( p_thread, "failed send call NetConnection.connect" );
        return -1;
    }
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
    free( tmp_buffer );

    /* Wait for NetConnection.connect result */
    vlc_mutex_lock( &p_thread->lock );
    vlc_cond_wait( &p_thread->wait, &p_thread->lock );
    vlc_mutex_unlock( &p_thread->lock );

    if( p_thread->result_connect )
    {
        msg_Err( p_thread, "failed call NetConnection.connect" );
        return -1;
    }

    /* Force control thread to stop if receive NetStream.play call and wait is not ready */
    vlc_mutex_lock( &p_thread->lock );

    /* Build NetStream.createStream call */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "createStream" );
    rtmp_body_append( rtmp_body, tmp_buffer, 
        AMF_DATATYPE_SIZE_STRING + strlen( "createStream" ) );
    free( tmp_buffer );

    p_thread->stream_client_id = RTMP_DEFAULT_STREAM_CLIENT_ID;

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER,
        &AMF_CALL_STREAM_CLIENT_NUMBER );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE, 
        0, RTMP_CONTENT_TYPE_INVOKE, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    tmp_buffer = rtmp_encode_packet( p_thread, rtmp_packet );

    /* Call NetStream.createStream */
    i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, rtmp_packet->length_encoded );
    if( i_ret != rtmp_packet->length_encoded )
    {
        free( rtmp_packet->body->body );
        free( rtmp_packet->body );
        free( rtmp_packet );
        free( tmp_buffer );
        msg_Err( p_thread, "failed send call NetStream.createStream" );
        return -1;
    }
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
    free( tmp_buffer );
/*TODO: read server stream number*/
    /* Build ping packet */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = rtmp_encode_ping( RTMP_PING_BUFFER_TIME_CLIENT, RTMP_SRC_DST_CONNECT_OBJECT, RTMP_TIME_CLIENT_BUFFER, 0 );
    rtmp_body_append( rtmp_body, tmp_buffer, RTMP_PING_SIZE_BUFFER_TIME_CLIENT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_CONTROL,
        0, RTMP_CONTENT_TYPE_PING, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    tmp_buffer = rtmp_encode_packet( p_thread, rtmp_packet );

    /* Send ping packet */
    i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, rtmp_packet->length_encoded );
    if( i_ret != rtmp_packet->length_encoded )
    {
        free( rtmp_packet->body->body );
        free( rtmp_packet->body );
        free( rtmp_packet );
        free( tmp_buffer );
        msg_Err( p_thread, "failed send ping BUFFER_TIME_CLIENT" );
        return -1;
    }
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
    free( tmp_buffer );

    /* Build NetStream.play call */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "play" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "play" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER,
        &AMF_CALL_NETSTREAM_PLAY );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, p_thread->psz_media );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( p_thread->psz_media ) );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_CONTENT_TYPE_INVOKE, RTMP_SRC_DST_DEFAULT, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    tmp_buffer = rtmp_encode_packet( p_thread, rtmp_packet );

    /* Call NetStream.play */
    i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, rtmp_packet->length_encoded );
    if( i_ret != rtmp_packet->length_encoded )
    {
        free( rtmp_packet->body->body );
        free( rtmp_packet->body );
        free( rtmp_packet );
        free( tmp_buffer );
        msg_Err( p_thread, "failed send call NetStream.play" );
        return -1;
    }
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
    free( tmp_buffer );

    /* Build ping packet */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = rtmp_encode_ping( RTMP_PING_BUFFER_TIME_CLIENT, RTMP_SRC_DST_CONNECT_OBJECT2, RTMP_TIME_CLIENT_BUFFER, 0 );
    rtmp_body_append( rtmp_body, tmp_buffer, RTMP_PING_SIZE_BUFFER_TIME_CLIENT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_CONTROL,
        0, RTMP_CONTENT_TYPE_PING, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    tmp_buffer = rtmp_encode_packet( p_thread, rtmp_packet );

    /* Send ping packet */
    i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, rtmp_packet->length_encoded );
    if( i_ret != rtmp_packet->length_encoded )
    {
        free( rtmp_packet->body->body );
        free( rtmp_packet->body );
        free( rtmp_packet );
        free( tmp_buffer );
        msg_Err( p_thread, "failed send ping BUFFER_TIME_CLIENT" );
        return -1;
    }
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
    free( tmp_buffer );

    /* Wait for NetStream.play.start result */
    vlc_cond_wait( &p_thread->wait, &p_thread->lock );
    vlc_mutex_unlock( &p_thread->lock );

    if( p_thread->result_play )
    {
        msg_Err( p_thread, "failed call NetStream.play" );
        return -1;
    }

    /* Next packet is the beginning of flv stream */
    msg_Dbg( p_thread, "next packet is the beginning of flv stream" );

    return 0;
}

int
rtmp_connect_passive( rtmp_control_thread_t *p_thread )
{
    /* Force control thread to stop if receive NetStream.play call and wait is not ready */
    vlc_mutex_lock( &p_thread->lock );

    /* Wait for NetStream.play.start result */
    vlc_cond_wait( &p_thread->wait, &p_thread->lock );
    vlc_mutex_unlock( &p_thread->lock );

    if( p_thread->result_play )
    {
        msg_Err( p_thread, "failed call NetStream.play" );
        return -1;
    }

    return 0;
}

/* TODO
int
rtmp_seek( access_t *p_access, int64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;
    uint64_t tmp_number;
    ssize_t i_ret;
msg_Warn(p_access, "i_pos %lld", i_pos);
    // Build NetStream.seek call //
    rtmp_body = rtmp_body_new();

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "seek" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "seek" ) );
    free( tmp_buffer );

    tmp_number = 0;
    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER,
        &tmp_number );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );
//TODO: convert i_pos to double and see if they are milliseconds
    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER,
        &i_pos );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_sys->p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_DATATYPE_INVOKE, RTMP_SRC_DST_DEFAULT, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    tmp_buffer = rtmp_encode_packet( p_access, rtmp_packet ); 

    // Call NetStream.seek //
    i_ret = net_Write( p_access, p_sys->fd, NULL, tmp_buffer, rtmp_packet->length_encoded );
    if( i_ret < rtmp_packet->length_encoded )
    {
        free( rtmp_packet->body->body );
        free( rtmp_packet->body );
        free( rtmp_packet );
        free( tmp_buffer );
        msg_Err( p_access, "failed call NetStream.seek" );
        return -1;
    }
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
    free( tmp_buffer );

    // Receive TODO: see what //
    rtmp_packet = rtmp_read_net_packet( p_access, p_sys->fd );
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );

    return 0;
}
*/

rtmp_packet_t *
rtmp_build_bytes_read( rtmp_control_thread_t *p_thread, uint32_t reply )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;

    /* Build bytes read packet */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = (uint8_t *) malloc( sizeof( uint32_t ) * sizeof( uint8_t ) );
    if( !tmp_buffer ) return NULL;

    reply = hton32( reply );
    memcpy( tmp_buffer, &reply, sizeof( uint32_t ) );

    rtmp_body_append( rtmp_body, tmp_buffer, sizeof( uint32_t ) );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_CONTROL,
        0, RTMP_CONTENT_TYPE_BYTES_READ, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

rtmp_packet_t *
rtmp_build_publish_start( rtmp_control_thread_t *p_thread )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;

    /* Build publish start event */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "onStatus" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "onStatus" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER,
        &p_thread->stream_server_id );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_OBJECT );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "level",
        AMF_DATATYPE_STRING, "status" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "level" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "status" ) );
    free ( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "code",
        AMF_DATATYPE_STRING, "NetStream.Publish.Start" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "code" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "NetStream.Publish.Start" ) );
    free ( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "description",
        AMF_DATATYPE_STRING, "" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "description" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "" ) );
    free ( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "details",
        AMF_DATATYPE_STRING, p_thread->psz_publish );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "details" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( p_thread->psz_publish ) );
    free ( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "clientid",
        AMF_DATATYPE_NUMBER, &p_thread->stream_client_id );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "clientid" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element ( AMF_DATATYPE_END_OF_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_END_OF_OBJECT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_CONTENT_TYPE_INVOKE, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

rtmp_packet_t *
rtmp_build_flv_over_rtmp( rtmp_control_thread_t *p_thread, block_t *p_buffer )
{
    rtmp_packet_t *rtmp_packet;

    if( p_thread->flv_length_body > 0 )
    {
        p_thread->flv_length_body -= p_buffer->i_buffer;
        rtmp_body_append( p_thread->flv_body, p_buffer->p_buffer, p_buffer->i_buffer );

        if( p_thread->flv_length_body > 0 )
            return NULL;

    }
    else
    {
        p_thread->flv_content_type = *p_buffer->p_buffer;

        p_buffer->p_buffer[0] = 0;
        p_thread->flv_length_body = ntoh32( *(uint32_t *) (p_buffer->p_buffer) );

        p_buffer->p_buffer[3] = 0;
        p_thread->flv_timestamp = ntoh32( *(uint32_t *) (p_buffer->p_buffer + 3) );
    }

    if( p_thread->flv_length_body > p_buffer->i_buffer - FLV_TAG_SIZE - FLV_TAG_PREVIOUS_TAG_SIZE )
    {
        p_thread->flv_length_body -= p_buffer->i_buffer - FLV_TAG_SIZE - FLV_TAG_PREVIOUS_TAG_SIZE;
        rtmp_body_append( p_thread->flv_body, p_buffer->p_buffer + FLV_TAG_SIZE, p_buffer->i_buffer - FLV_TAG_SIZE );

        return NULL;
    }
    else
    {
        rtmp_body_append( p_thread->flv_body, p_buffer->p_buffer + FLV_TAG_SIZE, p_thread->flv_length_body );
    }

    rtmp_packet = rtmp_new_packet( p_thread, rtmp_get_stream_index( p_thread->flv_content_type ),
        p_thread->flv_timestamp, p_thread->flv_content_type, RTMP_SRC_DST_DEFAULT, p_thread->flv_body );
    p_thread->flv_length_body = 0;
    rtmp_body_reset( p_thread->flv_body );

    return rtmp_packet;
}

rtmp_packet_t *
rtmp_read_net_packet( rtmp_control_thread_t *p_thread )
{
    int length_header;
    int stream_index;
    int bytes_left;
    uint8_t p_read[12];
    rtmp_packet_t *rtmp_packet;
    ssize_t i_ret;


    for(;;)
    {
        i_ret = net_Read( p_thread, p_thread->fd, NULL, p_read, 1, true );
        if( i_ret != 1 )
            goto error;

        length_header = rtmp_decode_header_size( (vlc_object_t *) p_thread, p_read[0] & RTMP_HEADER_SIZE_MASK );
        stream_index = p_read[0] & RTMP_HEADER_STREAM_INDEX_MASK;

        i_ret = net_Read( p_thread, p_thread->fd, NULL, p_read + 1, length_header - 1, true );
        if( i_ret != length_header - 1 )
            goto error;

        /* Update timestamp if not is an interchunk packet */
        if( length_header == 1 && p_thread->rtmp_headers_recv[stream_index].body == NULL )
        {
            p_thread->rtmp_headers_recv[stream_index].timestamp +=
                p_thread->rtmp_headers_recv[stream_index].timestamp_relative;
        }

        /* Length 4 and 8 headers have relative timestamp */
        if( length_header == 4 || length_header == 8 )
        {
            p_read[0] = 0;

            p_thread->rtmp_headers_recv[stream_index].timestamp_relative = ntoh32( *(uint32_t *) p_read );
            p_thread->rtmp_headers_recv[stream_index].timestamp +=
                p_thread->rtmp_headers_recv[stream_index].timestamp_relative;
        }

        if( length_header >= 8 )
        {
            p_read[3] = 0;

            p_thread->rtmp_headers_recv[stream_index].length_body = ntoh32( *(uint32_t *) (p_read + 3) );
            p_thread->rtmp_headers_recv[stream_index].content_type = p_read[7];
        }

        /* Length 12 headers have absolute timestamp */
        if( length_header >= 12 )
        {
            p_read[0] = 0;

            p_thread->rtmp_headers_recv[stream_index].timestamp = ntoh32( *(uint32_t *) p_read );
            p_thread->rtmp_headers_recv[stream_index].src_dst = ntoh32( *(uint32_t *) (p_read + 8) );
        }

        if( p_thread->rtmp_headers_recv[stream_index].body == NULL )
        {
            p_thread->rtmp_headers_recv[stream_index].body =
                rtmp_body_new( p_thread->rtmp_headers_recv[stream_index].length_body );
        }

        bytes_left = p_thread->rtmp_headers_recv[stream_index].body->length_buffer -
            p_thread->rtmp_headers_recv[stream_index].body->length_body;

        if( bytes_left > p_thread->chunk_size_recv )
            bytes_left = p_thread->chunk_size_recv;

        i_ret = net_Read( p_thread, p_thread->fd, NULL,
            p_thread->rtmp_headers_recv[stream_index].body->body +
            p_thread->rtmp_headers_recv[stream_index].body->length_body,
            bytes_left, true );
        if( i_ret != bytes_left )
            goto error;

        p_thread->rtmp_headers_recv[stream_index].body->length_body += bytes_left;

        if( p_thread->rtmp_headers_recv[stream_index].length_body == p_thread->rtmp_headers_recv[stream_index].body->length_body )
        {
            rtmp_packet = (rtmp_packet_t *) malloc( sizeof( rtmp_packet_t ) );
            if( !rtmp_packet ) goto error;

            rtmp_packet->stream_index = stream_index;
            rtmp_packet->timestamp = p_thread->rtmp_headers_recv[stream_index].timestamp;
            rtmp_packet->timestamp_relative = p_thread->rtmp_headers_recv[stream_index].timestamp_relative;
            rtmp_packet->content_type = p_thread->rtmp_headers_recv[stream_index].content_type;
            rtmp_packet->src_dst = p_thread->rtmp_headers_recv[stream_index].src_dst;
            rtmp_packet->length_body = p_thread->rtmp_headers_recv[stream_index].length_body;
            rtmp_packet->body = p_thread->rtmp_headers_recv[stream_index].body;

            p_thread->rtmp_headers_recv[stream_index].body = NULL;

            return rtmp_packet;
        }
    }

error:
    msg_Err( p_thread, "rtmp_read_net_packet: net_Read error");
    return NULL;
}

void
rtmp_init_handler( rtmp_handler_t *rtmp_handler )
{
    rtmp_handler[RTMP_CONTENT_TYPE_CHUNK_SIZE] = rtmp_handler_chunk_size;
    rtmp_handler[RTMP_CONTENT_TYPE_UNKNOWN_02] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_BYTES_READ] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_PING] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_SERVER_BW] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_CLIENT_BW] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_UNKNOWN_07] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_AUDIO_DATA] = rtmp_handler_audio_data;
    rtmp_handler[RTMP_CONTENT_TYPE_VIDEO_DATA] = rtmp_handler_video_data;
    rtmp_handler[RTMP_CONTENT_TYPE_UNKNOWN_0A_0E] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_FLEX_STREAM] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_FLEX_SHARED_OBJECT] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_MESSAGE] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_NOTIFY] = rtmp_handler_notify;
    rtmp_handler[RTMP_CONTENT_TYPE_SHARED_OBJECT] = rtmp_handler_null;
    rtmp_handler[RTMP_CONTENT_TYPE_INVOKE] = rtmp_handler_invoke;
}

static void
rtmp_handler_null( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    VLC_UNUSED(p_thread);
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
}

static void
rtmp_handler_chunk_size( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    p_thread->chunk_size_recv = ntoh32( *(uint32_t *) (rtmp_packet->body->body) );

    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
}

static void
rtmp_handler_audio_data( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    block_t *p_buffer;

    if( !p_thread->has_audio )
    {
        p_thread->has_audio = 1;

        flv_get_metadata_audio( p_thread, rtmp_packet,
            &p_thread->metadata_stereo, &p_thread->metadata_samplesize,
            &p_thread->metadata_samplerate, &p_thread->metadata_audiocodecid );
    }

    flv_rebuild( p_thread, rtmp_packet );
    p_buffer = rtmp_new_block( p_thread, rtmp_packet->body->body, rtmp_packet->body->length_body );
    block_FifoPut( p_thread->p_fifo_input, p_buffer );

    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
}

static void
rtmp_handler_video_data( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    block_t *p_buffer;

    if( !p_thread->has_video )
    {
        p_thread->has_video = 1;

        flv_get_metadata_video( p_thread, rtmp_packet,
            &p_thread->metadata_videocodecid, &p_thread->metadata_frametype );
    }

    flv_rebuild( p_thread, rtmp_packet );
    p_buffer = rtmp_new_block( p_thread, rtmp_packet->body->body, rtmp_packet->body->length_body );
    block_FifoPut( p_thread->p_fifo_input, p_buffer );

    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
}

static void
rtmp_handler_notify( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    block_t *p_buffer;

    p_thread->metadata_received = 1;

    flv_rebuild( p_thread, rtmp_packet );
    p_buffer = rtmp_new_block( p_thread, rtmp_packet->body->body, rtmp_packet->body->length_body );
    block_FifoPut( p_thread->p_fifo_input, p_buffer );

    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );
}

static void
rtmp_handler_invoke( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    rtmp_packet_t *tmp_rtmp_packet;
    uint8_t *i, *end, *tmp_buffer;
    double number;
    char *string, *string2;
    ssize_t i_ret;

    i = rtmp_packet->body->body;
    end = rtmp_packet->body->body + rtmp_packet->body->length_body;

    i++; /* Pass over AMF_DATATYPE_STRING */
    string = amf_decode_string( &i );

    i++; /* Pass over AMF_DATATYPE_NUMBER */
    number = amf_decode_number( &i );

    msg_Dbg( p_thread, "%s %.1f", string, number );

    if( strcmp( "connect", string ) == 0 )
    {
        /* Connection bandwith */
        tmp_rtmp_packet = rtmp_encode_onBWDone( p_thread, AMF_CALL_ONBWDONE );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send connection bandwith" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );

        /* Server bandwith */
        tmp_rtmp_packet = rtmp_encode_server_bw( p_thread, RTMP_SERVER_BW );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send server bandwith" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );

        /* Clear stream */
        tmp_rtmp_packet = rtmp_encode_ping_clear_stream( p_thread, RTMP_SRC_DST_CONNECT_OBJECT );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send clear stream" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );

        /* Reply NetConnection.connect */
        tmp_rtmp_packet = rtmp_encode_NetConnection_connect_result( p_thread, number );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send reply NetConnection.connect" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );
    }
    else if( strcmp( "createStream", string ) == 0 )
    {
        p_thread->stream_client_id = number;
        p_thread->stream_server_id = RTMP_DEFAULT_STREAM_SERVER_ID;

        /* Reply createStream */
        tmp_rtmp_packet = rtmp_encode_createStream_result( p_thread, p_thread->stream_client_id, p_thread->stream_server_id );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send reply createStream" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );

        /* Reset stream */
        tmp_rtmp_packet = rtmp_encode_ping_reset_stream( p_thread );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send reset stream" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );

        /* Clear stream */
        tmp_rtmp_packet = rtmp_encode_ping_clear_stream( p_thread, RTMP_SRC_DST_CONNECT_OBJECT2 );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );
    
        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send clear stream" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );
    }
    else if( strcmp( "publish", string ) == 0 )
    {
        i++;
        msg_Dbg( p_thread, "null" );

        i++;
        string2 = amf_decode_string( &i );
        msg_Dbg( p_thread, "string: %s", string2 );

        p_thread->psz_publish = strdup( string2 );

        free( string2 );
    }
    else if( strcmp( "play", string ) == 0 )
    {
        i++;
        msg_Dbg( p_thread, "null" );

        i++;
        string2 = amf_decode_string( &i );
        msg_Dbg( p_thread, "string: %s", string2 );

        /* Reply NetStream.play.reset */
        tmp_rtmp_packet = rtmp_encode_NetStream_play_reset_onStatus( p_thread, string2 );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send reply NetStream.play.reset" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );

        /* Reply NetStream.play.start */
        tmp_rtmp_packet = rtmp_encode_NetStream_play_start_onStatus( p_thread, string2 );

        tmp_buffer = rtmp_encode_packet( p_thread, tmp_rtmp_packet );

        i_ret = net_Write( p_thread, p_thread->fd, NULL, tmp_buffer, tmp_rtmp_packet->length_encoded );
        if( i_ret != tmp_rtmp_packet->length_encoded )
        {
            msg_Err( p_thread, "failed send reply NetStream.play.start" );
            goto error;
        }
        free( tmp_rtmp_packet->body->body );
        free( tmp_rtmp_packet->body );
        free( tmp_rtmp_packet );
        free( tmp_buffer );

        free( string2 );

        p_thread->result_play = 0;

        vlc_mutex_lock( &p_thread->lock );
        vlc_cond_signal( &p_thread->wait );
        vlc_mutex_unlock( &p_thread->lock );
    }

    free( string );

    while( i < end )
    {
        if( *i == AMF_DATATYPE_NUMBER )
        {
            i++;
            msg_Dbg( p_thread, "number: %le", amf_decode_number( &i ) );
        }
        else if( *i == AMF_DATATYPE_BOOLEAN )
        {
            i++;
            msg_Dbg( p_thread, "boolean: %s", amf_decode_boolean( &i ) ? "true" : "false" );
        }
        else if( *i == AMF_DATATYPE_STRING )
        {
            i++;
            string = amf_decode_string( &i );
            msg_Dbg( p_thread, "string: %s", string );
            free( string );
        }
        else if( *i == AMF_DATATYPE_OBJECT )
        {
            i++;
            msg_Dbg( p_thread, "object" );
            while( ( string = amf_decode_object( &i ) ) != NULL )
            {
                if( *i == AMF_DATATYPE_NUMBER )
                {
                    i++;
                    msg_Dbg( p_thread, "key: %s value: %le", string, amf_decode_number( &i ) );
                }
                else if( *i == AMF_DATATYPE_BOOLEAN )
                {
                    i++;
                    msg_Dbg( p_thread, "key: %s value: %s", string, amf_decode_boolean( &i ) ? "true" : "false" );
                }
                else if( *i == AMF_DATATYPE_STRING )
                {
                    i++;
                    string2 = amf_decode_string( &i );
                    msg_Dbg( p_thread, "key: %s value: %s", string, string2 );
                    if( strcmp( "code", string ) == 0 )
                    {
                        if( strcmp( "NetConnection.Connect.Success", string2 ) == 0 )
                        {
                            p_thread->result_connect = 0;

                            vlc_mutex_lock( &p_thread->lock );
                            vlc_cond_signal( &p_thread->wait );
                            vlc_mutex_unlock( &p_thread->lock );
                        }
                        else if( strcmp( "NetConnection.Connect.InvalidApp", string2 ) == 0 )
                        {
                            p_thread->b_die = 1; 

                            vlc_mutex_lock( &p_thread->lock );
                            vlc_cond_signal( &p_thread->wait );
                            vlc_mutex_unlock( &p_thread->lock );
                        }
                        else if( strcmp( "NetStream.Play.Start", string2 ) == 0 )
                        {
                            p_thread->result_play = 0;

                            vlc_mutex_lock( &p_thread->lock );
                            vlc_cond_signal( &p_thread->wait );
                            vlc_mutex_unlock( &p_thread->lock );
                        }
                        else if( strcmp( "NetStream.Play.Stop", string2 ) == 0 )
                        {
                            p_thread->result_stop = 1;

                            block_FifoWake( p_thread->p_fifo_input );
                        }
                    }
                    free( string2 );
                }
                else if( *i == AMF_DATATYPE_NULL )
                {
                    i++;
                    msg_Dbg( p_thread, "key: %s value: Null", string );
                }
                else if( *i == AMF_DATATYPE_UNDEFINED )
                {
                    i++;
                    msg_Dbg( p_thread, "key: %s value: undefined (Null)", string );
                }
                else
                {
                    i++;
                    msg_Warn( p_thread, "key: %s value: undefined AMF type", string );
                }
                free( string );
            }
            msg_Dbg( p_thread, "end of object" );
        }
        else if( *i == AMF_DATATYPE_NULL)
        {
            i++;
            msg_Dbg( p_thread, "null" );
        }
        else if( *i == AMF_DATATYPE_UNDEFINED )
        {
            i++;
            msg_Dbg( p_thread, "undefined (null)" );
        }
        else
        {
            i++;
            msg_Warn( p_thread, "undefined AMF type" );
        }
    }
    
    free( rtmp_packet->body->body );
    free( rtmp_packet->body );
    free( rtmp_packet );

    return;

error:
    free( tmp_rtmp_packet->body->body );
    free( tmp_rtmp_packet->body );
    free( tmp_rtmp_packet );
    free( tmp_buffer );
}

/* length header calculated automatically based on last packet in the same channel */
/* timestamps passed are always absolute */
static rtmp_packet_t *
rtmp_new_packet( rtmp_control_thread_t *p_thread, uint8_t stream_index, uint32_t timestamp, uint8_t content_type, uint32_t src_dst, rtmp_body_t *body )
{
    int interchunk_headers;
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t *) malloc( sizeof( rtmp_packet_t ) );
    if( !rtmp_packet ) return NULL;

    interchunk_headers = body->length_body / p_thread->chunk_size_send;
    if( body->length_body % p_thread->chunk_size_send == 0 )
        interchunk_headers--;

    if( src_dst != p_thread->rtmp_headers_send[stream_index].src_dst )
    {
        p_thread->rtmp_headers_send[stream_index].timestamp = timestamp;
        p_thread->rtmp_headers_send[stream_index].length_body = body->length_body;
        p_thread->rtmp_headers_send[stream_index].content_type = content_type;
        p_thread->rtmp_headers_send[stream_index].src_dst = src_dst;
        
        rtmp_packet->length_header = 12;
    }
    else if( content_type != p_thread->rtmp_headers_send[stream_index].content_type
        || body->length_body != p_thread->rtmp_headers_send[stream_index].length_body )
    {
        p_thread->rtmp_headers_send[stream_index].timestamp_relative = 
            timestamp - p_thread->rtmp_headers_send[stream_index].timestamp;
        p_thread->rtmp_headers_send[stream_index].timestamp = timestamp;
        p_thread->rtmp_headers_send[stream_index].length_body = body->length_body;
        p_thread->rtmp_headers_send[stream_index].content_type = content_type;

        rtmp_packet->length_header = 8;
    }
    else if( timestamp != p_thread->rtmp_headers_send[stream_index].timestamp )
    {
        p_thread->rtmp_headers_send[stream_index].timestamp_relative = 
            timestamp - p_thread->rtmp_headers_send[stream_index].timestamp;
        p_thread->rtmp_headers_send[stream_index].timestamp = timestamp;

        rtmp_packet->length_header = 4;
    }
    else
    {
        rtmp_packet->length_header = 1;
    }
/*TODO: puede que no haga falta guardar el timestamp relative */
    rtmp_packet->stream_index = stream_index;
    if( rtmp_packet->length_header == 12 )
    {
        rtmp_packet->timestamp = timestamp;
        rtmp_packet->timestamp_relative = 0;
    }
    else
    {
        rtmp_packet->timestamp = timestamp;
        rtmp_packet->timestamp_relative = p_thread->rtmp_headers_send[stream_index].timestamp_relative;
    }
    rtmp_packet->length_encoded = rtmp_packet->length_header + body->length_body + interchunk_headers;
    rtmp_packet->length_body = body->length_body;
    rtmp_packet->content_type = content_type;
    rtmp_packet->src_dst = src_dst;

    rtmp_packet->body = (rtmp_body_t *) malloc( sizeof( rtmp_body_t ) );
    if( !rtmp_packet->body )
    {
       free( rtmp_packet );
       return NULL;
    }

    rtmp_packet->body->length_body = body->length_body;
    rtmp_packet->body->length_buffer = body->length_body;
    rtmp_packet->body->body = (uint8_t *) malloc( rtmp_packet->body->length_buffer * sizeof( uint8_t ) );
    if( !rtmp_packet->body->body )
    {
        free( rtmp_packet->body );
        free( rtmp_packet );
        return NULL;
    }
    memcpy( rtmp_packet->body->body, body->body, rtmp_packet->body->length_body );

    return rtmp_packet;
}

static block_t *
rtmp_new_block( rtmp_control_thread_t *p_thread, uint8_t *buffer, int32_t length_buffer )
{
    block_t *p_buffer;
    /* DOWN: p_thread->p_empty_blocks->i_depth */
    while ( block_FifoCount( p_thread->p_empty_blocks ) > MAX_EMPTY_BLOCKS )
    {
        p_buffer = block_FifoGet( p_thread->p_empty_blocks );
        block_Release( p_buffer );
    }
    /* DOWN: p_thread->p_empty_blocks->i_depth */
    if( block_FifoCount( p_thread->p_empty_blocks ) == 0 )
    {
        p_buffer = block_New( p_thread, length_buffer );
    }
    else
    {
        p_buffer = block_FifoGet( p_thread->p_empty_blocks );
        p_buffer = block_Realloc( p_buffer, 0, length_buffer );
    }

    p_buffer->i_buffer = length_buffer;

    memcpy( p_buffer->p_buffer, buffer, p_buffer->i_buffer );

    return p_buffer;
}

/* call sequence for each packet rtmp_new_packet -> rtmp_encode_packet -> send */
/* no parallelism allowed because of optimization in header length */
uint8_t *
rtmp_encode_packet( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    uint8_t *out;
    int interchunk_headers;
    uint32_t timestamp, length_body, src_dst;
    int i, j;

    out = (uint8_t *) malloc( rtmp_packet->length_encoded * sizeof( uint8_t ) );
    if( !out ) return NULL;

    interchunk_headers = rtmp_packet->body->length_body / p_thread->chunk_size_send;
    if( rtmp_packet->body->length_body % p_thread->chunk_size_send == 0 )
        interchunk_headers--;

    if( rtmp_packet->length_header == 12 )
    {
        /* Timestamp absolute */
        timestamp = hton32( rtmp_packet->timestamp );
        memcpy( out, &timestamp, sizeof( uint32_t ) );

        src_dst = hton32( rtmp_packet->src_dst );
        memcpy( out + 8, &src_dst, sizeof( uint32_t ) );
    }

    if( rtmp_packet->length_header >= 8 )
    {
        /* Length without inter chunk headers */
        length_body = hton32( rtmp_packet->body->length_body );
        memcpy( out + 3, &length_body, sizeof( uint32_t ) );

        out[7] = rtmp_packet->content_type;
    }
    if( rtmp_packet->length_header >= 4 && rtmp_packet->length_header != 12 )
    {
        /* Timestamp relative */
        timestamp = hton32( rtmp_packet->timestamp_relative );
        memcpy( out, &timestamp, sizeof( uint32_t ) );
    }

    out[0] = rtmp_encode_header_size( (vlc_object_t *) p_thread, rtmp_packet->length_header ) + rtmp_packet->stream_index;

    /* Insert inter chunk headers */
    for(i = 0, j = 0; i < rtmp_packet->body->length_body + interchunk_headers; i++, j++)
    {
        if( j % p_thread->chunk_size_send == 0 && j != 0 )
            out[rtmp_packet->length_header + i++] = RTMP_HEADER_SIZE_1 + rtmp_packet->stream_index;
        out[rtmp_packet->length_header + i] = rtmp_packet->body->body[j];
    }

    return out;
}

static rtmp_packet_t *
rtmp_encode_onBWDone( rtmp_control_thread_t *p_thread, double number )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;

    /* Build onBWDone */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "onBWDone" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "onBWDone" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_CONTENT_TYPE_INVOKE, RTMP_SRC_DST_CONNECT_OBJECT, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static rtmp_packet_t *
rtmp_encode_server_bw( rtmp_control_thread_t *p_thread, uint32_t number )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;

    /* Build server bw */
    rtmp_body = rtmp_body_new( -1 );

    rtmp_body_append( rtmp_body, (uint8_t *) &number, sizeof( uint32_t ) );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_CONTROL,
        0, RTMP_CONTENT_TYPE_SERVER_BW, RTMP_SRC_DST_CONNECT_OBJECT, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static rtmp_packet_t *
rtmp_encode_NetConnection_connect_result( rtmp_control_thread_t *p_thread, double number )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;

    /* Build NetConnection.connect result */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "_result" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "_result" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_OBJECT );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "level",
        AMF_DATATYPE_STRING, "status" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "level" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "status" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "code",
        AMF_DATATYPE_STRING, "NetConnection.Connect.Success" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "code" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "NetConnection.Connect.Success" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "description",
        AMF_DATATYPE_STRING, "Connection succeeded." );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "description" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "Connection succeeded." ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element ( AMF_DATATYPE_END_OF_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_END_OF_OBJECT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_CONTENT_TYPE_INVOKE, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static rtmp_packet_t *
rtmp_encode_createStream_result( rtmp_control_thread_t *p_thread, double stream_client_id, double stream_server_id )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;

    /* Build createStream result */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "_result" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "_result" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER, &stream_client_id );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER, &stream_server_id );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_CONTENT_TYPE_INVOKE, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static rtmp_packet_t *
rtmp_encode_ping_reset_stream( rtmp_control_thread_t *p_thread )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;

    /* Build ping reset stream */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = rtmp_encode_ping( RTMP_PING_RESET_STREAM, RTMP_SRC_DST_CONNECT_OBJECT2, 0, 0 );
    rtmp_body_append( rtmp_body, tmp_buffer, RTMP_PING_SIZE_RESET_STREAM );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_CONTROL,
        0, RTMP_CONTENT_TYPE_PING, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static rtmp_packet_t *
rtmp_encode_ping_clear_stream( rtmp_control_thread_t *p_thread, uint32_t src_dst )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;

    /* Build ping clear stream */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = rtmp_encode_ping( RTMP_PING_CLEAR_STREAM, src_dst, 0, 0 );
    rtmp_body_append( rtmp_body, tmp_buffer, RTMP_PING_SIZE_CLEAR_STREAM );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_CONTROL,
        0, RTMP_CONTENT_TYPE_PING, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static rtmp_packet_t *
rtmp_encode_NetStream_play_reset_onStatus( rtmp_control_thread_t *p_thread, char *psz_media )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;
    double number;
    char *description;

    /* Build NetStream.play.reset onStatus */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "onStatus" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "onStatus" ) );
    free( tmp_buffer );

    number = 1; /* TODO: review this number*/
    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_OBJECT );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "level",
        AMF_DATATYPE_STRING, "status" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "level" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "status" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "code",
        AMF_DATATYPE_STRING, "NetStream.Play.Reset" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "code" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "NetStream.Play.Reset" ) );
    free( tmp_buffer );

    description = (char *) malloc( strlen( "Playing and resetting ") + strlen( psz_media ) + strlen( "." ) + 1 );
    if( !description )
    {
        free( rtmp_body->body );
        free( rtmp_body );
        return NULL;
    }
    sprintf( description, "Playing and resetting %s.", psz_media );
    tmp_buffer = amf_encode_object_variable( "description",
        AMF_DATATYPE_STRING, description );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "description" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( description ) );
    free( tmp_buffer );
    free( description );

    tmp_buffer = amf_encode_object_variable( "details",
        AMF_DATATYPE_STRING, psz_media );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "details" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( psz_media ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "clientid",
        AMF_DATATYPE_NUMBER, &p_thread->stream_client_id );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "clientid" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element ( AMF_DATATYPE_END_OF_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_END_OF_OBJECT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_NOTIFY,
        0, RTMP_CONTENT_TYPE_INVOKE, RTMP_SRC_DST_DEFAULT, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static rtmp_packet_t *
rtmp_encode_NetStream_play_start_onStatus( rtmp_control_thread_t *p_thread, char *psz_media )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;
    double number;
    char *description;

    /* Build NetStream.play.start onStatus */
    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "onStatus" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "onStatus" ) );
    free( tmp_buffer );

    number = 1; /* TODO: review this number*/
    tmp_buffer = amf_encode_element( AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_NULL, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_NULL );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_OBJECT );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "level",
        AMF_DATATYPE_STRING, "status" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "level" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "status" ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "code",
        AMF_DATATYPE_STRING, "NetStream.Play.Start" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "code" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( "NetStream.Play.Start" ) );
    free( tmp_buffer );

    description = (char *) malloc( strlen( "Started playing ") + strlen( psz_media ) + strlen( "." ) + 1 );
    if( !description )
    {
        free( rtmp_body->body );
        free( rtmp_body );
        return NULL;
    }

    sprintf( description, "Started playing %s.", psz_media );
    tmp_buffer = amf_encode_object_variable( "description",
        AMF_DATATYPE_STRING, description );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "description" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( description ) );
    free( tmp_buffer );
    free( description );

    tmp_buffer = amf_encode_object_variable( "details",
        AMF_DATATYPE_STRING, psz_media );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "details" ) +
        AMF_DATATYPE_SIZE_STRING + strlen( psz_media ) );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "clientid",
        AMF_DATATYPE_NUMBER, &p_thread->stream_client_id );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "clientid" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element ( AMF_DATATYPE_END_OF_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_END_OF_OBJECT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_thread, RTMP_DEFAULT_STREAM_INDEX_NOTIFY,
        0, RTMP_CONTENT_TYPE_INVOKE, RTMP_SRC_DST_DEFAULT, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

static uint8_t
rtmp_encode_header_size( vlc_object_t *p_this, uint8_t header_size )
{
    if( header_size == 1 )
        return RTMP_HEADER_SIZE_1;
    else if( header_size == 4 )
        return RTMP_HEADER_SIZE_4;
    else if( header_size == 8 )
        return RTMP_HEADER_SIZE_8;
    else if( header_size == 12 )
        return RTMP_HEADER_SIZE_12;
    else
    {
        msg_Err( p_this, "invalid header size for encoding" );
        return 0;
    }
}

static uint8_t
rtmp_decode_header_size( vlc_object_t *p_this, uint8_t header_size )
{
    if( header_size == RTMP_HEADER_SIZE_1 )
        return 1;
    else if( header_size == RTMP_HEADER_SIZE_4 )
        return 4;
    else if( header_size == RTMP_HEADER_SIZE_8 )
        return 8;
    else if( header_size == RTMP_HEADER_SIZE_12 )
        return 12;
    else
    {
        msg_Err( p_this, "invalid RTMP_HEADER_SIZE_XX " );
        return 0;
    }
}

static uint8_t
rtmp_get_stream_index( uint8_t content_type )
{
    if( content_type == RTMP_CONTENT_TYPE_AUDIO_DATA )
        return RTMP_DEFAULT_STREAM_INDEX_AUDIO_DATA;
    else if( content_type == RTMP_CONTENT_TYPE_VIDEO_DATA )
        return RTMP_DEFAULT_STREAM_INDEX_VIDEO_DATA;
    else if( content_type == RTMP_CONTENT_TYPE_NOTIFY )
        return RTMP_DEFAULT_STREAM_INDEX_NOTIFY;
    else
        return -1;
}

/*****************************************************************************
 * Body handling implementation:
 ******************************************************************************/
rtmp_body_t *
rtmp_body_new( int length_buffer )
{
    rtmp_body_t *rtmp_body;

    rtmp_body = (rtmp_body_t *) malloc( sizeof( rtmp_body_t ) );
    if( !rtmp_body ) return NULL;

    rtmp_body->length_body = 0;
    if( length_buffer < 0 )
        rtmp_body->length_buffer = RTMP_BODY_SIZE_ALLOC;
    else
        rtmp_body->length_buffer = length_buffer;
    rtmp_body->body = (uint8_t *) malloc( rtmp_body->length_buffer * sizeof( uint8_t ) );
    if( !rtmp_body->body )
    {
        free( rtmp_body );
        return NULL;
    }
    return rtmp_body;
}

void
rtmp_body_reset( rtmp_body_t *rtmp_body )
{
    rtmp_body->length_body = 0;
}

static void
rtmp_body_append( rtmp_body_t *rtmp_body, uint8_t *buffer, uint32_t length )
{
    if( rtmp_body->length_body + length > rtmp_body->length_buffer )
    {
        uint8_t *tmp;
        rtmp_body->length_buffer = rtmp_body->length_body + length;
        tmp =  realloc( rtmp_body->body,
                        rtmp_body->length_buffer * sizeof( uint8_t ) );
        if( !tmp ) return;
        rtmp_body->body = tmp;
    }

    memcpy( rtmp_body->body + rtmp_body->length_body, buffer, length );
    rtmp_body->length_body += length;
}

/*****************************************************************************
 * RTMP ping implementation:
 ******************************************************************************/
static uint8_t *
rtmp_encode_ping( uint16_t type, uint32_t src_dst, uint32_t third_arg, uint32_t fourth_arg )
{
    uint8_t *out = NULL;
    VLC_UNUSED(fourth_arg);

    if( type == RTMP_PING_CLEAR_STREAM )
        out = (uint8_t *) malloc( RTMP_PING_SIZE_CLEAR_STREAM * sizeof( uint8_t ) );
    else if( type == RTMP_PING_CLEAR_PLAYING_BUFFER )
        out = (uint8_t *) malloc( RTMP_PING_SIZE_CLEAR_PLAYING_BUFFER * sizeof( uint8_t ) );
    else if( type == RTMP_PING_BUFFER_TIME_CLIENT )
    {
        out = (uint8_t *) malloc( RTMP_PING_SIZE_BUFFER_TIME_CLIENT * sizeof( uint8_t ) );
        if( !out ) goto error;
        third_arg = hton32( third_arg );
        memcpy( out + 6, &third_arg, sizeof( uint32_t ) );
    }
    else if( type == RTMP_PING_RESET_STREAM )
    {
        out = (uint8_t *) malloc( RTMP_PING_SIZE_RESET_STREAM * sizeof( uint8_t ) );
    }
/*    else if( type == RTMP_PING_CLIENT_FROM_SERVER ) TODO: research this
    {
    }
    else if( type == RTMP_PING_PONG_FROM_CLIENT )
    {
    }
*/    else
    {
        out = (uint8_t *) malloc( RTMP_PING_SIZE_BUFFER_TIME_CLIENT * sizeof( uint8_t ) );
        if( !out ) goto error;
        out[6] = 0x0D; out[7] = 0x0E; out[8] = 0x0A; out[9] = 0x0D;
    }

    if( !out ) goto error;

    type = hton16( type );
    memcpy( out, &type, sizeof( uint16_t ) );

    src_dst = hton32( src_dst );
    memcpy( out + 2, &src_dst, sizeof( uint32_t ) );

    return out;

error:
    return NULL;
}

/*****************************************************************************
 * AMF implementation:
 ******************************************************************************/
static uint8_t *
amf_encode_element( uint8_t element, const void *value )
{
    uint8_t *out;

    if ( element == AMF_DATATYPE_NUMBER )
    {
        uint64_t number = *(uint64_t *) value;

        out = (uint8_t *) malloc( AMF_DATATYPE_SIZE_NUMBER * sizeof( uint8_t ) );
        if( !out ) return NULL;
        
        number = hton64( number );
        out[0] = AMF_DATATYPE_NUMBER;
        memcpy( out + 1, &number, sizeof( uint64_t ) );
    } else if ( element == AMF_DATATYPE_BOOLEAN )
    {
        out = (uint8_t *) malloc( AMF_DATATYPE_SIZE_BOOLEAN * sizeof( uint8_t ) );
        if( !out ) return NULL;

        out[0] = AMF_DATATYPE_BOOLEAN;
        out[1] = *(uint8_t *) value;
    } else if ( element == AMF_DATATYPE_STRING )
    {
        uint16_t length_psz, length_psz_cpy;

        length_psz = length_psz_cpy = strlen( (char *) value );
        out = (uint8_t *) malloc( ( AMF_DATATYPE_SIZE_STRING + length_psz ) * sizeof( uint8_t ) );
        if( !out ) return NULL;

        out[0] = AMF_DATATYPE_STRING;
        length_psz = hton16( length_psz );
        memcpy( out + 1, &length_psz, sizeof( uint16_t ) );
        memcpy( out + 3, value, length_psz_cpy );
    } else if ( element == AMF_DATATYPE_OBJECT )
    {
        out = (uint8_t *) malloc( AMF_DATATYPE_SIZE_OBJECT * sizeof( uint8_t ) );
        if( !out ) return NULL;

        out[0] = AMF_DATATYPE_OBJECT;
    } else if ( element == AMF_DATATYPE_NULL )
    {
        out = (uint8_t *) malloc( AMF_DATATYPE_SIZE_NULL * sizeof( uint8_t ) );
        if( !out ) return NULL;

        out[0] = AMF_DATATYPE_NULL;
    } else if ( element == AMF_DATATYPE_MIXED_ARRAY )
    {
        uint32_t highest_index = *(uint32_t *) value;

        out = (uint8_t *) malloc( AMF_DATATYPE_SIZE_MIXED_ARRAY * sizeof( uint8_t ) );
        if( !out ) return NULL;

        highest_index = hton32( highest_index );
        out[0] = AMF_DATATYPE_MIXED_ARRAY;
        memcpy( out + 1, &highest_index, sizeof( uint32_t ) );
    } else if ( element == AMF_DATATYPE_END_OF_OBJECT )
    {
        out = (uint8_t *) calloc( AMF_DATATYPE_SIZE_END_OF_OBJECT, sizeof( uint8_t ) );

        out[AMF_DATATYPE_SIZE_END_OF_OBJECT - 1] = AMF_DATATYPE_END_OF_OBJECT;
    } else
    {
        out = (uint8_t *) malloc( AMF_DATATYPE_SIZE_NUMBER * sizeof( uint8_t ) );
        if( !out ) return NULL;

        out[0] = AMF_DATATYPE_NUMBER;
        out[1] = 0x0D; out[2] = 0x0E; out[3] = 0x0A; out[4] = 0x0D;
        out[5] = 0x0B; out[6] = 0x0E; out[7] = 0x0E; out[8] = 0x0F;
    }

    return out;
}

static uint8_t *
amf_encode_object_variable( const char *key, uint8_t element, const void *value )
{
    uint8_t *out, *out_value;
    int length_value;
    uint16_t length_psz, length_psz_cpy;

    length_psz = length_psz_cpy = strlen( key );

    if( element == AMF_DATATYPE_NUMBER )
        length_value = AMF_DATATYPE_SIZE_NUMBER;
    else if( element == AMF_DATATYPE_BOOLEAN )
        length_value = AMF_DATATYPE_SIZE_BOOLEAN;
    else if( element == AMF_DATATYPE_STRING )
        length_value = AMF_DATATYPE_SIZE_STRING + strlen( (char *) value );
    else if( element == AMF_DATATYPE_NULL )
        length_value = AMF_DATATYPE_SIZE_NULL;
    else
    {
        out = (uint8_t *) malloc( AMF_DATATYPE_SIZE_NUMBER * sizeof( uint8_t ) );
        if( !out ) return NULL;

        out[0] = AMF_DATATYPE_NUMBER;
        out[1] = 0xD; out[2] = 0xE; out[3] = 0xA; out[4] = 0xD;
        out[5] = 0xB; out[6] = 0xE; out[7] = 0xE; out[8] = 0xF;

        return out;
    }

    out = (uint8_t *) malloc( ( AMF_DATATYPE_SIZE_OBJECT_VARIABLE + length_psz + length_value ) * sizeof( uint8_t ) );
    if( !out ) return NULL;

    length_psz = hton16( length_psz );
    memcpy( out, &length_psz, sizeof( uint16_t ) );
    memcpy( out + 2, key, length_psz_cpy );

    out_value = amf_encode_element( element, value );
    memcpy( out + 2 + length_psz_cpy, out_value, length_value );
    free( out_value );

    return out;
}

static double
amf_decode_number( uint8_t **buffer )
{
    uint64_t number;
    double out;

    number = ntoh64( *(uint64_t *) *buffer );
    memcpy(&out, &number, sizeof( uint64_t ) );
    *buffer += sizeof( uint64_t );

    return out;
}

static int
amf_decode_boolean( uint8_t **buffer )
{
    int out;

    out = **buffer;
    *buffer += 1;

    return out;
}

/* return value allocated dinamically */
static char *
amf_decode_string( uint8_t **buffer )
{
    char *out;
    int length;
    int i;

    length = ntoh16( *(uint16_t *) *buffer );
    *buffer += sizeof( uint16_t );

    out = (char *) malloc( length + 1 ); /* '\0' terminated */
    if( !out ) return NULL;

    for(i = 0; i < length; i++)
        out[i] = (*buffer)[i];

    *buffer += length;

    out[i] = '\0';

    return out;
}

/* returns in each call next key, at end of object returns NULL */
/* need to decode value of key after call */
static char *
amf_decode_object( uint8_t **buffer )
{
    if( **buffer == 0x0 && *(*buffer + 1) == 0x00 && *(*buffer + 2) == 0x09)
    {
        *buffer += 3;

        return NULL;
    }
    else
        return amf_decode_string( buffer );
}

/*****************************************************************************
 * FLV rebuilding implementation:
 ******************************************************************************/
static void
flv_rebuild( rtmp_control_thread_t *p_thread, rtmp_packet_t *rtmp_packet )
{
    uint32_t length_tag, timestamp;
    uint8_t *tmp;

    tmp = (uint8_t *) realloc( rtmp_packet->body->body,
                               rtmp_packet->body->length_body +
                               FLV_TAG_PREVIOUS_TAG_SIZE + FLV_TAG_SIZE );
    if( !tmp ) return;
    rtmp_packet->body->body = tmp;
    memmove( rtmp_packet->body->body + FLV_TAG_PREVIOUS_TAG_SIZE + FLV_TAG_SIZE,
             rtmp_packet->body->body, rtmp_packet->body->length_body );

    /* Insert tag */
    p_thread->flv_tag_previous_tag_size = hton32( p_thread->flv_tag_previous_tag_size );
    memcpy( rtmp_packet->body->body, &p_thread->flv_tag_previous_tag_size, sizeof( uint32_t ) );

    /* Fill backwards because of overlapping*/
    rtmp_packet->body->body[11] = 0x00;

    timestamp = hton32( rtmp_packet->timestamp );
    memcpy( rtmp_packet->body->body + 7, &timestamp, sizeof( uint32_t ) );

    length_tag = hton32( rtmp_packet->body->length_body );
    memcpy( rtmp_packet->body->body + 4, &length_tag, sizeof( uint32_t ) );

    rtmp_packet->body->body[4] = rtmp_packet->content_type;

    rtmp_packet->body->body[12] = 0x00;
    rtmp_packet->body->body[13] = 0x00;
    rtmp_packet->body->body[14] = 0x00;

    p_thread->flv_tag_previous_tag_size = rtmp_packet->body->length_body + FLV_TAG_SIZE;

    /* Update size */
    rtmp_packet->body->length_body += FLV_TAG_PREVIOUS_TAG_SIZE + FLV_TAG_SIZE;
    rtmp_packet->body->length_buffer = rtmp_packet->body->length_body;
}

static void
flv_get_metadata_audio( rtmp_control_thread_t *p_thread, rtmp_packet_t *packet_audio, uint8_t *stereo, uint8_t *audiosamplesize, uint32_t *audiosamplerate, uint8_t *audiocodecid )
{
    uint8_t data_audio;

    data_audio = *packet_audio->body->body;

    if( ( data_audio & FLV_AUDIO_STEREO_MASK ) == FLV_AUDIO_STEREO_MONO )
        *stereo = FLV_AUDIO_STEREO_MONO;
    else if( ( data_audio & FLV_AUDIO_STEREO_MASK ) == FLV_AUDIO_STEREO_STEREO )
        *stereo = FLV_AUDIO_STEREO_STEREO;
    else
        msg_Warn( p_thread, "unknown metadata audio stereo" );

    if( ( data_audio & FLV_AUDIO_SIZE_MASK ) == FLV_AUDIO_SIZE_8_BIT )
        *audiosamplesize = FLV_AUDIO_SIZE_8_BIT >> 1;
    else if( ( data_audio & FLV_AUDIO_SIZE_MASK ) == FLV_AUDIO_SIZE_16_BIT )
        *audiosamplesize = FLV_AUDIO_SIZE_16_BIT >> 1;
    else
        msg_Warn( p_thread, "unknown metadata audio sample size" );

    if( ( data_audio & FLV_AUDIO_RATE_MASK ) == FLV_AUDIO_RATE_5_5_KHZ )
        *audiosamplerate = 5512;
    else if( ( data_audio & FLV_AUDIO_RATE_MASK ) == FLV_AUDIO_RATE_11_KHZ )
        *audiosamplerate = 11025;
    else if( ( data_audio & FLV_AUDIO_RATE_MASK ) == FLV_AUDIO_RATE_22_KHZ )
        *audiosamplerate = 22050;
    else if( ( data_audio & FLV_AUDIO_RATE_MASK ) == FLV_AUDIO_RATE_44_KHZ )
        *audiosamplerate = 44100;
    else
        msg_Warn( p_thread, "unknown metadata audio sample rate" );

    if( ( data_audio & FLV_AUDIO_CODEC_ID_MASK ) == FLV_AUDIO_CODEC_ID_UNCOMPRESSED )
        *audiocodecid = FLV_AUDIO_CODEC_ID_UNCOMPRESSED >> 4;
    else if( ( data_audio & FLV_AUDIO_CODEC_ID_MASK ) == FLV_AUDIO_CODEC_ID_ADPCM )
        *audiocodecid = FLV_AUDIO_CODEC_ID_ADPCM >> 4;
    else if( ( data_audio & FLV_AUDIO_CODEC_ID_MASK ) == FLV_AUDIO_CODEC_ID_MP3 )
        *audiocodecid = FLV_AUDIO_CODEC_ID_MP3 >> 4;
    else if( ( data_audio & FLV_AUDIO_CODEC_ID_MASK ) == FLV_AUDIO_CODEC_ID_NELLYMOSER_8KHZ_MONO )
        *audiocodecid = FLV_AUDIO_CODEC_ID_NELLYMOSER_8KHZ_MONO >> 4;
    else if( ( data_audio & FLV_AUDIO_CODEC_ID_MASK ) == FLV_AUDIO_CODEC_ID_NELLYMOSER )
        *audiocodecid = FLV_AUDIO_CODEC_ID_NELLYMOSER >> 4;
    else
        msg_Warn( p_thread, "unknown metadata audio codec id" );
}

static void
flv_get_metadata_video( rtmp_control_thread_t *p_thread, rtmp_packet_t *packet_video, uint8_t *videocodecid, uint8_t *frametype )
{
    uint8_t data_video;

    data_video = *packet_video->body->body;

    if( ( data_video & FLV_VIDEO_CODEC_ID_MASK ) == FLV_VIDEO_CODEC_ID_SORENSEN_H263 )
        *videocodecid = FLV_VIDEO_CODEC_ID_SORENSEN_H263;
    else if( ( data_video & FLV_VIDEO_CODEC_ID_MASK ) == FLV_VIDEO_CODEC_ID_SCREEN_VIDEO )
        *videocodecid = FLV_VIDEO_CODEC_ID_SCREEN_VIDEO;
    else if( ( data_video & FLV_VIDEO_CODEC_ID_MASK ) == FLV_VIDEO_CODEC_ID_ON2_VP6 )
        *videocodecid = FLV_VIDEO_CODEC_ID_ON2_VP6;
    else if( ( data_video & FLV_VIDEO_CODEC_ID_MASK ) == FLV_VIDEO_CODEC_ID_ON2_VP6_ALPHA )
        *videocodecid = FLV_VIDEO_CODEC_ID_ON2_VP6_ALPHA;
    else if( ( data_video & FLV_VIDEO_CODEC_ID_MASK ) == FLV_VIDEO_CODEC_ID_SCREEN_VIDEO_2 )
        *videocodecid = FLV_VIDEO_CODEC_ID_SCREEN_VIDEO_2;
    else
        msg_Warn( p_thread, "unknown metadata video codec id" );

    if( ( data_video & FLV_VIDEO_FRAME_TYPE_MASK ) == FLV_VIDEO_FRAME_TYPE_KEYFRAME )
        *frametype = FLV_VIDEO_FRAME_TYPE_KEYFRAME >> 4;
    else if( ( data_video & FLV_VIDEO_FRAME_TYPE_MASK ) == FLV_VIDEO_FRAME_TYPE_INTER_FRAME )
        *frametype = FLV_VIDEO_FRAME_TYPE_INTER_FRAME >> 4;
    else if( ( data_video & FLV_VIDEO_FRAME_TYPE_MASK ) == FLV_VIDEO_FRAME_TYPE_DISPOSABLE_INTER_FRAME )
        *frametype = FLV_VIDEO_FRAME_TYPE_DISPOSABLE_INTER_FRAME >> 4;
    else
        msg_Warn( p_thread, "unknown metadata video frame type" );
}

static rtmp_packet_t *
flv_build_onMetaData( access_t *p_access, uint64_t duration, uint8_t stereo, uint8_t audiosamplesize, uint32_t audiosamplerate, uint8_t audiocodecid, uint8_t videocodecid )
{
    rtmp_packet_t *rtmp_packet;
    rtmp_body_t *rtmp_body;
    uint8_t *tmp_buffer;
    double number;

    rtmp_body = rtmp_body_new( -1 );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_STRING, "onMetaData" );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_STRING + strlen( "onMetaData" ) );
    free( tmp_buffer );

    number = 0;
    tmp_buffer = amf_encode_element( AMF_DATATYPE_MIXED_ARRAY, &number );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_MIXED_ARRAY );
    free( tmp_buffer );

    number = duration;
    tmp_buffer = amf_encode_object_variable( "duration",
        AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "duration" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_object_variable( "stereo",
        AMF_DATATYPE_BOOLEAN, &stereo );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "stereo" ) +
        AMF_DATATYPE_SIZE_BOOLEAN );
    free( tmp_buffer );

    number = audiosamplesize;
    tmp_buffer = amf_encode_object_variable( "audiosamplesize",
        AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "audiosamplesize" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    number = audiosamplerate;
    tmp_buffer = amf_encode_object_variable( "audiosamplerate",
        AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "audiosamplerate" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    number = audiocodecid;
    tmp_buffer = amf_encode_object_variable( "audiocodecid",
        AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "audiocodecid" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    number = videocodecid;
    tmp_buffer = amf_encode_object_variable( "videocodecid",
        AMF_DATATYPE_NUMBER, &number );
    rtmp_body_append( rtmp_body, tmp_buffer,
        AMF_DATATYPE_SIZE_OBJECT_VARIABLE + strlen( "videocodecid" ) +
        AMF_DATATYPE_SIZE_NUMBER );
    free( tmp_buffer );

    tmp_buffer = amf_encode_element( AMF_DATATYPE_END_OF_OBJECT, NULL );
    rtmp_body_append( rtmp_body, tmp_buffer, AMF_DATATYPE_SIZE_END_OF_OBJECT );
    free( tmp_buffer );

    rtmp_packet = rtmp_new_packet( p_access->p_sys->p_thread, RTMP_DEFAULT_STREAM_INDEX_INVOKE,
        0, RTMP_CONTENT_TYPE_NOTIFY, 0, rtmp_body );
    free( rtmp_body->body );
    free( rtmp_body );

    return rtmp_packet;
}

block_t *
flv_get_metadata( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    rtmp_packet_t *flv_metadata_packet;
    block_t *p_buffer;

    flv_metadata_packet = flv_build_onMetaData( p_access, 0, p_sys->p_thread->metadata_stereo,
        p_sys->p_thread->metadata_samplesize, p_sys->p_thread->metadata_samplerate,
        p_sys->p_thread->metadata_audiocodecid, p_sys->p_thread->metadata_videocodecid );
    flv_rebuild( p_sys->p_thread, flv_metadata_packet );
    p_buffer = rtmp_new_block( p_sys->p_thread, flv_metadata_packet->body->body, flv_metadata_packet->body->length_buffer );

    free( flv_metadata_packet->body->body );
    free( flv_metadata_packet->body );
    free( flv_metadata_packet );

    return p_buffer;
}

block_t *
flv_insert_header( access_t *p_access, block_t *first_packet )
{
    access_sys_t *p_sys = p_access->p_sys;
    int old_buffer_size;
    uint32_t tmp_number;

    old_buffer_size = first_packet->i_buffer;

    first_packet = block_Realloc( first_packet, 0, first_packet->i_buffer + FLV_HEADER_SIZE );

    memmove( first_packet->p_buffer + FLV_HEADER_SIZE,
        first_packet->p_buffer, old_buffer_size );

    memcpy( first_packet->p_buffer, FLV_HEADER_SIGNATURE, sizeof( FLV_HEADER_SIGNATURE ) );
    first_packet->p_buffer[3] = FLV_HEADER_VERSION;
    if( p_sys->p_thread->has_audio && p_sys->p_thread->has_video )
        first_packet->p_buffer[4] = FLV_HEADER_AUDIO | FLV_HEADER_VIDEO;
    else if( p_sys->p_thread->has_audio )
        first_packet->p_buffer[4] = FLV_HEADER_AUDIO;
    else
        first_packet->p_buffer[4] = FLV_HEADER_VIDEO;
    tmp_number = hton32( FLV_HEADER_SIZE );
    memcpy( first_packet->p_buffer + 5, &tmp_number, sizeof( uint32_t ) );

    return first_packet;
}
