/*****************************************************************************
 * mpeg_ts.c : Transport Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: ts.c,v 1.2 2002/08/07 00:29:36 sam Exp $
 *
 * Authors: Henri Fallon <henri@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "iso_lang.h"

#if defined MODULE_NAME_IS_ts_dvbpsi
#   ifdef HAVE_DVBPSI_DR_H
#       include <dvbpsi/dvbpsi.h>
#       include <dvbpsi/descriptor.h>
#       include <dvbpsi/pat.h>
#       include <dvbpsi/pmt.h>
#       include <dvbpsi/dr.h>
#   else
#       include "dvbpsi.h"
#       include "descriptor.h"
#       include "tables/pat.h"
#       include "tables/pmt.h"
#       include "descriptors/dr.h"
#   endif
#endif

#include "system.h"

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define TS_READ_ONCE 200

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    module_t *   p_module;
    mpeg_demux_t mpeg;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate   ( vlc_object_t * );
static void Deactivate ( vlc_object_t * );
static int  Demux      ( input_thread_t * );

#if defined MODULE_NAME_IS_ts
static void TSDemuxPSI ( input_thread_t *, data_packet_t *,
                          es_descriptor_t *, vlc_bool_t );
static void TSDecodePAT( input_thread_t *, es_descriptor_t *);
static void TSDecodePMT( input_thread_t *, es_descriptor_t *);
#define PSI_CALLBACK TSDemuxPSI
#elif defined MODULE_NAME_IS_ts_dvbpsi
static void TS_DVBPSI_DemuxPSI  ( input_thread_t *, data_packet_t *,
                                  es_descriptor_t *, vlc_bool_t );
static void TS_DVBPSI_HandlePAT ( input_thread_t *, dvbpsi_pat_t * );
static void TS_DVBPSI_HandlePMT ( input_thread_t *, dvbpsi_pmt_t * );
#define PSI_CALLBACK TS_DVBPSI_DemuxPSI
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
#if defined MODULE_NAME_IS_ts
    set_description( _("ISO 13818-1 MPEG Transport Stream input") );
    set_capability( "demux", 160 );
    add_shortcut( "ts" );
#elif defined MODULE_NAME_IS_ts_dvbpsi
    set_description( _("ISO 13818-1 MPEG Transport Stream input (libdvbpsi)") );
    set_capability( "demux", 170 );
    add_shortcut( "ts_dvbpsi" );
#endif
    set_callbacks( Activate, Deactivate );
vlc_module_end();

/*****************************************************************************
 * Activate: initialize TS structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    demux_sys_t *       p_demux;
    es_descriptor_t *   p_pat_es;
    es_ts_data_t *      p_demux_data;
    stream_ts_data_t *  p_stream_data;
    byte_t *            p_peek;

    /* Set the demux function */
    p_input->pf_demux = Demux;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    /* Have a peep at the show. */
    if( input_Peek( p_input, &p_peek, 1 ) < 1 )
    {
        msg_Err( p_input, "cannot peek()" );
        return -1;
    }

    if( *p_peek != TS_SYNC_CODE )
    {
        if( *p_input->psz_demux && !strncmp( p_input->psz_demux, "ts", 3 ) )
        {
            /* User forced */
            msg_Err( p_input, "this does not look like a TS stream, continuing" );
        }
        else
        {
            msg_Warn( p_input, "TS module discarded (no sync)" );
            return -1;
        }
    }

    /* Adapt the bufsize for our only use. */
    if( p_input->i_mtu != 0 )
    {
        /* Have minimum granularity to avoid bottlenecks at the input level. */
        p_input->i_bufsize = (p_input->i_mtu / TS_PACKET_SIZE) * TS_PACKET_SIZE;
    }

    p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t ) );
    if( p_demux == NULL )
    {
        return -1;
    }

    p_input->p_private = (void*)&p_demux->mpeg;
    p_demux->p_module = module_Need( p_input, "mpeg-system", NULL );
    if( p_demux->p_module == NULL )
    {
        free( p_input->p_demux_data );
        return -1;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( input_InitStream( p_input, sizeof( stream_ts_data_t ) ) == -1 )
    {
        module_Unneed( p_input, p_demux->p_module );
        free( p_input->p_demux_data );
        return -1;
    }

    p_stream_data = (stream_ts_data_t *)p_input->stream.p_demux_data;
    p_stream_data->i_pat_version = PAT_UNINITIALIZED ;

#ifdef MODULE_NAME_IS_ts_dvbpsi
    p_stream_data->p_pat_handle = (dvbpsi_handle *)
      dvbpsi_AttachPAT( (dvbpsi_pat_callback) &TS_DVBPSI_HandlePAT, p_input ); 

    if( p_stream_data->p_pat_handle == NULL )
    {
        msg_Err( p_input, "could not create PAT decoder" );
        module_Unneed( p_input, p_demux->p_module );
        free( p_input->p_demux_data );
        return -1;
    }
#endif
    
    /* We'll have to catch the PAT in order to continue
     * Then the input will catch the PMT and then the others ES
     * The PAT es is indepedent of any program. */
    p_pat_es = input_AddES( p_input, NULL,
                            0x00, sizeof( es_ts_data_t ) );
    p_demux_data = (es_ts_data_t *)p_pat_es->p_demux_data;
    p_demux_data->b_psi = 1;
    p_demux_data->i_psi_type = PSI_IS_PAT;
    p_demux_data->p_psi_section = malloc(sizeof(psi_section_t));
    p_demux_data->p_psi_section->b_is_complete = 1;

    vlc_mutex_unlock( &p_input->stream.stream_lock );
    
    return 0;
}

