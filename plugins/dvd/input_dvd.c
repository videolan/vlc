/*****************************************************************************
 * input_dvd.c: DVD raw reading plugin.
 * ---
 * This plugins should handle all the known specificities of the DVD format,
 * especially the 2048 bytes logical block size.
 * It depends on:
 *  -input_netlist used to read packets
 *  -dvd_ifo for ifo parsing and analyse
 *  -dvd_css for unscrambling
 *  -dvd_udf to find files
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_dvd.c,v 1.9 2001/02/13 10:08:51 stef Exp $
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
#include <sys/uio.h>

#include <string.h>
#include <errno.h>
#include <malloc.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "intf_msg.h"

#include "main.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"
#include "input_netlist.h"

#include "dvd_ifo.h"
#include "dvd_css.h"
#include "input_dvd.h"
#include "mpeg_system.h"

#include "debug.h"

#include "modules.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DVDProbe    ( probedata_t *p_data );
static int  DVDCheckCSS ( struct input_thread_s * );
static int  DVDRead     ( struct input_thread_s *, data_packet_t ** );
static void DVDInit     ( struct input_thread_s * );
static void DVDEnd      ( struct input_thread_s * );
static void DVDSeek     ( struct input_thread_s *, off_t );
static int  DVDRewind   ( struct input_thread_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = DVDProbe;
    input.pf_init             = DVDInit;
    input.pf_open             = input_FileOpen;
    input.pf_close            = input_FileClose;
    input.pf_end              = DVDEnd;
    input.pf_read             = DVDRead;
    input.pf_demux            = input_DemuxPS;
    input.pf_new_packet       = input_NetlistNewPacket;
    input.pf_new_pes          = input_NetlistNewPES;
    input.pf_delete_packet    = input_NetlistDeletePacket;
    input.pf_delete_pes       = input_NetlistDeletePES;
    input.pf_rewind           = DVDRewind;
    input.pf_seek             = DVDSeek;
#undef input
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * DVDProbe: verifies that the stream is a PS stream
 *****************************************************************************/
static int DVDProbe( probedata_t *p_data )
{
    if( TestMethod( INPUT_METHOD_VAR, "dvd" ) )
    {
        return( 999 );
    }

    return 5;
}

/*****************************************************************************
 * DVDCheckCSS: check the stream
 *****************************************************************************/
static int DVDCheckCSS( input_thread_t * p_input )
{
#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    return CSSTest( p_input->i_handle );
#else
    /* DVD ioctls unavailable.
     * FIXME: Check the stream to see whether it is encrypted or not 
     * to give and accurate error message */
    return 0;
#endif
}

/*****************************************************************************
 * DVDInit: initializes DVD structures
 *****************************************************************************/
