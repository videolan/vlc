/*****************************************************************************
 * input_dvd.c: DVD reading
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_dvd.c,v 1.6 2001/01/22 05:20:44 stef Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <sys/types.h>

#include <string.h>
#include <errno.h>
#include <malloc.h>

#include <sys/ioctl.h>
#ifdef HAVE_SYS_DVDIO_H
# include <sys/dvdio.h>
#endif
#ifdef LINUX_DVD
#include <linux/cdrom.h>
#endif

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "main.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"

#include "dvd_ifo.h"
#include "dvd_css.h"
#include "input_dvd.h"
#include "mpeg_system.h"

#include "debug.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DVDProbe    ( struct input_thread_s * );
static int  DVDRead     ( struct input_thread_s *,
                          data_packet_t * p_packets[INPUT_READ_ONCE] );
static void DVDInit     ( struct input_thread_s * );
static void DVDEnd      ( struct input_thread_s * );
/* FIXME : DVDSeek should be on 64 bits ? Is it possible in input ? */
static int  DVDSeek     ( struct input_thread_s *, off_t );
static int  DVDRewind   ( struct input_thread_s * );
static struct data_packet_s * NewPacket ( void *, size_t );
static void DeletePacket( void *, struct data_packet_s * );
static void DeletePES   ( void *, struct pes_packet_s * );

/*
 * Data reading functions
 */

/*****************************************************************************
 * DVDProbe: check the stream
 *****************************************************************************/
static int DVDProbe( input_thread_t * p_input )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    dvd_struct dvd;

    dvd.type = DVD_STRUCT_COPYRIGHT;
    dvd.copyright.layer_num = 0;

    if( ioctl( p_input->i_handle, DVD_READ_STRUCT, &dvd ) < 0 )
    {
        intf_ErrMsg( "DVD ioctl error" );
        return -1;
    }

    return dvd.copyright.cpst;
#else
    return 0;
#endif
}

/*****************************************************************************
 * DVDInit: initializes DVD structures
 *****************************************************************************/
