/*****************************************************************************
 * input.c: input thread
 * Read an MPEG2 stream, demultiplex and parse it before sending it to
 * decoders.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors:
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

#include <errno.h>                                                  /* errno */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <string.h>                                            /* strerror() */

#include <stdlib.h>                                                /* free() */
#include <netinet/in.h>                                           /* ntohs() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"
#include "plugins.h"
#include "debug.h"

#include "input.h"
#include "input_psi.h"
#include "input_pcr.h"
#include "input_netlist.h"
#include "decoder_fifo.h"
#include "input_file.h"
#include "input_network.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "audio_decoder.h"        /* audiodec_t (for audio_decoder_thread.h) */
#include "audio_decoder_thread.h"                           /* adec_thread_t */

#include "ac3_decoder.h"              /* ac3dec_t (for ac3_decoder_thread.h) */
#include "ac3_decoder_thread.h"                           /* ac3dec_thread_t */

#include "lpcm_decoder.h"
#include "lpcm_decoder_thread.h"

#include "video.h"                          /* picture_t (for video_output.h) */
#include "video_output.h"                                   /* vout_thread_t */

#include "vdec_idct.h"                     /* dctelem_t (for video_parser.h) */
#include "vdec_motion.h"                  /* f_motion_t (for video_parser.h) */
#include "vpar_blocks.h"                /* macroblock_t (for video_parser.h) */
#include "vpar_headers.h"                 /* sequence_t (for video_parser.h) */
#include "vpar_synchro.h"            /* video_synchro_t (for video_parser.h) */
#include "video_parser.h"                                   /* vpar_thread_t */

#include "spu_decoder.h"                                  /* spudec_thread_t */

#include "main.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread   ( input_thread_t *p_input );
static void ErrorThread ( input_thread_t *p_input );
static void EndThread   ( input_thread_t *p_input );

static __inline__ int   input_ReadPacket( input_thread_t *p_input );
static __inline__ void  input_SortPacket( input_thread_t *p_input,
                                          ts_packet_t *ts_packet );
static __inline__ void  input_DemuxTS( input_thread_t *p_input,
                                       ts_packet_t *ts_packet,
                                       es_descriptor_t *es_descriptor );
static __inline__ void  input_DemuxPES( input_thread_t *p_input,
                                        ts_packet_t *ts_packet,
                                        es_descriptor_t *p_es_descriptor,
                                        boolean_t b_unit_start, boolean_t b_packet_lost );
static __inline__ void  input_ParsePES( input_thread_t *p_input,
                                        es_descriptor_t *p_es_descriptor );
static __inline__ void  input_DemuxPSI( input_thread_t *p_input,
                                        ts_packet_t *ts_packet,
                                        es_descriptor_t *p_es_descriptor,
                                        boolean_t b_unit_start, boolean_t b_packet_lost );

/*****************************************************************************
 * input_CreateThread: creates a new input thread
 *****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *****************************************************************************/
input_thread_t *input_CreateThread ( int i_method, void *p_source, int i_port, int i_vlan,
                                     p_vout_thread_t p_vout, p_aout_thread_t p_aout, int *pi_status )
{
    input_thread_t *    p_input;                        /* thread descriptor */
    int                 i_status;                           /* thread status */
    int                 i_index;          /* index for tables initialization */

    /* Allocate descriptor */
    intf_DbgMsg("\n");
    p_input = (input_thread_t *)malloc( sizeof(input_thread_t) );
    if( p_input == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( NULL );
    }

    /* Initialize thread properties */
    p_input->b_die              = 0;
    p_input->b_error            = 0;
    p_input->pi_status          = (pi_status != NULL) ? pi_status : &i_status;
    *p_input->pi_status         = THREAD_CREATE;

    /* Initialize input method description */
    p_input->i_method           = i_method;
    p_input->p_source           = p_source;
    p_input->i_port             = i_port;
    p_input->i_vlan             = i_vlan;
    switch( i_method )
    {
    case INPUT_METHOD_TS_FILE:                               /* file methods */
        p_input->p_Open =   input_FileOpen;
        p_input->p_Read =   input_FileRead;
        p_input->p_Close =  input_FileClose;
        break;
    case INPUT_METHOD_TS_VLAN_BCAST:                  /* vlan network method */
        if( !p_main->b_vlans )
        {
            intf_ErrMsg("error: vlans are not activated\n");
            free( p_input );
            return( NULL );
        }
        /* ... pass through */
    case INPUT_METHOD_TS_UCAST:                           /* network methods */
    case INPUT_METHOD_TS_MCAST:
    case INPUT_METHOD_TS_BCAST:
        p_input->p_Open =   input_NetworkOpen;
        p_input->p_Read =   input_NetworkRead;
        p_input->p_Close =  input_NetworkClose;
        break;
    default:
        intf_ErrMsg("error: unknow input method\n");
        free( p_input );
        return( NULL );
        break;
    }

    /* Initialize stream description */
    for( i_index = 0; i_index < INPUT_MAX_ES; i_index++ )
    {
        p_input->p_es[i_index].i_id = EMPTY_PID;
        p_input->pp_selected_es[i_index] = NULL;
    }

    /* Initialize default settings for spawned decoders */
    p_input->p_aout                     = p_aout;
    p_input->p_vout                     = p_vout;

#ifdef STATS
    /* Initialize statistics */
    p_input->c_loops                    = 0;
    p_input->c_bytes                    = 0;
    p_input->c_payload_bytes            = 0;
    p_input->c_packets_read             = 0;
    p_input->c_packets_trashed          = 0;
#endif

    /* Initialize PSI and PCR decoders */
    if( input_PsiInit( p_input ) )
    {
        free( p_input );
        return( NULL );
    }

    if( input_PcrInit( p_input ) )
    {
        input_PsiEnd( p_input );
        free( p_input );
        return( NULL );
    }

    /* Initialize netlists */
    if( input_NetlistInit( p_input ) )
    {
        input_PsiEnd( p_input );
        input_PcrEnd( p_input );
        free( p_input );
        return( NULL );
    }

    intf_DbgMsg("configuration: method=%d, source=%s, port=%d, vlan=%d\n",
                i_method, p_source, i_port, i_vlan );

    /* Let the appropriate method open the socket. */
    if( p_input->p_Open( p_input ) )
    {
        input_NetlistEnd( p_input );
        input_PsiEnd( p_input );
        input_PcrEnd( p_input );
        free( p_input );
        return( NULL );
    }

    /* Create thread and set locks. */
    vlc_mutex_init( &p_input->netlist.lock );
    vlc_mutex_init( &p_input->programs_lock );
    vlc_mutex_init( &p_input->es_lock );
    if( vlc_thread_create(&p_input->thread_id, "input", (void *) RunThread, (void *) p_input) )
    {
        intf_ErrMsg("error: %s\n", strerror(errno) );
        p_input->p_Close( p_input );
        input_NetlistEnd( p_input );;
        input_PsiEnd( p_input );
        input_PcrEnd( p_input );
        free( p_input );
        return( NULL );
    }

    intf_Msg("Input initialized\n");

    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
        if( i_status != THREAD_READY )
        {
            return( NULL );
        }
    }
    return( p_input );
}

