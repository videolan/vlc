/*****************************************************************************
 * input_ctrl.c: Decoder control
 * Controls the extraction and the decoding of the programs elements carried
 * within a stream.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <stdio.h>
#include <netinet/in.h>                                             /* ntohs */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"
#include "plugins.h"
#include "debug.h"

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"

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

/*****************************************************************************
 * input_AddPgrmElem: Start the extraction and the decoding of a program element
 *****************************************************************************
 * Add the element given by its PID in the list of PID to extract and spawn
 * the decoding thread.
 * This function only modifies the table of selected es, but must NOT modify
 * the table of ES itself.
 *****************************************************************************/
int input_AddPgrmElem( input_thread_t *p_input, int i_current_id )
{
    int i_es_loop, i_selected_es_loop;

    /* Since this function is intended to be called by interface, lock the
     * elementary stream structure. */
    vlc_mutex_lock( &p_input->es_lock );

    /* Find out which PID we need. */
    for( i_es_loop = 0; i_es_loop < INPUT_MAX_ES; i_es_loop++ )
    {
        if( p_input->p_es[i_es_loop].i_id == i_current_id )
        {
            if( p_input->p_es[i_es_loop].p_dec != NULL )
            {
                /* We already have a decoder for that PID. */
                vlc_mutex_unlock( &p_input->es_lock );
                intf_ErrMsg("input error: PID %d already selected\n",
                            i_current_id);
                return( -1 );
            }

            intf_DbgMsg("Requesting selection of PID %d\n",
                        i_current_id);

            /* Find a free spot in pp_selected_es. */
            for( i_selected_es_loop = 0; p_input->pp_selected_es[i_selected_es_loop] != NULL
                  && i_selected_es_loop < INPUT_MAX_SELECTED_ES; i_selected_es_loop++ );

            if( i_selected_es_loop == INPUT_MAX_SELECTED_ES )
            {
                /* array full */
                vlc_mutex_unlock( &p_input->es_lock );
                intf_ErrMsg("input error: MAX_SELECTED_ES reached: try increasing it in config.h\n");
                return( -1 );
            }

            /* Don't decode PSI streams ! */
            if( p_input->p_es[i_es_loop].b_psi )
            {
                intf_ErrMsg("input_error: trying to decode PID %d which is the one of a PSI\n", i_current_id);
                vlc_mutex_unlock( &p_input->es_lock );
                return( -1 );
            }
            else
            {
                /* Spawn the decoder. */
                switch( p_input->p_es[i_es_loop].i_type )
                {
                    
                    case AC3_AUDIO_ES:
                        fprintf (stderr, "Start an AC3 decoder\n");
                        /* Spawn ac3 thread */
                        if ( ((ac3dec_thread_t *)(p_input->p_es[i_es_loop].p_dec) =
                            ac3dec_CreateThread(p_input)) == NULL )
                        {
                            intf_ErrMsg( "Could not start ac3 decoder\n" );
                            vlc_mutex_unlock( &p_input->es_lock );
                            return( -1 );
                        }
                        break;

		             case LPCM_AUDIO_ES:
                        /* Spawn lpcm thread */
                        fprintf (stderr, "Start a LPCM decoder\n");
                        if ( ((lpcmdec_thread_t *)(p_input->p_es[i_es_loop].p_dec) =
                            lpcmdec_CreateThread(p_input)) == NULL )
                        {
                            intf_ErrMsg( "LPCM Debug: Could not start lpcm decoder\n" );
                            vlc_mutex_unlock( &p_input->es_lock );
                            return( -1 );
                        }
                        break;


                    case DVD_SPU_ES:
                        /* Spawn spu thread */
                        if ( ((spudec_thread_t *)(p_input->p_es[i_es_loop].p_dec) =
                            spudec_CreateThread(p_input)) == NULL )
                        {
                            intf_ErrMsg( "Could not start spu decoder\n" );
                            vlc_mutex_unlock( &p_input->es_lock );
                            return( -1 );
                        }
                        break;

                    case MPEG1_AUDIO_ES:
                    case MPEG2_AUDIO_ES:
                        /* Spawn audio thread. */
                        if( ((adec_thread_t*)(p_input->p_es[i_es_loop].p_dec) =
                            adec_CreateThread( p_input )) == NULL )
                        {
                            intf_ErrMsg("Could not start audio decoder\n");
                            vlc_mutex_unlock( &p_input->es_lock );
                            return( -1 );
                        }
                        break;

                    case MPEG1_VIDEO_ES:
                    case MPEG2_VIDEO_ES:
                        /* Spawn video thread. */
#ifdef OLD_DECODER
                        if( ((vdec_thread_t*)(p_input->p_es[i_es_loop].p_dec) =
                            vdec_CreateThread( p_input )) == NULL )
#else
                        if( ((vpar_thread_t*)(p_input->p_es[i_es_loop].p_dec) =
                            vpar_CreateThread( p_input )) == NULL )
#endif
                        {
#ifdef OLD_DECODER
                            intf_ErrMsg("Could not start video decoder\n");
#else
                            intf_ErrMsg("Could not start video parser\n");
#endif
                            vlc_mutex_unlock( &p_input->es_lock );
                            return( -1 );
                        }
                        break;

                    default:
                        /* That should never happen. */
                        intf_DbgMsg("input error: unknown stream type (0x%.2x)\n",
                                    p_input->p_es[i_es_loop].i_type);
                        vlc_mutex_unlock( &p_input->es_lock );
                        return( -1 );
                        break;
                }

                /* Initialise the demux */
                p_input->p_es[i_es_loop].p_pes_packet = NULL;
                p_input->p_es[i_es_loop].i_continuity_counter = 0xff;
                p_input->p_es[i_es_loop].b_random = 0;

                /* Mark stream to be demultiplexed. */
                intf_DbgMsg("Stream %d added in %d\n", i_current_id, i_selected_es_loop);
                p_input->pp_selected_es[i_selected_es_loop] = &p_input->p_es[i_es_loop];
                vlc_mutex_unlock( &p_input->es_lock );
                return( 0 );
            }
        }
    }

    /* We haven't found this PID in the current stream. */
    vlc_mutex_unlock( &p_input->es_lock );
    intf_ErrMsg("input error: can't find PID %d\n", i_current_id);
    return( -1 );
}