/*****************************************************************************
 * Deactivate: deinitialize TS structures
 *****************************************************************************/
static void Deactivate( vlc_object_t * p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;

    module_Unneed( p_input, p_input->p_demux_data->p_module );
    free( p_input->p_demux_data );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * packets.
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t *   p_demux = p_input->p_demux_data;
    int             i_read_once = (p_input->i_mtu ?
                                   p_input->i_bufsize / TS_PACKET_SIZE :
                                   TS_READ_ONCE);
    int             i;

    for( i = 0; i < i_read_once; i++ )
    {
        data_packet_t *     p_data;
        ssize_t             i_result;

        i_result = p_demux->mpeg.pf_read_ts( p_input, &p_data );

        if( i_result <= 0 )
        {
            return i_result;
        }

        p_demux->mpeg.pf_demux_ts( p_input, p_data,
                                   (psi_callback_t) &PSI_CALLBACK );
    }

    return i_read_once;
}


#if defined MODULE_NAME_IS_ts
/*
 * PSI demultiplexing and decoding without libdvbpsi
 */

/*****************************************************************************
 * DemuxPSI : makes up complete PSI data
 *****************************************************************************/
static void TSDemuxPSI( input_thread_t * p_input, data_packet_t * p_data, 
        es_descriptor_t * p_es, vlc_bool_t b_unit_start )
{
    es_ts_data_t  * p_demux_data;
    
    p_demux_data = (es_ts_data_t *)p_es->p_demux_data;

#define p_psi (p_demux_data->p_psi_section)
#define p (p_data->p_payload_start)

    if( b_unit_start )
    {
        /* unit_start set to 1 -> presence of a pointer field
         * (see ISO/IEC 13818 (2.4.4.2) which should be set to 0x00 */
        if( (u8)p[0] != 0x00 )
        {
            msg_Warn( p_input,
                      "non-zero pointer field found, trying to continue" );
            p+=(u8)p[0];
        }
        else
        {
            p++;
        }

        /* This is the begining of a new section */

        if( ((u8)(p[1]) & 0xc0) != 0x80 ) 
        {
            msg_Warn( p_input, "invalid PSI packet" );
            p_psi->b_trash = 1;
        }
        else 
        {
            p_psi->i_section_length = ((p[1] & 0xF) << 8) | p[2];
            p_psi->b_section_complete = 0;
            p_psi->i_read_in_section = 0;
            p_psi->i_section_number = (u8)p[6];

            if( p_psi->b_is_complete || p_psi->i_section_number == 0 )
            {
                /* This is a new PSI packet */
                p_psi->b_is_complete = 0;
                p_psi->b_trash = 0;
                p_psi->i_version_number = ( p[5] >> 1 ) & 0x1f;
                p_psi->i_last_section_number = (u8)p[7];

                /* We'll write at the begining of the buffer */
                p_psi->p_current = p_psi->buffer;
            }
            else
            {
                if( p_psi->b_section_complete )
                {
                    /* New Section of an already started PSI */
                    p_psi->b_section_complete = 0;
                    
                    if( p_psi->i_version_number != (( p[5] >> 1 ) & 0x1f) )
                    {
                        msg_Warn( p_input,
                                  "PSI version differs inside same PAT" );
                        p_psi->b_trash = 1;
                    }
                    if( p_psi->i_section_number + 1 != (u8)p[6] )
                    {
                        msg_Warn( p_input,
                                  "PSI Section discontinuity, packet lost?" );
                        p_psi->b_trash = 1;
                    }
                    else
                        p_psi->i_section_number++;
                }
                else
                {
                    msg_Warn( p_input, "got unexpected new PSI section" );
                    p_psi->b_trash = 1;
                }
            }
        }
    } /* b_unit_start */
    
    if( !p_psi->b_trash )
    {
        /* read */
        if( (p_data->p_payload_end - p) >=
            ( p_psi->i_section_length - p_psi->i_read_in_section ) )
        {
            /* The end of the section is in this TS packet */
            memcpy( p_psi->p_current, p, 
            (p_psi->i_section_length - p_psi->i_read_in_section) );
    
            p_psi->b_section_complete = 1;
            p_psi->p_current += 
                (p_psi->i_section_length - p_psi->i_read_in_section);
                        
            if( p_psi->i_section_number == p_psi->i_last_section_number )
            {
                /* This was the last section of PSI */
                p_psi->b_is_complete = 1;

                switch( p_demux_data->i_psi_type)
                {
                case PSI_IS_PAT:
                    TSDecodePAT( p_input, p_es );
                    break;
                case PSI_IS_PMT:
                    TSDecodePMT( p_input, p_es );
                    break;
                default:
                    msg_Warn( p_input, "received unknown PSI in DemuxPSI" );
                }
            }
        }
        else
        {
            memcpy( p_psi->buffer, p, p_data->p_payload_end - p );
            p_psi->i_read_in_section += p_data->p_payload_end - p;

            p_psi->p_current += p_data->p_payload_end - p;
        }
    }

#undef p_psi    
#undef p
   
    input_DeletePacket( p_input->p_method_data, p_data );
    
    return ;
}