/*****************************************************************************
 * input_DestroyThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_DestroyThread( input_thread_t *p_input, int *pi_status )
{
    int         i_status;                                   /* thread status */

    /* Set status */
    p_input->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_input->pi_status = THREAD_DESTROY;

    /* Request thread destruction */
    p_input->b_die = 1;

    /* If status is NULL, wait until thread has been destroyed */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
    }
}

#if 0
/*****************************************************************************
 * input_OpenAudioStream: open an audio stream
 *****************************************************************************
 * This function spawns an audio decoder and plugs it on the audio output
 * thread.
 *****************************************************************************/
int input_OpenAudioStream( input_thread_t *p_input, int i_id )
{
    /* XXX?? */
}

/*****************************************************************************
 * input_CloseAudioStream: close an audio stream
 *****************************************************************************
 * This function destroys an audio decoder.
 *****************************************************************************/
void input_CloseAudioStream( input_thread_t *p_input, int i_id )
{
    /* XXX?? */
}

/*****************************************************************************
 * input_OpenVideoStream: open a video stream
 *****************************************************************************
 * This function spawns a video decoder and plugs it on a video output thread.
 *****************************************************************************/
int input_OpenVideoStream( input_thread_t *p_input,
                           struct vout_thread_s *p_vout, struct video_cfg_s * p_cfg )
{
    /* XXX?? */
}

/*****************************************************************************
 * input_CloseVideoStream: close a video stream
 *****************************************************************************
 * This function destroys an video decoder.
 *****************************************************************************/
void input_CloseVideoStream( input_thread_t *p_input, int i_id )
{
    /* XXX?? */
}
#endif

/* following functions are local */

/*****************************************************************************
 * InitThread: initialize input thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( input_thread_t *p_input )
{
    /* Mark thread as running and return */
    intf_DbgMsg("\n");
    *p_input->pi_status =        THREAD_READY;
    intf_DbgMsg("thread ready\n");
    return( 0 );
}

/*****************************************************************************
 * RunThread: main thread loop
 *****************************************************************************
 * Thread in charge of processing the network packets and demultiplexing.
 *****************************************************************************/
static void RunThread( input_thread_t *p_input )
{
    /*
     * Initialize thread and free configuration
     */
    p_input->b_error = InitThread( p_input );
    if( p_input->b_error )
    {
        free( p_input );                               /* destroy descriptor */
        return;
    }

    /*
     * Main loop
     */
    intf_DbgMsg("\n");
    while( !p_input->b_die && !p_input->b_error )
    {
        /* Scatter read the UDP packet from the network or the file. */
        if( (input_ReadPacket( p_input )) == (-1) )
        {
            /* FIXME??: Normally, a thread can't kill itself, but we don't have
             * any method in case of an error condition ... */
            p_input->b_error = 1;
        }

#ifdef STATS
        p_input->c_loops++;
#endif
    }

    /*
     * Error loop
     */
    if( p_input->b_error )
    {
        ErrorThread( p_input );
    }

    /* End of thread */
    EndThread( p_input );
    intf_DbgMsg("thread end\n");
}


/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop.
 *****************************************************************************/
