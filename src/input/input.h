/*****************************************************************************
 * input.h: structures of the input not exported to other modules
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input.h,v 1.10 2001/01/15 08:07:31 sam Exp $
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

/*
 * Communication plugin -> input
 */

/* FIXME: you've gotta move this move this, you've gotta move this move this */
#define INPUT_READ_ONCE     7   /* We live in a world dominated by Ethernet. *
                                 * Ethernet MTU is 1500 bytes, so in a UDP   *
                                 * packet we can put : 1500/188 = 7 TS       *
                                 * packets. Have a nice day and merry Xmas.  */
#define PADDING_PACKET_SIZE 100 /* Size of the NULL packet inserted in case
                                 * of data loss (this should be < 188).      */

/*****************************************************************************
 * input_capabilities_t
 *****************************************************************************
 * This structure gives pointers to the useful methods of the plugin
 *****************************************************************************/
typedef struct input_capabilities_s
{
    /* Plugin properties */
    int                     i_weight; /* for a given stream type, the plugin *
                                       * with higher weight will be used     */

    /* Init/End */
    int                  (* pf_probe)( struct input_thread_s * );
    void                 (* pf_init)( struct input_thread_s * );
    void                 (* pf_end)( struct input_thread_s * );

    /* Read & Demultiplex */
    int                  (* pf_read)( struct input_thread_s *,
                          struct data_packet_s * pp_packets[INPUT_READ_ONCE] );
    void                 (* pf_demux)( struct input_thread_s *,
                                       struct data_packet_s * );

    /* Packet management facilities */
    struct data_packet_s *(* pf_new_packet)( void *, size_t );
    struct pes_packet_s *(* pf_new_pes)( void * );
    void                 (* pf_delete_packet)( void *,
                                               struct data_packet_s * );
    void                 (* pf_delete_pes)( void *, struct pes_packet_s * );

    /* Stream control capabilities */
    int                  (* pf_rewind)( struct input_thread_s * );
                                           /* NULL if we don't support going *
                                            * backwards (it's gonna be fun)  */
    int                  (* pf_seek)( struct input_thread_s *, off_t );
} input_capabilities_t;

/*****************************************************************************
 * Prototypes from input_ext-dec.c
 *****************************************************************************/
void InitBitstream  ( struct bit_stream_s *, struct decoder_fifo_s * );
void NextDataPacket ( struct bit_stream_s * );

/*****************************************************************************
 * Prototypes from input_programs.c
 *****************************************************************************/
void input_InitStream( struct input_thread_s *, size_t );
void input_EndStream( struct input_thread_s * );
struct pgrm_descriptor_s * input_FindProgram( struct input_thread_s *, u16 );
struct pgrm_descriptor_s * input_AddProgram( struct input_thread_s *,
                                             u16, size_t );
void input_DelProgram( struct input_thread_s *, struct pgrm_descriptor_s * );
void input_DumpStream( struct input_thread_s * );
struct es_descriptor_s * input_FindES( struct input_thread_s *, u16 );
struct es_descriptor_s * input_AddES( struct input_thread_s *,
                                      struct pgrm_descriptor_s *, u16,
                                      size_t );
void input_DelES( struct input_thread_s *, struct es_descriptor_s * );
int input_SelectES( struct input_thread_s *, struct es_descriptor_s * );

/*****************************************************************************
 * Prototypes from input_dec.c
 *****************************************************************************/
//decoder_capabilities_s * input_ProbeDecoder( void );
vlc_thread_t input_RunDecoder( struct decoder_capabilities_s *, void * );
void input_EndDecoder( struct input_thread_s *, struct es_descriptor_s * );
void input_DecodePES( struct decoder_fifo_s *, struct pes_packet_s * );

/*****************************************************************************
 * Create a NULL packet for padding in case of a data loss
 *****************************************************************************/
static __inline__ void input_NullPacket( input_thread_t * p_input,
                                         es_descriptor_t * p_es )
{
    data_packet_t *             p_pad_data;
    pes_packet_t *              p_pes;

    if( (p_pad_data = p_input->p_plugin->pf_new_packet(
                    p_input->p_method_data,
                    PADDING_PACKET_SIZE )) == NULL )
    {
        intf_ErrMsg("Out of memory");
        p_input->b_error = 1;
        return;
    }

    /* XXX FIXME SARASS TODO: remove the following one-liner kludge when
     * we have bitstream IV, we won't need it anymore */
    ((WORD_TYPE *)p_pad_data->p_payload_start)++;

    memset( p_pad_data->p_buffer, 0, PADDING_PACKET_SIZE );
    p_pad_data->b_discard_payload = 1;
    p_pes = p_es->p_pes;

    if( p_pes != NULL )
    {
        p_pes->b_messed_up = 1;
        p_es->p_last->p_next = p_pad_data;
        p_es->p_last = p_pad_data;
    }
    else
    {
        if( (p_pes = p_input->p_plugin->pf_new_pes(
                                        p_input->p_method_data )) == NULL )
        {
            intf_ErrMsg("Out of memory");
            p_input->b_error = 1;
            return;
        }

        p_pes->p_first = p_pad_data;
        p_pes->b_messed_up = p_pes->b_discontinuity = 1;
        input_DecodePES( p_es->p_decoder_fifo, p_pes );
    }

    p_es->b_discontinuity = 0;
}

