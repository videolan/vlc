/*****************************************************************************
 * input.h: structures of the input not exported to other modules
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input.h,v 1.36 2001/04/25 10:22:32 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

/*
 * Communication plugin -> input
 */

/* FIXME: you've gotta move this move this, you've gotta move this move this */
#define INPUT_READ_ONCE     7   /* We live in a world dominated by Ethernet. *
                                 * Ethernet MTU is 1500 bytes, so in a UDP   *
                                 * packet we can put : 1500/188 = 7 TS       *
                                 * packets. Have a nice day and merry Xmas.  */
#define PADDING_PACKET_SIZE 188 /* Size of the NULL packet inserted in case
                                 * of data loss (this should be < 188).      */
#define PADDING_PACKET_NUMBER 10 /* Number of padding packets top insert to
                                  * escape a decoder.                        */
#define NO_SEEK             -1

/*****************************************************************************
 * Prototypes from input_ext-dec.c
 *****************************************************************************/
void InitBitstream  ( struct bit_stream_s *, struct decoder_fifo_s *,
                      void (* pf_bitstream_callback)( struct bit_stream_s *,
                                                      boolean_t ),
                      void * p_callback_arg );
void NextDataPacket ( struct bit_stream_s * );

/*****************************************************************************
 * Prototypes from input.c to open files
 *****************************************************************************/
void input_FileOpen ( struct input_thread_s * );
void input_FileClose( struct input_thread_s * );

/*****************************************************************************
 * Prototypes from input.c to open a network socket 
 *****************************************************************************/
#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
void input_NetworkOpen ( struct input_thread_s * );
void input_NetworkClose( struct input_thread_s * );
#endif

/*****************************************************************************
 * Prototypes from input_programs.c
 *****************************************************************************/
int  input_InitStream( struct input_thread_s *, size_t );
void input_EndStream ( struct input_thread_s * );
struct pgrm_descriptor_s * input_FindProgram( struct input_thread_s *, u16 );
struct pgrm_descriptor_s * input_AddProgram ( struct input_thread_s *,
                                              u16, size_t );
void input_DelProgram( struct input_thread_s *, struct pgrm_descriptor_s * );
struct input_area_s * input_AddArea( struct input_thread_s * );
void input_DelArea   ( struct input_thread_s *, struct input_area_s * );
struct es_descriptor_s * input_FindES( struct input_thread_s *, u16 );
struct es_descriptor_s * input_AddES ( struct input_thread_s *,
                                       struct pgrm_descriptor_s *, u16,
                                       size_t );
void input_DelES     ( struct input_thread_s *, struct es_descriptor_s * );
int  input_SelectES  ( struct input_thread_s *, struct es_descriptor_s * );
int  input_UnselectES( struct input_thread_s *, struct es_descriptor_s * );

/*****************************************************************************
 * Prototypes from input_dec.c
 *****************************************************************************/
//decoder_capabilities_s * input_ProbeDecoder( void );
vlc_thread_t input_RunDecoder( struct decoder_capabilities_s *, void * );
void input_EndDecoder( struct input_thread_s *, struct es_descriptor_s * );
void input_DecodePES ( struct decoder_fifo_s *, struct pes_packet_s * );
void input_EscapeDiscontinuity( struct input_thread_s *,
                                struct pgrm_descriptor_s * );
void input_EscapeAudioDiscontinuity( struct input_thread_s *,
                                     struct pgrm_descriptor_s * );

/*****************************************************************************
 * Prototypes from input_clock.c
 *****************************************************************************/
void input_ClockInit( struct pgrm_descriptor_s * );
void input_ClockManageRef( struct input_thread_s *,
                           struct pgrm_descriptor_s *, mtime_t );
mtime_t input_ClockGetTS( struct input_thread_s *,
                          struct pgrm_descriptor_s *, mtime_t );

/*****************************************************************************
 * Create a NULL packet for padding in case of a data loss
 *****************************************************************************/
static __inline__ void input_NullPacket( input_thread_t * p_input,
                                         es_descriptor_t * p_es )
{
    data_packet_t *             p_pad_data;
    pes_packet_t *              p_pes;

    if( (p_pad_data = p_input->pf_new_packet(
                    p_input->p_method_data,
                    PADDING_PACKET_SIZE )) == NULL )
    {
        intf_ErrMsg("Out of memory");
        p_input->b_error = 1;
        return;
    }

    memset( p_pad_data->p_buffer, 0, PADDING_PACKET_SIZE );
    p_pad_data->b_discard_payload = 1;
    p_pes = p_es->p_pes;

    if( p_pes != NULL )
    {
        p_pes->b_discontinuity = 1;
        p_es->p_last->p_next = p_pad_data;
        p_es->p_last = p_pad_data;
    }
    else
    {
        if( (p_pes = p_input->pf_new_pes( p_input->p_method_data )) == NULL )
        {
            intf_ErrMsg("Out of memory");
            p_input->b_error = 1;
            return;
        }

        p_pes->i_rate = p_input->stream.control.i_rate;
        p_pes->p_first = p_pad_data;
        p_pes->b_discontinuity = 1;
        input_DecodePES( p_es->p_decoder_fifo, p_pes );
    }
}