static void DVDInit( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_method;
    off64_t              i_start;

    if( (p_method = malloc( sizeof(thread_dvd_data_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_method;
    p_input->p_method_data = NULL;

    p_method->i_fd = p_input->i_handle;


    lseek64( p_input->i_handle, 0, SEEK_SET );

    /* Ifo initialisation */
    p_method->ifo = IfoInit( p_input->i_handle );
    IfoRead( &(p_method->ifo) );

#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    /* CSS authentication and keys */
    if( ( p_method->b_encrypted = DVDProbe( p_input ) ) )
    {
        int   i;

fprintf(stderr, " CSS Init start\n" );
        p_method->css = CSSInit( p_input->i_handle );
fprintf(stderr, " CSS Init end\n" );
        p_method->css.i_title_nb = p_method->ifo.vmg.mat.i_tts_nb;
        if( (p_method->css.p_title_key =
             malloc( p_method->css.i_title_nb *
                     sizeof(p_method->css.p_title_key) ) ) == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            p_input->b_error = 1;
            return;
        }
        for( i=0 ; i<p_method->css.i_title_nb ; i++ )
        {
            p_method->css.p_title_key[i].i =
                      p_method->ifo.p_vts[i].i_pos +
                      p_method->ifo.p_vts[i].mat.i_tt_vobs_ssector *DVD_LB_SIZE;
        }
fprintf(stderr, " CSS Get start\n" );
        CSSGetKeys( &(p_method->css) );
fprintf(stderr, " CSS Get end\n" );
    }
#endif

    i_start = p_method->ifo.p_vts[0].i_pos +
              p_method->ifo.p_vts[0].mat.i_tt_vobs_ssector *DVD_LB_SIZE;

    i_start = lseek64( p_input->i_handle, i_start, SEEK_SET );
    fprintf(stderr, "Begin at : %lld\n", (long long)i_start );

#if 1
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );
    input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );

    if( p_input->stream.b_seekable )
    {
        stream_ps_data_t * p_demux_data =
             (stream_ps_data_t *)p_input->stream.pp_programs[0]->p_demux_data;

        /* Pre-parse the stream to gather stream_descriptor_t. */
        p_input->stream.pp_programs[0]->b_is_ok = 0;
        p_demux_data->i_PSM_version = EMPTY_PSM_VERSION;

        while( !p_input->b_die && !p_input->b_error
                && !p_demux_data->b_has_PSM )
        {
            int                 i_result, i;
            data_packet_t *     pp_packets[INPUT_READ_ONCE];

            i_result = DVDRead( p_input, pp_packets );
            if( i_result == 1 )
            {
                /* EOF */
                vlc_mutex_lock( &p_input->stream.stream_lock );
                p_input->stream.pp_programs[0]->b_is_ok = 1;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                break;
            }
            if( i_result == -1 )
            {
                p_input->b_error = 1;
                break;
            }

            for( i = 0; i < INPUT_READ_ONCE && pp_packets[i] != NULL; i++ )
            {
                /* FIXME: use i_p_config_t */
                input_ParsePS( p_input, pp_packets[i] );
                DeletePacket( p_input->p_method_data, pp_packets[i] );
            }

            /* File too big. */
            if( p_input->stream.i_tell > INPUT_PREPARSE_LENGTH )
            {
                break;
            }
        }
        lseek64( p_input->i_handle, i_start, SEEK_SET );
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.i_tell = 0;
        if( p_demux_data->b_has_PSM )
        {
            /* (The PSM decoder will care about spawning the decoders) */
            p_input->stream.pp_programs[0]->b_is_ok = 1;
        }
#ifdef AUTO_SPAWN
        else
        {
            /* (We have to do it ourselves) */
            int                 i_es;

            /* FIXME: we should do multiple passes in case an audio type
             * is not present */
            for( i_es = 0;
                 i_es < p_input->stream.pp_programs[0]->i_es_number;
                 i_es++ )
            {
#define p_es p_input->stream.pp_programs[0]->pp_es[i_es]
                switch( p_es->i_type )
                {
                    case MPEG1_VIDEO_ES:
                    case MPEG2_VIDEO_ES:
                        input_SelectES( p_input, p_es );
                        break;

                    case MPEG1_AUDIO_ES:
                    case MPEG2_AUDIO_ES:
                        if( main_GetIntVariable( INPUT_DVD_AUDIO_VAR, 0 )
                                == REQUESTED_MPEG 
                            && main_GetIntVariable( INPUT_DVD_CHANNEL_VAR, 0 )
                                == (p_es->i_id & 0x1F) )
                        {
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case AC3_AUDIO_ES:
                        if( main_GetIntVariable( INPUT_DVD_AUDIO_VAR, 0 )
                                == REQUESTED_AC3
                            && main_GetIntVariable( INPUT_DVD_CHANNEL_VAR, 0 )
                                == ((p_es->i_id & 0xF00) >> 8) )
                        {
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case DVD_SPU_ES:
                        if( main_GetIntVariable( INPUT_DVD_SUBTITLE_VAR, -1 )
                                == ((p_es->i_id & 0x1F00) >> 8) )
                        {
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case LPCM_AUDIO_ES:
                        /* FIXME ! */
                        break;
                }
            }
                    
        }
#endif
#ifdef STATS
        input_DumpStream( p_input );
#endif
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else
    {
#endif
        /* The programs will be added when we read them. */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.pp_programs[0]->b_is_ok = 0;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
}

/*****************************************************************************
 * DVDEnd: frees unused data
 *****************************************************************************/
static void DVDEnd( input_thread_t * p_input )
{
    free( p_input->stream.p_demux_data );
    free( p_input->p_plugin_data );
}

/*****************************************************************************
 * SafeRead: reads a chunk of stream and correctly detects errors
 *****************************************************************************/
static __inline__ int SafeRead( input_thread_t * p_input, byte_t * p_buffer,
                                size_t i_len )
{
    thread_dvd_data_t * p_method;
    int                 i_nb;

    p_method = (thread_dvd_data_t *)p_input->p_plugin_data;
//    if( !p_method->b_encrypted )
//    {
        i_nb = read( p_input->i_handle, p_buffer, i_len );
#if 0
    }
    else
    {
        i_nb = read( p_input->i_handle, p_buffer, 4096 );
        CSSDescrambleSector( p_method->css.p_title_key.key, p_buffer );
    }
    switch( i_nb )
    {
        case 0:
            /* End of File */
            return( 1 );
        case -1:
            intf_ErrMsg( "Read failed (%s)", strerror(errno) );
            return( -1 );
        default:
            break;
    }
#endif
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.i_tell += i_nb; //lseek64( p_input->i_handle,
                             //         p_input->stream.i_tell+i_len, SEEK_SET );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    return( 0 );
}

/*****************************************************************************
 * DVDRead: reads data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int DVDRead( input_thread_t * p_input,
                   data_packet_t * pp_packets[INPUT_READ_ONCE] )
{
    byte_t              p_header[6];
    data_packet_t *     p_data;
    size_t              i_packet_size;
    int                 i_packet, i_error;
    thread_dvd_data_t * p_method;

    p_method = (thread_dvd_data_t *)p_input->p_plugin_data;

    memset( pp_packets, 0, INPUT_READ_ONCE * sizeof(data_packet_t *) );
    for( i_packet = 0; i_packet < INPUT_READ_ONCE; i_packet++ )
    {
        /* Read what we believe to be a packet header. */
        if( (i_error = SafeRead( p_input, p_header, 6 )) )
        {
            return( i_error );
        }

        if( (U32_AT(p_header) & 0xFFFFFF00) != 0x100L )
        {
            /* This is not the startcode of a packet. Read the stream
             * until we find one. */
            u32         i_startcode = U32_AT(p_header);
            int         i_dummy,i_nb;

            if( i_startcode )
            {
                /* It is common for MPEG-1 streams to pad with zeros
                 * (although it is forbidden by the recommendation), so
                 * don't bother everybody in this case. */
                intf_WarnMsg( 1, "Garbage at input (%x)", i_startcode );
            }

            while( (i_startcode & 0xFFFFFF00) != 0x100L )
            {
                i_startcode <<= 8;
                if( (i_nb = read( p_input->i_handle, &i_dummy, 1 )) != 0 )
                {
                    i_startcode |= i_dummy;
                }
                else
                {
                    return( 1 );
                }
            }

            /* Packet found. */
            *(u32 *)p_header = U32_AT(&i_startcode);
            if( (i_error = SafeRead( p_input, p_header + 4, 2 )) )
            {
                return( i_error );
            }
        }

        if( U32_AT(p_header) != 0x1BA )
        {
            /* That's the case for all packets, except pack header. */
            i_packet_size = U16_AT(&p_header[4]);
        }
        else
        {
            /* Pack header. */
            if( (p_header[4] & 0xC0) == 0x40 )
            {
                /* MPEG-2 */
                i_packet_size = 8;
            }
            else if( (p_header[4] & 0xF0) == 0x20 )
            {
                /* MPEG-1 */
                i_packet_size = 6;
            }
            else
            {
                intf_ErrMsg( "Unable to determine stream type" );
                return( -1 );
            }
        }

        /* Fetch a packet of the appropriate size. */
        if( (p_data = NewPacket( p_input, i_packet_size + 6 )) == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            return( -1 );
        }

        /* Copy the header we already read. */
        memcpy( p_data->p_buffer, p_header, 6 );

        /* Read the remaining of the packet. */
        if( i_packet_size && (i_error =
                SafeRead( p_input, p_data->p_buffer + 6, i_packet_size )) )
        {
            return( i_error );
        }

        /* In MPEG-2 pack headers we still have to read stuffing bytes. */
        if( U32_AT(p_header) == 0x1BA )
        {
            if( i_packet_size == 8 && (p_data->p_buffer[13] & 0x7) != 0 )
            {
                /* MPEG-2 stuffing bytes */
                byte_t      p_garbage[8];
                if( (i_error = SafeRead( p_input, p_garbage,
                                         p_data->p_buffer[13] & 0x7)) )
                {
                    return( i_error );
                }
            }
        }

        /* Give the packet to the other input stages. */
        pp_packets[i_packet] = p_data;
    }

    return( 0 );
}


/*****************************************************************************
 * DVDRewind : reads a stream backward
 *****************************************************************************/
static int DVDRewind( input_thread_t * p_input )
{
    return( -1 );
}

/*****************************************************************************
 * DVDSeek : Goes to a given position on the stream ; this one is used by the 
 * input and translate chronological position from input to logical postion
 * on the device
 *****************************************************************************/
static int DVDSeek( input_thread_t * p_input, off_t i_off )
{
    return( -1 );
}

/*
 * Packet management utilities
 */

/*****************************************************************************
 * NewPacket: allocates a data packet
 *****************************************************************************/
static struct data_packet_s * NewPacket( void * p_garbage,
                                         size_t i_size )
{
    data_packet_t * p_data;

    /* Safety check */
    if( i_size > INPUT_MAX_PACKET_SIZE )
    {
        intf_ErrMsg( "Packet too big (%d)", i_size );
        return NULL;
    }

    if( (p_data = (data_packet_t *)malloc( sizeof(data_packet_t) )) == NULL )
    {
        intf_DbgMsg( "Out of memory" );
        return NULL;
    }

    if( (p_data->p_buffer = (byte_t *)malloc( i_size )) == NULL )
    {
        intf_DbgMsg( "Out of memory" );
        free( p_data );
        return NULL;
    }

    /* Initialize data */
    p_data->p_next = NULL;
    p_data->b_discard_payload = 0;

    p_data->p_payload_start = p_data->p_buffer;
    p_data->p_payload_end = p_data->p_buffer + i_size;

    return( p_data );
}

/*****************************************************************************
 * NewPES: allocates a pes packet
 *****************************************************************************/
static pes_packet_t * NewPES( void * p_garbage )
{
    pes_packet_t * p_pes;

    if( (p_pes = (pes_packet_t *)malloc( sizeof(pes_packet_t) )) == NULL )
    {
        intf_DbgMsg( "Out of memory" );
        return NULL;
    }

    p_pes->b_messed_up = p_pes->b_data_alignment = p_pes->b_discontinuity =
        p_pes->i_pts = p_pes->i_dts = 0;
    p_pes->i_pes_size = 0;
    p_pes->p_first = NULL;

    return( p_pes );
}

/*****************************************************************************
 * DeletePacket: deletes a data packet
 *****************************************************************************/
static void DeletePacket( void * p_garbage,
                          data_packet_t * p_data )
{
    ASSERT(p_data);
    ASSERT(p_data->p_buffer);
    free( p_data->p_buffer );
    free( p_data );
}

/*****************************************************************************
 * DeletePES: deletes a PES packet and associated data packets
 *****************************************************************************/
static void DeletePES( void * p_garbage, pes_packet_t * p_pes )
{
    data_packet_t *     p_data;
    data_packet_t *     p_next;

    p_data = p_pes->p_first;

    while( p_data != NULL )
    {
        p_next = p_data->p_next;
        free( p_data->p_buffer );
        free( p_data );
        p_data = p_next;
    }

    free( p_pes );
}

/*****************************************************************************
 * DVDKludge: fakes a DVD plugin (FIXME)
 *****************************************************************************/
input_capabilities_t * DVDKludge( void )
{
    input_capabilities_t *  p_plugin;

    p_plugin = (input_capabilities_t *)malloc( sizeof(input_capabilities_t) );
    p_plugin->pf_probe = DVDProbe;
    p_plugin->pf_init = DVDInit;
    p_plugin->pf_end = DVDEnd;
    p_plugin->pf_read = DVDRead;
    p_plugin->pf_demux = input_DemuxPS; /* FIXME: use i_p_config_t ! */
    p_plugin->pf_new_packet = NewPacket;
    p_plugin->pf_new_pes = NewPES;
    p_plugin->pf_delete_packet = DeletePacket;
    p_plugin->pf_delete_pes = DeletePES;
    p_plugin->pf_rewind = DVDRewind;
    p_plugin->pf_seek = DVDSeek;

    return( p_plugin );
}