static void DVDInit( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_method;
    off_t                i_start;

    if( (p_method = malloc( sizeof(thread_dvd_data_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_method;
    p_input->p_method_data = NULL;

    p_method->i_fd = p_input->i_handle;
    /* FIXME: read several packets once */
    p_method->i_read_once = 1; 
    p_method->i_title = 0;
    p_method->b_encrypted = DVDCheckCSS( p_input );

    lseek( p_input->i_handle, 0, SEEK_SET );

    /* Reading structures initialisation */
    input_NetlistInit( p_input, 4096, 4096, DVD_LB_SIZE,
                       p_method->i_read_once ); 

    /* Ifo initialisation */
    p_method->ifo = IfoInit( p_input->i_handle );

    /* CSS initialisation */
    if( p_method->b_encrypted )
    {

#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
        p_method->css = CSSInit( p_input->i_handle );

        if( ( p_input->b_error = p_method->css.b_error ) )
        {
            intf_ErrMsg( "CSS fatal error" );
            return;
        }
#else
        intf_ErrMsg( "Unscrambling not supported" );
        p_input->b_error = 1;
        return;
#endif
    }

    /* Ifo structures reading */
    IfoRead( &(p_method->ifo) );
    intf_WarnMsg( 3, "Ifo: Initialized" );

    /* CSS title keys */
    if( p_method->b_encrypted )
    {

#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
        int   i;

        p_method->css.i_title_nb = p_method->ifo.vmg.mat.i_tts_nb;

        if( (p_method->css.p_title_key =
            malloc( p_method->css.i_title_nb *sizeof(title_key_t) ) ) == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            p_input->b_error = 1;
            return;
        }

        for( i=0 ; i<p_method->css.i_title_nb ; i++ )
        {
            p_method->css.p_title_key[i].i =
                p_method->ifo.p_vts[i].i_pos +
                p_method->ifo.p_vts[i].mat.i_tt_vobs_ssector * DVD_LB_SIZE;
        }

        CSSGetKeys( &(p_method->css) );

        intf_WarnMsg( 3, "CSS: initialized" );
#else
        intf_ErrMsg( "Unscrambling not supported" );
        p_input->b_error = 1;
        return;
#endif
    }

    /* FIXME: Kludge beginning of vts_01_1.vob */
    i_start = p_method->ifo.p_vts[0].i_pos +
              p_method->ifo.p_vts[0].mat.i_tt_vobs_ssector *DVD_LB_SIZE;

    i_start = lseek( p_input->i_handle, i_start, SEEK_SET );
    intf_WarnMsg( 3, "DVD: VOB start at : %lld", i_start );

    /* Initialize ES structures */
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
                input_NetlistDeletePacket( p_input->p_method_data,
                                           pp_packets[i] );
            }

            /* File too big. */
            if( p_input->stream.i_tell > INPUT_PREPARSE_LENGTH )
            {
                break;
            }
        }
        lseek( p_input->i_handle, i_start, SEEK_SET );
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.i_tell = i_start;
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
                        if( main_GetIntVariable( INPUT_CHANNEL_VAR, 0 )
                                == (p_es->i_id & 0x1F) )
                        switch( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) )
                        {
                        case 0:
                            main_PutIntVariable( INPUT_AUDIO_VAR,
                                                 REQUESTED_MPEG );
                        case REQUESTED_MPEG:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case AC3_AUDIO_ES:
                        if( main_GetIntVariable( INPUT_CHANNEL_VAR, 0 )
                                == ((p_es->i_id & 0xF00) >> 8) )
                        switch( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) )
                        {
                        case 0:
                            main_PutIntVariable( INPUT_AUDIO_VAR,
                                                 REQUESTED_AC3 );
                        case REQUESTED_AC3:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case DVD_SPU_ES:
                        if( main_GetIntVariable( INPUT_SUBTITLE_VAR, -1 )
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

        /* FIXME: kludge to implement file size */
        p_input->stream.i_size = 
         (off_t)( p_method->ifo.vmg.ptt_srpt.p_tts[1].i_ssector -
                  p_method->ifo.p_vts[0].mat.i_tt_vobs_ssector ) *DVD_LB_SIZE;
        intf_WarnMsg( 3, "DVD: stream size: %lld", p_input->stream.i_size );


        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else
    {
        /* The programs will be added when we read them. */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.pp_programs[0]->b_is_ok = 0;

        /* FIXME: kludge to implement file size */
        p_input->stream.i_size = 
            ( p_method->ifo.vmg.ptt_srpt.p_tts[1].i_ssector -
              p_method->ifo.p_vts[0].mat.i_tt_vobs_ssector ) *DVD_LB_SIZE;
        intf_WarnMsg( 3, "DVD: stream size: %lld", p_input->stream.i_size );

        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

}

/*****************************************************************************
 * DVDEnd: frees unused data
 *****************************************************************************/
static void DVDEnd( input_thread_t * p_input )
{
    /* FIXME: check order of calls */
//    CSSEnd( p_input );
//    IfoEnd( (ifo_t*)(&p_input->p_plugin_data->ifo ) );
    free( p_input->stream.p_demux_data );
    free( p_input->p_plugin_data );
    input_NetlistEnd( p_input );
}

/*****************************************************************************
 * DVDRead: reads data packets into the netlist.
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int DVDRead( input_thread_t * p_input,
                   data_packet_t ** pp_packets )
{
    thread_dvd_data_t *     p_method;
    netlist_t *             p_netlist;
    struct iovec *          p_vec;
    struct data_packet_s *  p_data;
    u8 *                    pi_cur;
    int                     i_packet_size;
    int                     i_packet;
    int                     i_pos;
    int                     i;
    boolean_t               b_first_packet;

    p_method = ( thread_dvd_data_t * ) p_input->p_plugin_data;
    p_netlist = ( netlist_t * ) p_input->p_method_data;

    /* Get an iovec pointer */
    if( ( p_vec = input_NetlistGetiovec( p_netlist, &p_data ) ) == NULL )
    {
        intf_ErrMsg( "DVD: read error" );
        return -1;
    }

    /* Reads from DVD */
    readv( p_input->i_handle, p_vec, p_method->i_read_once );

#if defined( HAVE_SYS_DVDIO_H ) || defined( LINUX_DVD )
    if( p_method->b_encrypted )
    {
        for( i=0 ; i<p_method->i_read_once ; i++ )
        {
            CSSDescrambleSector(
                        p_method->css.p_title_key[p_method->i_title].key, 
                        p_vec[i].iov_base );
            ((u8*)(p_vec[i].iov_base))[0x14] &= 0x8F;
        }
    }
#endif

    /* Update netlist indexes */
    input_NetlistMviovec( p_netlist, p_method->i_read_once );

    i_packet = 0;
    /* Read headers to compute payload length */
    for( i = 0 ; i < p_method->i_read_once ; i++ )
    {
        i_pos = 0;
        b_first_packet = 1;
        while( i_pos < p_netlist->i_buffer_size )
        {
            pi_cur = (u8*)(p_vec[i].iov_base + i_pos);
            /*default header */
            if( U32_AT( pi_cur ) != 0x1BA )
            {
                /* That's the case for all packets, except pack header. */
                i_packet_size = U16_AT( pi_cur + 4 );
            }
            else
            {
                /* Pack header. */
                if( ( pi_cur[4] & 0xC0 ) == 0x40 )
                {
                    /* MPEG-2 */
                    i_packet_size = 8;
                }
                else if( ( pi_cur[4] & 0xF0 ) == 0x20 )
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
            if( b_first_packet )
            {
                p_data->b_discard_payload = 0;
                b_first_packet = 0;
            }
            else
            { 
                p_data = input_NetlistNewPacket( p_netlist ,
                                                 i_packet_size + 6 );
                memcpy( p_data->p_buffer,
                        p_vec[i].iov_base + i_pos , i_packet_size + 6 );
            }

            p_data->p_payload_end = p_data->p_payload_start + i_packet_size + 6;
            pp_packets[i_packet] = p_data;
            i_packet++;
            i_pos += i_packet_size + 6;
        }
    }
    pp_packets[i_packet] = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.i_tell += p_method->i_read_once *DVD_LB_SIZE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

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
static void DVDSeek( input_thread_t * p_input, off_t i_off )
{
    thread_dvd_data_t *     p_method;
    off_t                   i_pos;
    
    p_method = ( thread_dvd_data_t * )p_input->p_plugin_data;

    /* We have to take care of offset of beginning of title */
    i_pos = i_off - ( p_method->ifo.p_vts[0].i_pos +
              p_method->ifo.p_vts[0].mat.i_tt_vobs_ssector *DVD_LB_SIZE );

    /* With DVD, we have to be on a sector boundary */
    i_pos = i_pos & (~0x7ff);

    i_pos = lseek( p_input->i_handle, i_pos, SEEK_SET );

    p_input->stream.i_tell = i_pos;

    return;
}