static void ErrorThread( input_thread_t *p_input )
{
    /* Wait until a `die' order */
    intf_DbgMsg("\n");
    while( !p_input->b_die )
    {
        /* Sleep a while */
        msleep( VOUT_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * EndThread: end the input thread
 *****************************************************************************/
static void EndThread( input_thread_t * p_input )
{
    int *       pi_status;                                  /* threas status */
    int         i_es_loop;                                       /* es index */

    /* Store status */
    intf_DbgMsg("\n");
    pi_status = p_input->pi_status;
    *pi_status = THREAD_END;

    /* Close input method */
    p_input->p_Close( p_input );

    /* Destroy all decoder threads */
    for( i_es_loop = 0;
         (i_es_loop < INPUT_MAX_ES) && (p_input->pp_selected_es[i_es_loop] != NULL) ;
         i_es_loop++ )
    {
        switch( p_input->pp_selected_es[i_es_loop]->i_type )
        {
        case MPEG1_VIDEO_ES:
        case MPEG2_VIDEO_ES:
            vpar_DestroyThread( (vpar_thread_t*)(p_input->pp_selected_es[i_es_loop]->p_dec) /*, NULL */ );
            break;
        case MPEG1_AUDIO_ES:
        case MPEG2_AUDIO_ES:
            adec_DestroyThread( (adec_thread_t*)(p_input->pp_selected_es[i_es_loop]->p_dec) );
            break;
        case AC3_AUDIO_ES:
            ac3dec_DestroyThread( (ac3dec_thread_t *)(p_input->pp_selected_es[i_es_loop]->p_dec) );
            break;
        case LPCM_AUDIO_ES:
            lpcmdec_DestroyThread((lpcmdec_thread_t *)(p_input->pp_selected_es[i_es_loop]->p_dec) );
            break;
        case DVD_SPU_ES:
            spudec_DestroyThread( (spudec_thread_t *)(p_input->pp_selected_es[i_es_loop]->p_dec) );
            break;
        case 0:
            /* Special streams for the PSI decoder, PID 0 and 1 */
            break;
#ifdef DEBUG
        default:
            intf_DbgMsg("error: unknown decoder type %d\n", p_input->pp_selected_es[i_es_loop]->i_type );
            break;
#endif
        }
    }

    input_NetlistEnd( p_input );                            /* clean netlist */
    input_PsiEnd( p_input );                        /* clean PSI information */
    input_PcrEnd( p_input );                        /* clean PCR information */
    free( p_input );                          /* free input_thread structure */

    /* Update status */
    *pi_status = THREAD_OVER;
}

/*****************************************************************************
 * input_ReadPacket: reads a packet from the network or the file
 *****************************************************************************/
static __inline__ int input_ReadPacket( input_thread_t *p_input )
{
    int                 i_base_index; /* index of the first free iovec */
    int                 i_current_index;
    int                 i_packet_size;
#ifdef INPUT_LIFO_TS_NETLIST
    int                 i_meanwhile_released;
    int                 i_currently_removed;
#endif
    ts_packet_t *       p_ts_packet;

    /* In this function, we only care about the TS netlist. PES netlist
     * is for the demultiplexer. */
#ifdef INPUT_LIFO_TS_NETLIST
    i_base_index = p_input->netlist.i_ts_index;

    /* Verify that we still have packets in the TS netlist */
    if( (INPUT_MAX_TS + INPUT_TS_READ_ONCE - 1 - p_input->netlist.i_ts_index) <= INPUT_TS_READ_ONCE )
    {
        intf_ErrMsg("input error: TS netlist is empty !\n");
        return( -1 );
    }

#else /* FIFO netlist */
    i_base_index = p_input->netlist.i_ts_start;
    if( p_input->netlist.i_ts_start + INPUT_TS_READ_ONCE -1 > INPUT_MAX_TS )
    {
        /* The netlist is splitted in 2 parts. We must gather them to consolidate
           the FIFO (we make the loop easily in having the same iovec at the far
           end and in the beginning of netlist_free).
           That's why the netlist is (INPUT_MAX_TS +1) + (INPUT_TS_READ_ONCE -1)
           large. */
        memcpy( p_input->netlist.p_ts_free + INPUT_MAX_TS + 1,
                p_input->netlist.p_ts_free,
                (p_input->netlist.i_ts_start + INPUT_TS_READ_ONCE - 1 - INPUT_MAX_TS)
                  * sizeof(struct iovec) );
    }

    /* Verify that we still have packets in the TS netlist */
    if( ((p_input->netlist.i_ts_end -1 - p_input->netlist.i_ts_start) & INPUT_MAX_TS) <= INPUT_TS_READ_ONCE )
    {
        intf_ErrMsg("input error: TS netlist is empty !\n");
        return( -1 );
    }
#endif /* FIFO netlist */

    /* Scatter read the buffer. */
    i_packet_size = (*p_input->p_Read)( p_input,
                           &p_input->netlist.p_ts_free[i_base_index],
                           INPUT_TS_READ_ONCE );
    if( i_packet_size == (-1) )
    {
#if 0
        intf_DbgMsg("Read packet %d %p %d %d\n", i_base_index,
                    &p_input->netlist.p_ts_free[i_base_index],
                    p_input->netlist.i_ts_start,
                    p_input->netlist.i_ts_end);
#endif
        intf_ErrMsg("input error: readv() failed (%s)\n", strerror(errno));
        return( -1 );
    }

    if( i_packet_size == 0 )
    {
        /* No packet has been received, so stop here. */
        return( 0 );
    }

    /* Demultiplex the TS packets (1..INPUT_TS_READ_ONCE) received. */
    for( i_current_index = i_base_index;
         (i_packet_size -= TS_PACKET_SIZE) >= 0;
         i_current_index++ )
    {
        /* BTW, something REALLY bad could happen if we receive packets with
           a wrong size. */
        p_ts_packet = (ts_packet_t*)(p_input->netlist.p_ts_free[i_current_index].iov_base);
        /* Don't cry :-), we are allowed to do that cast, because initially,
           our buffer was malloc'ed with sizeof(ts_packet_t) */

        /* Find out if we need this packet and demultiplex. */
        input_SortPacket( p_input /* for current PIDs and netlist */,
                          p_ts_packet);
    }

    if( i_packet_size > 0 )
    {
        intf_ErrMsg("input error: wrong size\n");
        return( -1 );
    }

    /* Remove the TS packets we have just filled from the netlist */
#ifdef INPUT_LIFO_TS_NETLIST
    /* We need to take a lock here while we're calculating index positions. */
    vlc_mutex_lock( &p_input->netlist.lock );

    i_meanwhile_released = i_base_index - p_input->netlist.i_ts_index;
    if( i_meanwhile_released )
    {
        /* That's where it becomes funny :-). Since we didn't take locks for
           efficiency reasons, other threads (including ourselves, with
           input_DemuxPacket) might have released packets to the netlist.
           So we have to copy these iovec where they should go.

           BTW, that explains why the TS netlist is
           (INPUT_MAX_TS +1) + (TS_READ_ONCE -1) large. */

        i_currently_removed = i_current_index - i_base_index;
        if( i_meanwhile_released < i_currently_removed )
        {
            /* Copy all iovecs in that case */
            memcpy( &p_input->netlist.p_ts_free[p_input->netlist.i_ts_index]
                     + i_currently_removed,
                    &p_input->netlist.p_ts_free[p_input->netlist.i_ts_index],
                    i_meanwhile_released * sizeof(struct iovec) );
        }
        else
        {
            /* We have fewer places than items, so we only move
               i_currently_removed of them. */
            memcpy( &p_input->netlist.p_ts_free[i_base_index],
                    &p_input->netlist.p_ts_free[p_input->netlist.i_ts_index],
                    i_currently_removed * sizeof(struct iovec) );
        }

        /* Update i_netlist_index with the information gathered above. */
        p_input->netlist.i_ts_index += i_currently_removed;
    }
    else
    {
        /* Nothing happened. */
        p_input->netlist.i_ts_index = i_current_index;
    }

    vlc_mutex_unlock( &p_input->netlist.lock );

#else /* FIFO netlist */
    /* & is modulo ; that's where we make the loop. */
    p_input->netlist.i_ts_start = i_current_index & INPUT_MAX_TS;
#endif

#ifdef STATS
    p_input->c_packets_read += i_current_index - i_base_index;
    p_input->c_bytes += (i_current_index - i_base_index) * TS_PACKET_SIZE;
#endif
    return( 0 );
}

/*****************************************************************************
 * input_SortPacket: find out whether we need that packet
 *****************************************************************************/
static __inline__ void input_SortPacket( input_thread_t *p_input,
                                         ts_packet_t *p_ts_packet )
{
    int             i_current_pid;
    int             i_es_loop;

    /* Verify that sync_byte, error_indicator and scrambling_control are
       what we expected. */
    if( !(p_ts_packet->buffer[0] == 0x47) || (p_ts_packet->buffer[1] & 0x80) ||
        (p_ts_packet->buffer[3] & 0xc0) )
    {
        intf_DbgMsg("input debug: invalid TS header (%p)\n", p_ts_packet);
    }
    else
    {
        /* Get the PID of the packet. Note that ntohs is needed, for endianness
           purposes (see man page). */
        i_current_pid = U16_AT(&p_ts_packet->buffer[1]) & 0x1fff;

        //intf_DbgMsg("input debug: pid %d received (%p)\n",
        //            i_current_pid, p_ts_packet);

        /* Lock current ES state. */
        vlc_mutex_lock( &p_input->es_lock );

    /* Verify that we actually want this PID. */
        for( i_es_loop = 0; i_es_loop < INPUT_MAX_SELECTED_ES; i_es_loop++ )
        {
            if( p_input->pp_selected_es[i_es_loop] != NULL)
            {
                if( (*p_input->pp_selected_es[i_es_loop]).i_id
                     == i_current_pid )
                {
                    /* Don't need the lock anymore, since the value pointed
                       out by p_input->pp_selected_es[i_es_loop] can only be
                       modified from inside the input_thread (by the PSI
                       decoder): interface thread is only allowed to modify
                       the pp_selected_es table */
                    vlc_mutex_unlock( &p_input->es_lock );

                    /* We're interested. Pass it to the demultiplexer. */
                    input_DemuxTS( p_input, p_ts_packet,
                                   p_input->pp_selected_es[i_es_loop] );
                    return;
                }
            }
            else
            {
                /* pp_selected_es should not contain any hole. */
                break;
            }
        }
        vlc_mutex_unlock( &p_input->es_lock );
    }

    /* We weren't interested in receiving this packet. Give it back to the
       netlist. */
    //intf_DbgMsg("SortPacket: freeing unwanted TS %p (pid %d)\n", p_ts_packet,
    //                 U16_AT(&p_ts_packet->buffer[1]) & 0x1fff);
    input_NetlistFreeTS( p_input, p_ts_packet );
#ifdef STATS
    p_input->c_packets_trashed++;
#endif
}

/*****************************************************************************
 * input_DemuxTS: first step of demultiplexing: the TS header
 *****************************************************************************
 * Stream must also only contain PES and PSI, so PID must have been filtered
 *****************************************************************************/
static __inline__ void input_DemuxTS( input_thread_t *p_input,
                                      ts_packet_t *p_ts_packet,
                                      es_descriptor_t *p_es_descriptor )
{
    int         i_dummy;
    boolean_t   b_adaption;                     /* Adaption field is present */
    boolean_t   b_payload;                         /* Packet carries payload */
    boolean_t   b_unit_start;          /* A PSI or a PES start in the packet */
    boolean_t   b_trash = 0;                 /* Must the packet be trashed ? */
    boolean_t   b_lost = 0;                     /* Was there a packet lost ? */

    ASSERT(p_input);
    ASSERT(p_ts_packet);
    ASSERT(p_es_descriptor);

#define p (p_ts_packet->buffer)

    //intf_DbgMsg("input debug: TS-demultiplexing packet %p, pid %d, number %d\n",
    //            p_ts_packet, U16_AT(&p[1]) & 0x1fff, p[3] & 0x0f);

#ifdef STATS
    p_es_descriptor->c_packets++;
    p_es_descriptor->c_bytes += TS_PACKET_SIZE;
#endif

    /* Extract flags values from TS common header. */
    b_unit_start = (p[1] & 0x40);
    b_adaption = (p[3] & 0x20);
    b_payload = (p[3] & 0x10);

    /* Extract adaption field informations if any */
    if( !b_adaption )
    {
        /* We don't have any adaptation_field, so payload start immediately
           after the 4 byte TS header */
        p_ts_packet->i_payload_start = 4;
    }
    else
    {
        /* p[4] is adaptation_field_length minus one */
        p_ts_packet->i_payload_start = 5 + p[4];

        /* The adaption field can be limited to the adaptation_field_length byte,
           so that there is nothing to do: skip this possibility */
        if( p[4] )
        {
            /* If the packet has both adaptation_field and payload, adaptation_field
               cannot be more than 182 bytes long; if there is only an
               adaptation_field, it must fill the next 183 bytes. */
            if( b_payload ? (p[4] > 182) : (p[4] != 183) )
            {
                intf_DbgMsg("input debug: invalid TS adaptation field (%p)\n",
                            p_ts_packet);
#ifdef STATS
                p_es_descriptor->c_invalid_packets++;
#endif
                b_trash = 1;
            }

            /* No we are sure that the byte containing flags is present: read it */
            else
            {
                /* discontinuity_indicator */
                if( p[5] & 0x80 )
                {
                    intf_DbgMsg("discontinuity_indicator encountered by TS demux " \
                                "(position read: %d, saved: %d)\n", p[5] & 0x80,
                                p_es_descriptor->i_continuity_counter);

                    /* If the PID carries the PCR, there will be a system time-base
                       discontinuity. We let the PCR decoder handle that. */
                    p_es_descriptor->b_discontinuity = 1;

                    /* There also may be a continuity_counter discontinuity:
               resynchronise our counter with the one of the stream */
                    p_es_descriptor->i_continuity_counter = (p[3] & 0x0f) - 1;
                }

                /* random_access_indicator */
                p_es_descriptor->b_random |= p[5] & 0x40;

                /* If this is a PCR_PID, and this TS packet contains a PCR,
           we pass it along to the PCR decoder. */
                if( (p_es_descriptor->b_pcr) && (p[5] & 0x10) )
                {
                    /* There should be a PCR field in the packet, check if the
               adaption field is long enough to carry it */
                    if( p[4] >= 7 )
                    {
                        /* Call the PCR decoder */
                        input_PcrDecode( p_input, p_es_descriptor, &p[6] );
                    }
                }
            }
        }
    }

    /* Check the continuity of the stream. */
    i_dummy = ((p[3] & 0x0f) - p_es_descriptor->i_continuity_counter) & 0x0f;
    if( i_dummy == 1 )
    {
        /* Everything is ok, just increase our counter */
        p_es_descriptor->i_continuity_counter++;
    }
    else
    {
        if( !b_payload && i_dummy == 0 )
        {
            /* This is a packet without payload, this is allowed by the draft
               As there is nothing interesting in this packet (except PCR that
               have already been handled), we can trash the packet. */
            intf_DbgMsg("Packet without payload received by TS demux\n");
            b_trash = 1;
        }
        else if( i_dummy <= 0 )
        {
            /* Duplicate packet: mark it as being to be trashed. */
            intf_DbgMsg("Duplicate packet received by TS demux\n");
            b_trash = 1;
        }
        else if( p_es_descriptor->i_continuity_counter == 0xFF )
        {
            /* This means that the packet is the first one we receive for this
               ES since the continuity counter ranges between 0 and 0x0F
               excepts when it has been initialized by the input: Init the
               counter to the correct value. */
            intf_DbgMsg("First packet for PID %d received by TS demux\n",
                        p_es_descriptor->i_id);
            p_es_descriptor->i_continuity_counter = (p[3] & 0x0f);
        }
        else
        {
            /* This can indicate that we missed a packet or that the
               continuity_counter wrapped and we received a dup packet: as we
               don't know, do as if we missed a packet to be sure to recover
               from this situation */
            intf_DbgMsg("Packet lost by TS demux: current %d, packet %d\n",
                        p_es_descriptor->i_continuity_counter & 0x0f,
                        p[3] & 0x0f);
            b_lost = 1;
            p_es_descriptor->i_continuity_counter = p[3] & 0x0f;
        }
    }

    /* Trash the packet if it has no payload or if it is bad */
    if( b_trash )
    {
        input_NetlistFreeTS( p_input, p_ts_packet );
#ifdef STATS
        p_input->c_packets_trashed++;
#endif
    }
    else
    {
        if( p_es_descriptor->b_psi )
        {
            /* The payload contains PSI tables */
            input_DemuxPSI( p_input, p_ts_packet, p_es_descriptor,
                            b_unit_start, b_lost );
        }
        else
        {
            /* The payload carries a PES stream */
            input_DemuxPES( p_input, p_ts_packet, p_es_descriptor,
                            b_unit_start, b_lost );
        }
    }

#undef p
}




/*****************************************************************************
 * input_DemuxPES:
 *****************************************************************************
 * Gather a PES packet.
 *****************************************************************************/
static __inline__ void input_DemuxPES( input_thread_t *p_input,
                                       ts_packet_t *p_ts_packet,
                                       es_descriptor_t *p_es_descriptor,
                                       boolean_t b_unit_start,
                                       boolean_t b_packet_lost )
{
    int                         i_dummy;
    pes_packet_t*               p_last_pes;
    ts_packet_t *               p_ts;
    int                         i_ts_payload_size;


#define p_pes (p_es_descriptor->p_pes_packet)

    ASSERT(p_input);
    ASSERT(p_ts_packet);
    ASSERT(p_es_descriptor);

    //intf_DbgMsg("PES-demultiplexing %p (%p)\n", p_ts_packet, p_pes);

    /* If we lost data, discard the PES packet we are trying to reassemble
       if any and wait for the beginning of a new one in order to synchronise
       again */
    if( b_packet_lost && p_pes != NULL )
    {
        intf_DbgMsg("PES %p trashed because of packet lost\n", p_pes);
        input_NetlistFreePES( p_input, p_pes );
        p_pes = NULL;
    }

    /* If the TS packet contains the begining of a new PES packet, and if we
       were reassembling a PES packet, then the PES should be complete now,
       so parse its header and give it to the decoders */
    if( b_unit_start && p_pes != NULL )
    {
        /* Parse the header. The header has a variable length, but in order
           to improve the algorithm, we will read the 14 bytes we may be
           interested in */

        /* If this part of the header did not fit in the current TS packet,
           copy the part of the header we are interested in to the
           p_pes_header_save buffer. The buffer is dynamicly allocated if
           needed so it's time expensive but this situation almost never
           occurs. */
        p_ts = p_pes->p_first_ts;
        i_ts_payload_size = p_ts->i_payload_end - p_ts->i_payload_start;

        if(i_ts_payload_size < PES_HEADER_SIZE)
        {
            intf_DbgMsg("Code never tested encountered, WARNING ! (benny)\n");
            if( !p_pes->p_pes_header_save )
            {
                p_pes->p_pes_header_save = malloc(PES_HEADER_SIZE);
            }

            i_dummy = 0;
            do
            {
                memcpy(p_pes->p_pes_header_save + i_dummy,
                       &p_ts->buffer[p_ts->i_payload_start], i_ts_payload_size);
                i_dummy += i_ts_payload_size;

                p_ts = p_ts->p_next_ts;
                if(!p_ts)
                {
                  /* The payload of the PES packet is shorter than the 14 bytes
                     we would read. This means that high packet lost occured
                     so the PES won't be useful for any decoder. Moreover,
                     this should never happen so we can trash the packet and
                     exit roughly without regrets */
                  intf_DbgMsg("PES packet too short: trashed\n");
                  input_NetlistFreePES( p_input, p_pes );
                  p_pes = NULL;
                  /* XXX: Stats */
                  return;
                }

                i_ts_payload_size = p_ts->i_payload_end - p_ts->i_payload_start;
            }
            while(i_ts_payload_size + i_dummy < PES_HEADER_SIZE);

            /* This last TS packet is partly header, partly payload, so just
               copy the header part */
            memcpy(p_pes->p_pes_header_save + i_dummy,
                   &p_ts->buffer[p_ts->i_payload_start],
                   PES_HEADER_SIZE - i_dummy);

            /* The header must be read in the buffer not in any TS packet */
            p_pes->p_pes_header = p_pes->p_pes_header_save;

            /* Get the PES size if defined */
            if( (i_dummy = U16_AT(p_pes->p_pes_header + 4)) )
            {
                p_pes->i_pes_real_size = i_dummy + 6;
            }
        }

        /* Now we have the part of the PES header we were interested in:
           p_pes_header and i_pes_real_size ; we can parse it */
        input_ParsePES( p_input, p_es_descriptor );
    }

    /* If we are at the beginning of a new PES packet, we must fetch a new
       PES buffer to begin with the reassembly of this PES packet. This is
       also here that we can synchronise with the stream if we we lost
       packets or if the decoder has just started */
    if( b_unit_start )
    {
        p_last_pes = p_pes;

        /* Get a new one PES from the PES netlist. */
        if( (p_pes = input_NetlistGetPES( p_input )) == (NULL) )
        {
            /* PES netlist is empty ! */
            p_input->b_error = 1;
        }
        else
        {
            //intf_DbgMsg("New PES packet %p (first TS: %p)\n", p_pes, p_ts_packet);

            /* Init the PES fields so that the first TS packet could be
             * correctly added to the PES packet (see below) */
            p_pes->p_first_ts = p_ts_packet;
            p_pes->p_last_ts = NULL;

            /* If the last pes packet was null, this means that the
             * synchronization was lost and so warn the decoder that he
             * will have to find a way to recover */
            if( !p_last_pes )
                p_pes->b_data_loss = 1;

            /* Read the b_random_access flag status and then reinit it */
            p_pes->b_random_access = p_es_descriptor->b_random;
            p_es_descriptor->b_random = 0;
        }

        /* If the PES header fits in the first TS packet, we can
         * already set p_pes->p_pes_header, and in all cases we
	 * set p_pes->i_pes_real_size */
        if( p_ts_packet->i_payload_end - p_ts_packet->i_payload_start
                >= PES_HEADER_SIZE )
        {
            p_pes->p_pes_header = &(p_ts_packet->buffer[p_ts_packet->i_payload_start]);
            if( (i_dummy = U16_AT(p_pes->p_pes_header + 4)) )
            {
                p_pes->i_pes_real_size = i_dummy + 6;
            }
        }
    }


    /* If we are synchronized with the stream, and so if we are ready to
       receive correctly the data, add the TS packet to the current PES
       packet */
    if( p_pes != NULL )
    {
        //intf_DbgMsg("Adding TS %p to PES %p\n", p_ts_packet, p_pes);

        /* Size of the payload carried in the TS packet */
        i_ts_payload_size = p_ts_packet->i_payload_end -
                            p_ts_packet->i_payload_start;

        /* Update the relations between the TS packets */
        p_ts_packet->p_prev_ts = p_pes->p_last_ts;
        p_ts_packet->p_next_ts = NULL;
        if( p_pes->i_ts_packets != 0 )
        {
            /* Regarder si il serait pas plus efficace de ne creer que
             * les liens precedent->suivant pour le moment, et les
             * liens suivant->precedent quand le paquet est termine */
            /* Otherwise it is the first TS packet. */
            p_pes->p_last_ts->p_next_ts = p_ts_packet;
        }
        /* Now add the TS to the PES packet */
        p_pes->p_last_ts = p_ts_packet;
        p_pes->i_ts_packets++;
        p_pes->i_pes_size += i_ts_payload_size;

        /* Stats */
#ifdef STATS
        i_dummy = p_ts_packet->i_payload_end - p_ts_packet->i_payload_start;
        p_es_descriptor->c_payload_bytes += i_dummy;
#endif

        /* We can check if the packet is finished */
        if( p_pes->i_pes_size == p_pes->i_pes_real_size )
        {
            /* The packet is finished, parse it */
            input_ParsePES( p_input, p_es_descriptor );

            /* Tell the Demux we have parsed this PES, no need to redo it */
            p_pes = NULL;
        }
    }
    else
    {
        /* Since we don't use the TS packet to build a PES packet, we don't
           need it anymore, so give it back to the netlist */
        //intf_DbgMsg("Trashing TS %p: no PES being build\n", p_ts_packet);
        input_NetlistFreeTS( p_input, p_ts_packet );
    }

#undef p_pes
}



/*****************************************************************************
 * input_ParsePES
 *****************************************************************************
 * Parse a finished PES packet and analyze its header.
 *****************************************************************************/
static __inline__ void input_ParsePES( input_thread_t *p_input,
                                       es_descriptor_t *p_es_descriptor )
{
    decoder_fifo_t *            p_fifo;
    u8                          i_pes_header_size;
    ts_packet_t *               p_ts;
    int                         i_ts_payload_size;


#define p_pes (p_es_descriptor->p_pes_packet)

    //intf_DbgMsg("End of PES packet %p\n", p_pes);

    /* First read the 6 header bytes common to all PES packets:
       use them to test the PES validity */
    if( (p_pes->p_pes_header[0] || p_pes->p_pes_header[1] ||
        (p_pes->p_pes_header[2] != 1)) ||
                                 /* packet_start_code_prefix != 0x000001 */
        ((p_pes->i_pes_real_size) &&
         (p_pes->i_pes_real_size != p_pes->i_pes_size)) )
               /* PES_packet_length is set and != total received payload */
    {
      /* Trash the packet and set p_pes to NULL to be sure the next PES
         packet will have its b_data_lost flag set */
      intf_DbgMsg("Corrupted PES packet (size doesn't match) : trashed\n");
      input_NetlistFreePES( p_input, p_pes );
      p_pes = NULL;
      /* Stats XXX?? */
    }
    else
    {
        /* The PES packet is valid. Check its type to test if it may
           carry additional informations in a header extension */
        p_pes->i_stream_id =  p_pes->p_pes_header[3];

        switch( p_pes->i_stream_id )
        {
        case 0xBE:  /* Padding */
        case 0xBC:  /* Program stream map */
        case 0xBF:  /* Private stream 2 */
        case 0xB0:  /* ECM */
        case 0xB1:  /* EMM */
        case 0xFF:  /* Program stream directory */
        case 0xF2:  /* DSMCC stream */
        case 0xF8:  /* ITU-T H.222.1 type E stream */
            /* The payload begins immediatly after the 6 bytes header, so
               we have finished with the parsing */
            i_pes_header_size = 6;
            break;

        default:
            switch( p_pes->p_pes_header[8] & 0xc0 )
            {
              case 0x80: /* MPEG2: 10xx xxxx */
              case 0x00: /* FIXME: This shouldn't be allowed !! */
                /* The PES header contains at least 3 more bytes: parse them */
                p_pes->b_data_alignment = p_pes->p_pes_header[6] & 0x04;
                p_pes->b_has_pts = p_pes->p_pes_header[7] & 0x80;
                i_pes_header_size = p_pes->p_pes_header[8] + 9;

                /* Now parse the optional header extensions (in the limit of
                   the 14 bytes */
                if( p_pes->b_has_pts )
                {
                    pcr_descriptor_t * p_pcr;

                    p_pcr = p_input->p_pcr;

                    p_pes->i_pts =
                        ( ((mtime_t)(p_pes->p_pes_header[9] & 0x0E) << 29) |
                          (((mtime_t)U16_AT(p_pes->p_pes_header + 10) << 14) - (1 << 14)) |
                          ((mtime_t)U16_AT(p_pes->p_pes_header + 12) >> 1) ) * 300;
                    p_pes->i_pts /= 27;

                    if( p_pcr->i_synchro_state )
                    {
                        switch( p_pcr->i_synchro_state )
                        {
                            case SYNCHRO_NOT_STARTED:
                                p_pes->b_has_pts = 0;
                                break;

                            case SYNCHRO_START:
                                p_pes->i_pts += p_pcr->delta_pcr;
                                p_pcr->delta_absolute = mdate() - p_pes->i_pts + INPUT_PTS_DELAY;
                                p_pes->i_pts += p_pcr->delta_absolute;
                                p_pcr->i_synchro_state = 0;
                                break;

                            case SYNCHRO_REINIT: /* We skip a PES */
                                p_pes->b_has_pts = 0;
                                p_pcr->i_synchro_state = SYNCHRO_START;
                                break;
                        }
                    }
                    else
                    {
                        p_pes->i_pts += p_pcr->delta_pcr + p_pcr->delta_absolute;
                    }
                }
                break;

            default: /* MPEG1 or some strange thing */
                /* since this isn't supported yet, we certainly gonna crash */
                intf_ErrMsg( "FIXME: unknown PES type %.2x\n",
                             p_pes->p_pes_header[8] );
                i_pes_header_size = 6;
                break;

            }
            break;
        }

        /* Now we've parsed the header, we just have to indicate in some
         * specific TS packets where the PES payload begins (renumber
         * i_payload_start), so that the decoders can find the beginning
         * of their data right out of the box. */
        p_ts = p_pes->p_first_ts;
        i_ts_payload_size = p_ts->i_payload_end - p_ts->i_payload_start;
        while( i_pes_header_size > i_ts_payload_size )
        {
            /* These packets are entirely filled by the PES header. */
            i_pes_header_size -= i_ts_payload_size;
            p_ts->i_payload_start = p_ts->i_payload_end;
            /* Go to the next TS packet: here we won't have to test it is
             * not NULL because we trash the PES packets when packet lost
             * occurs */
            p_ts = p_ts->p_next_ts;
            i_ts_payload_size = p_ts->i_payload_end - p_ts->i_payload_start;
        }
        /* This last packet is partly header, partly payload. */
        p_ts->i_payload_start += i_pes_header_size;


        /* Now we can eventually put the PES packet in the decoder's
         * PES fifo */
        switch( p_es_descriptor->i_type )
        {
            case MPEG1_VIDEO_ES:
            case MPEG2_VIDEO_ES:
                p_fifo = &(((vpar_thread_t*)(p_es_descriptor->p_dec))->fifo);
                break;

            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                p_fifo = &(((adec_thread_t*)(p_es_descriptor->p_dec))->fifo);
                break;

            case AC3_AUDIO_ES:
                p_fifo = &(((ac3dec_thread_t *)(p_es_descriptor->p_dec))->fifo);
                break;

	    case LPCM_AUDIO_ES:
                p_fifo = &(((lpcmdec_thread_t *)(p_es_descriptor->p_dec))->fifo);
                break;

            case DVD_SPU_ES:
                /* we skip the first byte at the beginning of the
                 * subpicture payload, it only contains the SPU ID. */
                p_ts->i_payload_start++;
                p_fifo = &(((spudec_thread_t *)(p_es_descriptor->p_dec))->fifo);
                break;

            default:
                /* This should never happen */
                intf_DbgMsg("Unknown stream type (%d, %d): PES trashed\n",
                    p_es_descriptor->i_id, p_es_descriptor->i_type);
                p_fifo = NULL;
                break;
        }

        if( p_fifo != NULL )
        {
            vlc_mutex_lock( &p_fifo->data_lock );
            if( DECODER_FIFO_ISFULL( *p_fifo ) )
            {
                /* The FIFO is full !!! This should not happen. */
#ifdef STATS
                p_input->c_packets_trashed += p_pes->i_ts_packets;
                p_es_descriptor->c_invalid_packets += p_pes->i_ts_packets;
#endif
                input_NetlistFreePES( p_input, p_pes );
                intf_DbgMsg("PES trashed - fifo full ! (%d, %d)\n",
                           p_es_descriptor->i_id, p_es_descriptor->i_type);
            }
        else
            {
                //intf_DbgMsg("Putting %p into fifo %p/%d\n",
                //            p_pes, p_fifo, p_fifo->i_end);
                p_fifo->buffer[p_fifo->i_end] = p_pes;
                DECODER_FIFO_INCEND( *p_fifo );

                /* Warn the decoder that it's got work to do. */
                vlc_cond_signal( &p_fifo->data_wait );
            }
            vlc_mutex_unlock( &p_fifo->data_lock );
        }
        else
        {
            intf_DbgMsg("No fifo to receive PES %p: trash\n", p_pes);
#ifdef STATS
            p_input->c_packets_trashed += p_pes->i_ts_packets;
            p_es_descriptor->c_invalid_packets += p_pes->i_ts_packets;
#endif
            input_NetlistFreePES( p_input, p_pes );
        }
    }
#undef p_pes
}



/*****************************************************************************
 * input_DemuxPSI:
 *****************************************************************************
 * Notice that current ES state has been locked by input_SortPacket.
 * (No more true, changed by benny - FIXME: See if it's ok, and definitely
 * change the code ?? )
 *****************************************************************************/
static __inline__ void input_DemuxPSI( input_thread_t *p_input,
                                       ts_packet_t *p_ts_packet,
                                       es_descriptor_t *p_es_descriptor,
                                       boolean_t b_unit_start, boolean_t b_packet_lost )
{
    int i_data_offset;    /* Offset of the interesting data in the TS packet */
    u16 i_data_length;                               /* Length of those data */
  //boolean_t b_first_section;         /* another section in the TS packet ? */

    ASSERT(p_input);
    ASSERT(p_ts_packet);
    ASSERT(p_es_descriptor);

#define p_psi (p_es_descriptor->p_psi_section)

    //intf_DbgMsg( "input debug: PSI demultiplexing %p (%p)\n", p_ts_packet, p_input);

    //intf_DbgMsg( "Packet: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x (unit start: %d)\n", p_ts_packet->buffer[p_ts_packet->i_payload_start], p_ts_packet->buffer[p_ts_packet->i_payload_start+1], p_ts_packet->buffer[p_ts_packet->i_payload_start+2], p_ts_packet->buffer[p_ts_packet->i_payload_start+3], p_ts_packet->buffer[p_ts_packet->i_payload_start+4], p_ts_packet->buffer[p_ts_packet->i_payload_start+5], p_ts_packet->buffer[p_ts_packet->i_payload_start+6], p_ts_packet->buffer[p_ts_packet->i_payload_start+7], p_ts_packet->buffer[p_ts_packet->i_payload_start+8], p_ts_packet->buffer[p_ts_packet->i_payload_start+9], p_ts_packet->buffer[p_ts_packet->i_payload_start+10], p_ts_packet->buffer[p_ts_packet->i_payload_start+11], p_ts_packet->buffer[p_ts_packet->i_payload_start+12], p_ts_packet->buffer[p_ts_packet->i_payload_start+13], p_ts_packet->buffer[p_ts_packet->i_payload_start+14], p_ts_packet->buffer[p_ts_packet->i_payload_start+15], p_ts_packet->buffer[p_ts_packet->i_payload_start+16], p_ts_packet->buffer[p_ts_packet->i_payload_start+17], p_ts_packet->buffer[p_ts_packet->i_payload_start+18], p_ts_packet->buffer[p_ts_packet->i_payload_start+19], p_ts_packet->buffer[p_ts_packet->i_payload_start+20], b_unit_start);


    /* Try to find the beginning of the payload in the packet to initialise
     * the do-while loop that follows -> Compute the i_data_offset variable:
     * by default, the value is set so that we won't enter in the while loop.
     * It will be set to a correct value if the data are not corrupted */
    i_data_offset = TS_PACKET_SIZE;

    /* Has the reassembly of a section already begun in a previous packet ? */
    if( p_psi->b_running_section )
    {
        /* Was data lost since the last TS packet ? */
        if( b_packet_lost )
        {
            /* Discard the packet and wait for the begining of a new one
             * to resynch */
            p_psi->b_running_section = 0;
            p_psi->i_current_position = 0;
            intf_DbgMsg( "PSI section(s) discarded due to packet loss\n" );
        }
        else
        {
            /* The data that complete a previously began section are always at
             * the beginning of the TS payload... */
            i_data_offset = p_ts_packet->i_payload_start;
            /* ...Unless there is a pointer field, that we have to bypass */
            if( b_unit_start )
                i_data_offset++;
            //intf_DbgMsg( "New part of the section received at offset %d\n", i_data_offset );
        }
    }
    /* We are looking for the beginning of a new section */
    else
    {
        if( b_unit_start )
        {
            /* Get the offset at which the data for that section can be found
             * The offset is stored in the pointer_field since we are
             * interested in the first section of the TS packet. Note that
             * the +1 is to bypass the pointer field */
            i_data_offset = p_ts_packet->i_payload_start +
                            p_ts_packet->buffer[p_ts_packet->i_payload_start] + 1;
            //intf_DbgMsg( "New section beginning at offset %d in TS packet\n", i_data_offset );
        }
        else
        {
            /* This may either mean that the TS is bad or that the packet
             * contains the end of a section that had been discarded in a
             * previous loop: trash the TS packet since we cannot do
             * anything with those data: */
            p_psi->b_running_section = 0;
            p_psi->i_current_position = 0;
            intf_DbgMsg( "PSI packet discarded due to lack of synchronisation\n" );
        }
    }

    /* The section we will deal with during the first iteration of the
     * following loop is the first one contained in the TS packet */
    //    b_first_section = 1;

    /* Reassemble the pieces of sections contained in the TS packet and
     * decode the sections that could have been completed.
     * Stop when we reach the end of the packet or stuffing bytes */
    while( i_data_offset < TS_PACKET_SIZE && p_ts_packet->buffer[i_data_offset] != 0xFF )
    {
        /* If the current section is a new one, reinit the data fields of
         * the p_psi struct to start its decoding */
        if( !p_psi->b_running_section )
        {
            /* Read the length of the new section */
            p_psi->i_length = (U16_AT(&p_ts_packet->buffer[i_data_offset+1]) & 0xFFF) + 3;
            //intf_DbgMsg( "Section length %d\n", p_psi->i_length );
            if( p_psi->i_length > PSI_SECTION_SIZE )
            {
                /* The TS packet is corrupted, stop here to avoid possible
                 * a seg fault */
                intf_DbgMsg( "PSI Section size is too big, aborting its reception\n" );
                break;
            }

            /* Init the reassembly of that section */
            p_psi->b_running_section = 1;
            p_psi->i_current_position = 0;
        }

      /* Compute the length of data related to the section in this TS packet */
        if( p_psi->i_length - p_psi->i_current_position > TS_PACKET_SIZE - i_data_offset)
            i_data_length = TS_PACKET_SIZE - i_data_offset;
        else
          i_data_length = p_psi->i_length - p_psi->i_current_position;

        /* Copy those data in the section buffer */
        memcpy( &p_psi->buffer[p_psi->i_current_position], &p_ts_packet->buffer[i_data_offset],
                i_data_length );

        /* Interesting data are now after the ones we copied, since no gap is
         * allowed between 2 sections in a TS packets */
        i_data_offset += i_data_length;

        /* Decode the packet if it is now complete */
        if (p_psi->i_length == p_psi->i_current_position + i_data_length)
        {
            /* Packet is complete, decode it */
            //intf_DbgMsg( "SECTION COMPLETE: starting decoding of its data\n" );
            input_PsiDecode( p_input, p_psi );

            /* Prepare the buffer to receive a new section */
            p_psi->i_current_position = 0;
            p_psi->b_running_section = 0;

            /* The new section won't be the first anymore */
            //b_first_section = 0;
        }
        else
        {
            /* Prepare the buffer to receive the next part of the section */
          p_psi->i_current_position += i_data_length;
          //intf_DbgMsg( "Section not complete, waiting for the end\n" );
        }

        //intf_DbgMsg( "Must loop ? Next data offset: %d, stuffing: %d\n",
        //             i_data_offset, p_ts_packet->buffer[i_data_offset] );
    }

    /* Relase the TS packet, we don't need it anymore */
    input_NetlistFreeTS( p_input, p_ts_packet );

#undef p_psi
}