/*****************************************************************************
 * input_DelPgrmElem: Stop the decoding of a program element
 *****************************************************************************
 * Stop the extraction of the element given by its PID and kill the associated
 * decoder thread
 * This function only modifies the table of selected es, but must NOT modify
 * the table of ES itself.
 *****************************************************************************/
int input_DelPgrmElem( input_thread_t *p_input, int i_current_id )
{
    int i_selected_es_loop, i_last_selected;

    /* Since this function is intended to be called by interface, lock the
       structure. */
    vlc_mutex_lock( &p_input->es_lock );

    /* Find out which PID we need. */
    for( i_selected_es_loop = 0; i_selected_es_loop < INPUT_MAX_SELECTED_ES;
         i_selected_es_loop++ )
    {
        if( p_input->pp_selected_es[i_selected_es_loop] )
        {
            if( p_input->pp_selected_es[i_selected_es_loop]->i_id == i_current_id )
            {
                if( !(p_input->pp_selected_es[i_selected_es_loop]->p_dec) )
                {
                    /* We don't have a decoder for that PID. */
                    vlc_mutex_unlock( &p_input->es_lock );
                    intf_ErrMsg("input error: PID %d already deselected\n",
                                i_current_id);
                    return( -1 );
                }

                intf_DbgMsg("input debug: requesting termination of PID %d\n",
                            i_current_id);

                /* Cancel the decoder. */
                switch( p_input->pp_selected_es[i_selected_es_loop]->i_type )
                {
                    case AC3_AUDIO_ES:
                        ac3dec_DestroyThread( (ac3dec_thread_t *)(p_input->pp_selected_es[i_selected_es_loop]->p_dec) );
                        break;
			
                    case LPCM_AUDIO_ES:
                        lpcmdec_DestroyThread( (lpcmdec_thread_t *)(p_input->pp_selected_es[i_selected_es_loop]->p_dec) );
                        break;

                    case DVD_SPU_ES:
                        spudec_DestroyThread( (spudec_thread_t *)(p_input->pp_selected_es[i_selected_es_loop]->p_dec) );
                        break;

                    case MPEG1_AUDIO_ES:
                    case MPEG2_AUDIO_ES:
                        adec_DestroyThread( (adec_thread_t*)(p_input->pp_selected_es[i_selected_es_loop]->p_dec) );
                        break;

                    case MPEG1_VIDEO_ES:
                    case MPEG2_VIDEO_ES:
#ifdef OLD_DECODER
                        vdec_DestroyThread( (vdec_thread_t*)(p_input->pp_selected_es[i_selected_es_loop]->p_dec) /*, NULL */ );
#else
                        vpar_DestroyThread( (vpar_thread_t*)(p_input->pp_selected_es[i_selected_es_loop]->p_dec) /*, NULL */ );
#endif
                        break;
                }

                /* Unmark stream. */
                p_input->pp_selected_es[i_selected_es_loop]->p_dec = NULL;

                /* Find last selected stream. */
                for( i_last_selected = i_selected_es_loop;
                        p_input->pp_selected_es[i_last_selected]
                        && i_last_selected < INPUT_MAX_SELECTED_ES;
                     i_last_selected++ );

                /* Exchange streams. */
                p_input->pp_selected_es[i_selected_es_loop] =
                            p_input->pp_selected_es[i_last_selected];
                p_input->pp_selected_es[i_last_selected] = NULL;

                vlc_mutex_unlock( &p_input->es_lock );
                return( 0 );
            }
        }
    }

    /* We haven't found this PID in the current stream. */
    vlc_mutex_unlock( &p_input->es_lock );
    intf_ErrMsg("input error: can't find PID %d\n", i_current_id);
    return( -1 );
}



/*****************************************************************************
 * input_IsElemRecv: Test if an element given by its PID is currently received
 *****************************************************************************
 * Cannot return the position of the es in the pp_selected_es, for it can
 * change once we have released the lock
 *****************************************************************************/
boolean_t input_IsElemRecv( input_thread_t *p_input, int i_id )
{
  boolean_t b_is_recv = 0;
    int i_index = 0;

   /* Since this function is intended to be called by interface, lock the
       structure. */
    vlc_mutex_lock( &p_input->es_lock );

    /* Scan the table */
    while( i_index < INPUT_MAX_SELECTED_ES && !p_input->pp_selected_es[i_index] )
    {
      if( p_input->pp_selected_es[i_index]->i_id == i_id )
      {
        b_is_recv = 1;
        break;
      }
    }

    /* Unlock the structure */
    vlc_mutex_unlock( &p_input->es_lock );

    return( b_is_recv );
}