/*****************************************************************************
 * DecodePAT : Decodes Programm association table and deal with it
 *****************************************************************************/
static void TSDecodePAT( input_thread_t * p_input, es_descriptor_t * p_es )
{
    stream_ts_data_t  * p_stream_data;
    es_ts_data_t      * p_demux_data;

    pgrm_descriptor_t * p_pgrm;
    es_descriptor_t   * p_current_es;
    byte_t            * p_current_data;           

    int                 i_section_length, i_program_id, i_pmt_pid;
    int                 i_loop, i_current_section;

    vlc_bool_t          b_changed = 0;

    p_demux_data = (es_ts_data_t *)p_es->p_demux_data;
    p_stream_data = (stream_ts_data_t *)p_input->stream.p_demux_data;
    
#define p_psi (p_demux_data->p_psi_section)

    /* Not so fast, Mike ! If the PAT version has changed, we first check
     * that its content has really changed before doing anything */
    if( p_stream_data->i_pat_version != p_psi->i_version_number )
    {
        int i_programs = p_input->stream.i_pgrm_number;

        p_current_data = p_psi->buffer;

        do
        {
            i_section_length = ((u32)(p_current_data[1] & 0xF) << 8) |
                                 p_current_data[2];
            i_current_section = (u8)p_current_data[6];
    
            for( i_loop = 0;
                 ( i_loop < (i_section_length - 9) / 4 ) && !b_changed;
                 i_loop++ )
            {
                i_program_id = ( (u32)*(p_current_data + i_loop * 4 + 8) << 8 )
                                 | *(p_current_data + i_loop * 4 + 9);
                i_pmt_pid = ( ((u32)*(p_current_data + i_loop * 4 + 10) & 0x1F)
                                    << 8 )
                               | *(p_current_data + i_loop * 4 + 11);

                if( i_program_id )
                {
                    if( (p_pgrm = input_FindProgram( p_input, i_program_id ))
                        && (p_current_es = input_FindES( p_input, i_pmt_pid ))
                        && p_current_es->p_pgrm == p_pgrm
                        && p_current_es->i_id == i_pmt_pid
                        && ((es_ts_data_t *)p_current_es->p_demux_data)->b_psi
                        && ((es_ts_data_t *)p_current_es->p_demux_data)
                            ->i_psi_type == PSI_IS_PMT )
                    {
                        i_programs--;
                    }
                    else
                    {
                        b_changed = 1;
                    }
                }
            }
            
            p_current_data += 3 + i_section_length;

        } while( ( i_current_section < p_psi->i_last_section_number )
                  && !b_changed );

        /* If we didn't find the expected amount of programs, the PAT has
         * changed. Otherwise, it only changed if b_changed is already != 0 */
        b_changed = b_changed || i_programs;
    }

    if( b_changed )
    {
        /* PAT has changed. We are going to delete all programs and 
         * create new ones. We chose not to only change what was needed
         * as a PAT change may mean the stream is radically changing and
         * this is a secure method to avoid crashes */
        es_ts_data_t      * p_es_demux;
        pgrm_ts_data_t    * p_pgrm_demux;
        
        p_current_data = p_psi->buffer;

        /* Delete all programs */
        while( p_input->stream.i_pgrm_number )
        {
            input_DelProgram( p_input, p_input->stream.pp_programs[0] );
        }
        
        do
        {
            i_section_length = ((u32)(p_current_data[1] & 0xF) << 8) |
                                 p_current_data[2];
            i_current_section = (u8)p_current_data[6];
    
            for( i_loop = 0; i_loop < (i_section_length - 9) / 4 ; i_loop++ )
            {
                i_program_id = ( (u32)*(p_current_data + i_loop * 4 + 8) << 8 )
                                 | *(p_current_data + i_loop * 4 + 9);
                i_pmt_pid = ( ((u32)*(p_current_data + i_loop * 4 + 10) & 0x1F)
                                    << 8 )
                               | *(p_current_data + i_loop * 4 + 11);
    
                /* If program = 0, we're having info about NIT not PMT */
                if( i_program_id )
                {
                    /* Add this program */
                    p_pgrm = input_AddProgram( p_input, i_program_id, 
                                               sizeof( pgrm_ts_data_t ) );
                   
                    /* whatis the PID of the PMT of this program */
                    p_pgrm_demux = (pgrm_ts_data_t *)p_pgrm->p_demux_data;
                    p_pgrm_demux->i_pmt_version = PMT_UNINITIALIZED;
    
                    /* Add the PMT ES to this program */
                    p_current_es = input_AddES( p_input, p_pgrm,(u16)i_pmt_pid,
                                        sizeof( es_ts_data_t) );
                    p_es_demux = (es_ts_data_t *)p_current_es->p_demux_data;
                    p_es_demux->b_psi = 1;
                    p_es_demux->i_psi_type = PSI_IS_PMT;
                    
                    p_es_demux->p_psi_section = 
                                            malloc( sizeof( psi_section_t ) );
                    p_es_demux->p_psi_section->b_is_complete = 0;
                }
            }
            
            p_current_data += 3 + i_section_length;

        } while( i_current_section < p_psi->i_last_section_number );

        /* Go to the beginning of the next section */
        p_stream_data->i_pat_version = p_psi->i_version_number;

    }
#undef p_psi

}

/*****************************************************************************
 * DecodePMT : decode a given Program Stream Map
 * ***************************************************************************
 * When the PMT changes, it may mean a deep change in the stream, and it is
 * careful to delete the ES and add them again. If the PMT doesn't change,
 * there no need to do anything.
 *****************************************************************************/
static void TSDecodePMT( input_thread_t * p_input, es_descriptor_t * p_es )
{

    pgrm_ts_data_t            * p_pgrm_data;
    es_ts_data_t              * p_demux_data;

    p_demux_data = (es_ts_data_t *)p_es->p_demux_data;
    p_pgrm_data = (pgrm_ts_data_t *)p_es->p_pgrm->p_demux_data;
    
#define p_psi (p_demux_data->p_psi_section)

    if( p_psi->i_version_number != p_pgrm_data->i_pmt_version ) 
    {
        es_descriptor_t   * p_new_es;  
        es_ts_data_t      * p_es_demux;
        byte_t            * p_current_data, * p_current_section;
        int                 i_section_length,i_current_section;
        int                 i_prog_info_length, i_loop;
        int                 i_es_info_length, i_pid, i_stream_type;
        
        p_current_section = p_psi->buffer;
        p_current_data = p_psi->buffer;

        p_pgrm_data->i_pcr_pid = ( ((u32)*(p_current_section + 8) & 0x1F) << 8 ) |
                                    *(p_current_section + 9);


        /* Lock stream information */
        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* Delete all ES in this program  except the PSI. We start from the
         * end because i_es_number gets decremented after each deletion. */
        for( i_loop = p_es->p_pgrm->i_es_number ; i_loop ; )
        {
            i_loop--;
            p_es_demux = (es_ts_data_t *)
                         p_es->p_pgrm->pp_es[i_loop]->p_demux_data;
            if ( ! p_es_demux->b_psi )
            {
                input_DelES( p_input, p_es->p_pgrm->pp_es[i_loop] );
            }
        }

        /* Then add what we received in this PMT */
        do
        {
            i_section_length = ( ((u32)*(p_current_data + 1) & 0xF) << 8 ) |
                                  *(p_current_data + 2);
            i_current_section = (u8)p_current_data[6];
            i_prog_info_length = ( ((u32)*(p_current_data + 10) & 0xF) << 8 ) |
                                    *(p_current_data + 11);

            /* For the moment we ignore program descriptors */
            p_current_data += 12 + i_prog_info_length;
    
            /* The end of the section, before the CRC is at 
             * p_current_section + i_section_length -1 */
            while( p_current_data < p_current_section + i_section_length -1 )
            {
                i_stream_type = (int)p_current_data[0];
                i_pid = ( ((u32)*(p_current_data + 1) & 0x1F) << 8 ) |
                           *(p_current_data + 2);
                i_es_info_length = ( ((u32)*(p_current_data + 3) & 0xF) << 8 ) |
                                      *(p_current_data + 4);
                
                /* Add this ES to the program */
                p_new_es = input_AddES( p_input, p_es->p_pgrm, 
                                        (u16)i_pid, sizeof( es_ts_data_t ) );

                /* Tell the interface what kind of stream it is and select 
                 * the required ones */
                {
                    switch( i_stream_type )
                    {
                        case MPEG1_VIDEO_ES:
                        case MPEG2_VIDEO_ES:
                            p_new_es->i_fourcc = VLC_FOURCC('m','p','g','v');
                            p_new_es->i_cat = VIDEO_ES;
                            break;
                        case MPEG1_AUDIO_ES:
                        case MPEG2_AUDIO_ES:
                            p_new_es->i_fourcc = VLC_FOURCC('m','p','g','a');
                            p_new_es->i_cat = AUDIO_ES;
                            break;
                        case LPCM_AUDIO_ES:
                            p_new_es->i_fourcc = VLC_FOURCC('l','p','c','m');
                            p_new_es->i_stream_id = 0xBD;
                            p_new_es->i_cat = AUDIO_ES;
                            break;
                        case A52_AUDIO_ES:
                            p_new_es->i_fourcc = VLC_FOURCC('a','5','2',' ');
                            p_new_es->i_stream_id = 0xBD;
                            p_new_es->i_cat = AUDIO_ES;
                            break;
                        /* Not sure this one is fully specification-compliant */
                        case DVD_SPU_ES:
                            p_new_es->i_fourcc = VLC_FOURCC('s','p','u',' ');
                            p_new_es->i_stream_id = 0xBD;
                            p_new_es->i_cat = SPU_ES;
                            break;
                        default :
                            p_new_es->i_fourcc = 0;
                            p_new_es->i_cat = UNKNOWN_ES;
                            break;
                    }
                }
                
                p_current_data += 5 + i_es_info_length;
            }

            /* Go to the beginning of the next section*/
            p_current_data += 3 + i_section_length;
           
            p_current_section++;
            
        } while( i_current_section < p_psi->i_last_section_number );

        p_pgrm_data->i_pmt_version = p_psi->i_version_number;

        /* if no program is selected :*/
        if( !p_input->stream.p_selected_program )
        {
            pgrm_descriptor_t *     p_pgrm_to_select;
            u16 i_id = (u16)config_GetInt( p_input, "program" );

            if( i_id != 0 ) /* if user specified a program */
            {
                p_pgrm_to_select = input_FindProgram( p_input, i_id );

                if( p_pgrm_to_select && p_pgrm_to_select == p_es->p_pgrm )
                    p_input->pf_set_program( p_input, p_pgrm_to_select );
            }
            else
                    p_input->pf_set_program( p_input, p_es->p_pgrm );
        }
        
        /* inform interface that stream has changed */
        p_input->stream.b_changed = 1;
        /*  Remove lock */
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    
#undef p_psi
}

#elif defined MODULE_NAME_IS_ts_dvbpsi
/*
 * PSI Decoding using libdvbcss 
 */

/*****************************************************************************
 * DemuxPSI : send the PSI to the right libdvbpsi decoder
 *****************************************************************************/
static void TS_DVBPSI_DemuxPSI( input_thread_t  * p_input, 
                                data_packet_t   * p_data, 
                                es_descriptor_t * p_es, 
                                vlc_bool_t        b_unit_start )
{
    es_ts_data_t        * p_es_demux_data;
    pgrm_ts_data_t      * p_pgrm_demux_data;
    stream_ts_data_t    * p_stream_demux_data;

    p_es_demux_data = ( es_ts_data_t * ) p_es->p_demux_data;
    p_stream_demux_data = ( stream_ts_data_t * ) p_input->stream.p_demux_data;

    switch( p_es_demux_data->i_psi_type)
    {
        case PSI_IS_PAT:
            dvbpsi_PushPacket( 
                    ( dvbpsi_handle ) p_stream_demux_data->p_pat_handle,
                    p_data->p_demux_start );
            break;
        case PSI_IS_PMT:
            p_pgrm_demux_data = ( pgrm_ts_data_t * )p_es->p_pgrm->p_demux_data;
            dvbpsi_PushPacket( 
                    ( dvbpsi_handle ) p_pgrm_demux_data->p_pmt_handle,
                    p_data->p_demux_start );
            break;
        default:
            msg_Warn( p_input, "received unknown PSI in DemuxPSI" );
    }
    
    input_DeletePacket( p_input->p_method_data, p_data );
}

/*****************************************************************************
 * HandlePAT: will treat a PAT returned by dvbpsi
 *****************************************************************************/

void TS_DVBPSI_HandlePAT( input_thread_t * p_input, dvbpsi_pat_t * p_new_pat )
{
    dvbpsi_pat_program_t *      p_pgrm;
    pgrm_descriptor_t *         p_new_pgrm;
    pgrm_ts_data_t *            p_pgrm_demux;
    es_descriptor_t *           p_current_es;
    es_ts_data_t *              p_es_demux;
    stream_ts_data_t *          p_stream_data;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    p_stream_data = (stream_ts_data_t *)p_input->stream.p_demux_data;
    
    if ( !p_new_pat->b_current_next || 
            p_stream_data->i_pat_version == PAT_UNINITIALIZED  )
    {
        /* Delete all programs */
        while( p_input->stream.i_pgrm_number )
        {
            input_DelProgram( p_input, p_input->stream.pp_programs[0] );
        }
    
        /* treat the new programs list */
        p_pgrm = p_new_pat->p_first_program;
        
        while( p_pgrm )
        {
            /* If program = 0, we're having info about NIT not PMT */
            if( p_pgrm->i_number )
            {
                /* Add this program */
                p_new_pgrm = input_AddProgram( p_input, p_pgrm->i_number, 
                                            sizeof( pgrm_ts_data_t ) );

                p_pgrm_demux = (pgrm_ts_data_t *)p_new_pgrm->p_demux_data;
                p_pgrm_demux->i_pmt_version = PMT_UNINITIALIZED;
        
                /* Add the PMT ES to this program */
                p_current_es = input_AddES( p_input, p_new_pgrm,
                                            (u16) p_pgrm->i_pid,
                                            sizeof( es_ts_data_t) );
                p_es_demux = (es_ts_data_t *)p_current_es->p_demux_data;
                p_es_demux->b_psi = 1;
                p_es_demux->i_psi_type = PSI_IS_PMT;
                        
                p_es_demux->p_psi_section = malloc( sizeof( psi_section_t ) );
                if ( p_es_demux->p_psi_section == NULL )
                {
                    msg_Err( p_input, "out of memory" );
                    p_input->b_error = 1;
                    return;
                }
            
                p_es_demux->p_psi_section->b_is_complete = 0;
                
                /* Create a PMT decoder */
                p_pgrm_demux->p_pmt_handle = (dvbpsi_handle *)
                    dvbpsi_AttachPMT( p_pgrm->i_number,
                            (dvbpsi_pmt_callback) &TS_DVBPSI_HandlePMT, 
                            p_input );

                if( p_pgrm_demux->p_pmt_handle == NULL )
                {
                    msg_Err( p_input, "could not create PMT decoder" );
                    p_input->b_error = 1;
                    return;
                }

            }
            p_pgrm = p_pgrm->p_next; 
        }
        
        p_stream_data->i_pat_version = p_new_pat->i_version;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * HandlePMT: will treat a PMT returned by dvbpsi
 *****************************************************************************/
void TS_DVBPSI_HandlePMT( input_thread_t * p_input, dvbpsi_pmt_t * p_new_pmt )
{
    dvbpsi_pmt_es_t *       p_es;
    pgrm_descriptor_t *     p_pgrm;
    es_descriptor_t *       p_new_es;
    pgrm_ts_data_t *        p_pgrm_demux;
   
    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    p_pgrm = input_FindProgram( p_input, p_new_pmt->i_program_number );

    if( p_pgrm == NULL )
    {
        msg_Warn( p_input, "PMT of unreferenced program found" );
        return;
    }

    p_pgrm_demux = (pgrm_ts_data_t *)p_pgrm->p_demux_data;
    p_pgrm_demux->i_pcr_pid = p_new_pmt->i_pcr_pid;

    if( !p_new_pmt->b_current_next || 
            p_pgrm_demux->i_pmt_version == PMT_UNINITIALIZED )
    {
        p_es = p_new_pmt->p_first_es;
        while( p_es )
        {
            /* Add this ES */
            p_new_es = input_AddES( p_input, p_pgrm, 
                            (u16)p_es->i_pid, sizeof( es_ts_data_t ) );
            if( p_new_es == NULL )
            {
                msg_Err( p_input, "could not add ES %d", p_es->i_pid );
                p_input->b_error = 1;
                return;
            }

            switch( p_es->i_type )
            {
                case MPEG1_VIDEO_ES:
                case MPEG2_VIDEO_ES:
                    p_new_es->i_fourcc = VLC_FOURCC('m','p','g','v');
                    p_new_es->i_cat = VIDEO_ES;
                    break;
                case MPEG1_AUDIO_ES:
                case MPEG2_AUDIO_ES:
                    p_new_es->i_fourcc = VLC_FOURCC('m','p','g','a');
                    p_new_es->i_cat = AUDIO_ES;
                    break;
                case LPCM_AUDIO_ES:
                    p_new_es->i_fourcc = VLC_FOURCC('l','p','c','m');
                    p_new_es->i_cat = AUDIO_ES;
                    p_new_es->i_stream_id = 0xBD;
                    break;
                case A52_AUDIO_ES:
                    p_new_es->i_fourcc = VLC_FOURCC('a','5','2',' ');
                    p_new_es->i_cat = AUDIO_ES;
                    p_new_es->i_stream_id = 0xBD;
                    break;
                case DVD_SPU_ES:
                    p_new_es->i_fourcc = VLC_FOURCC('s','p','u',' ');
                    p_new_es->i_cat = SPU_ES;
                    p_new_es->i_stream_id = 0xBD;
                    break;
                default:
                    p_new_es->i_fourcc = 0;
                    p_new_es->i_cat = UNKNOWN_ES;
            }

            if(    ( p_new_es->i_cat == AUDIO_ES )
                || (p_new_es->i_cat == SPU_ES ) )
            {
                dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;
                while( p_dr && ( p_dr->i_tag != 0x0a ) )
                    p_dr = p_dr->p_next;
                if( p_dr )
                {
                    dvbpsi_iso639_dr_t *p_decoded =
                                                dvbpsi_DecodeISO639Dr( p_dr );
                    if( p_decoded->i_code_count > 0 )
                    {
                        const iso639_lang_t * p_iso;
                        p_iso = GetLang_2T(p_decoded->i_iso_639_code);
                        if(p_iso)
                        {
                            if(p_iso->psz_native_name[0])
                                strcpy( p_new_es->psz_desc,
                                        p_iso->psz_native_name );
                            else
                                strcpy( p_new_es->psz_desc,
                                        p_iso->psz_eng_name );
                        }
                        else
                        {
                            strncpy( p_new_es->psz_desc,
                                     p_decoded->i_iso_639_code, 3 );
                        }
                    }
                }
                switch( p_es->i_type )
                {
                    case MPEG1_AUDIO_ES:
                    case MPEG2_AUDIO_ES:
                        strcat( p_new_es->psz_desc, " (mpeg)" );
                        break;
                    case LPCM_AUDIO_ES:
                        strcat( p_new_es->psz_desc, " (lpcm)" );
                        break;
                    case A52_AUDIO_ES:
                        strcat( p_new_es->psz_desc, " (A52)" );
                        break;
                }
            }

            p_es = p_es->p_next;
        }
        
        /* if no program is selected :*/
        if( !p_input->stream.p_selected_program )
        {
            pgrm_descriptor_t *     p_pgrm_to_select;
            u16 i_id = (u16)config_GetInt( p_input, "program" );

            if( i_id != 0 ) /* if user specified a program */
            {
                p_pgrm_to_select = input_FindProgram( p_input, i_id );

                if( p_pgrm_to_select && p_pgrm_to_select == p_pgrm )
                    p_input->pf_set_program( p_input, p_pgrm_to_select );
            }
            else
                    p_input->pf_set_program( p_input, p_pgrm );
        }
        /* if the pmt belongs to the currently selected program, we
         * reselect it to update its ES */
        else if( p_pgrm == p_input->stream.p_selected_program )
        {
            p_input->pf_set_program( p_input, p_pgrm );
        }
        
        p_pgrm_demux->i_pmt_version = p_new_pmt->i_version;
        p_input->stream.b_changed = 1;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}
#endif
