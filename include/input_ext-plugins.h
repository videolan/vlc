/*****************************************************************************
 * input_ext-plugins.h: structures of the input not exported to other modules,
 *                      but exported to plug-ins
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id: input_ext-plugins.h,v 1.18 2002/03/01 00:33:17 massiot Exp $
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
#define PADDING_PACKET_SIZE 188 /* Size of the NULL packet inserted in case
                                 * of data loss (this should be < 188).      */
#define PADDING_PACKET_NUMBER 10 /* Number of padding packets top insert to
                                  * escape a decoder.                        */
#define INPUT_DEFAULT_BUFSIZE 65536 /* Default buffer size to use when none
                                     * is natural.                           */
#define NO_SEEK             -1

/*****************************************************************************
 * Prototypes from input_programs.c
 *****************************************************************************/
#ifndef PLUGIN
int  input_InitStream( struct input_thread_s *, size_t );
void input_EndStream ( struct input_thread_s * );
struct pgrm_descriptor_s * input_FindProgram( struct input_thread_s *, u16 );
struct pgrm_descriptor_s * input_AddProgram ( struct input_thread_s *,
                                              u16, size_t );
void input_DelProgram( struct input_thread_s *, struct pgrm_descriptor_s * );
int input_SetProgram( struct input_thread_s *, struct pgrm_descriptor_s * );
struct input_area_s * input_AddArea( struct input_thread_s * );
void input_DelArea   ( struct input_thread_s *, struct input_area_s * );
struct es_descriptor_s * input_FindES( struct input_thread_s *, u16 );
struct es_descriptor_s * input_AddES ( struct input_thread_s *,
                                       struct pgrm_descriptor_s *, u16,
                                       size_t );
void input_DelES     ( struct input_thread_s *, struct es_descriptor_s * );
int  input_SelectES  ( struct input_thread_s *, struct es_descriptor_s * );
int  input_UnselectES( struct input_thread_s *, struct es_descriptor_s * );
#else
#   define input_InitStream p_symbols->input_InitStream
#   define input_EndStream p_symbols->input_EndStream
#   define input_SetProgram p_symbols->input_SetProgram
#   define input_FindES p_symbols->input_FindES
#   define input_AddES p_symbols->input_AddES
#   define input_DelES p_symbols->input_DelES
#   define input_SelectES p_symbols->input_SelectES
#   define input_UnselectES p_symbols->input_UnselectES
#   define input_AddProgram p_symbols->input_AddProgram
#   define input_DelProgram p_symbols->input_DelProgram
#   define input_AddArea p_symbols->input_AddArea
#   define input_DelArea p_symbols->input_DelArea
#endif

/*****************************************************************************
 * Prototypes from input_dec.c
 *****************************************************************************/
#ifndef PLUGIN
//decoder_capabilities_s * input_ProbeDecoder( void );
vlc_thread_t input_RunDecoder( struct input_thread_s *,
                               struct es_descriptor_s * );
void input_EndDecoder( struct input_thread_s *, struct es_descriptor_s * );
void input_DecodePES ( struct decoder_fifo_s *, struct pes_packet_s * );
void input_EscapeDiscontinuity( struct input_thread_s *,
                                struct pgrm_descriptor_s * );
void input_EscapeAudioDiscontinuity( struct input_thread_s * );
#else
#   define input_DecodePES p_symbols->input_DecodePES
#endif

/*****************************************************************************
 * Prototypes from input_clock.c
 *****************************************************************************/
#ifndef PLUGIN
void input_ClockInit( struct pgrm_descriptor_s * );
int  input_ClockManageControl( struct input_thread_s *,
                               struct pgrm_descriptor_s *, mtime_t );
void input_ClockManageRef( struct input_thread_s *,
                           struct pgrm_descriptor_s *, mtime_t );
mtime_t input_ClockGetTS( struct input_thread_s *,
                          struct pgrm_descriptor_s *, mtime_t );
#else
#   define input_ClockManageControl p_symbols->input_ClockManageControl
#endif

/*****************************************************************************
 * Prototypes from input_ext-plugins.h (buffers management)
 *****************************************************************************/
#ifndef PLUGIN
void * input_BuffersInit( void );
void input_BuffersEnd( struct input_buffers_s * );
struct data_buffer_s * input_NewBuffer( struct input_buffers_s *, size_t );
void input_ReleaseBuffer( struct input_buffers_s *, struct data_buffer_s * );
struct data_packet_s * input_ShareBuffer( struct input_buffers_s *,
                                          struct data_buffer_s * );
struct data_packet_s * input_NewPacket( struct input_buffers_s *, size_t );
void input_DeletePacket( struct input_buffers_s *, struct data_packet_s * );
struct pes_packet_s * input_NewPES( struct input_buffers_s * );
void input_DeletePES( struct input_buffers_s *, struct pes_packet_s * );
ssize_t input_FillBuffer( struct input_thread_s * );
ssize_t input_Peek( struct input_thread_s *, byte_t **, size_t );
ssize_t input_SplitBuffer( struct input_thread_s *, data_packet_t **, size_t );
int input_AccessInit( struct input_thread_s * );
void input_AccessReinit( struct input_thread_s * );
void input_AccessEnd( struct input_thread_s * );
#else
#   define input_BuffersInit p_symbols->input_BuffersInit
#   define input_BuffersEnd p_symbols->input_BuffersEnd
#   define input_NewBuffer p_symbols->input_NewBuffer
#   define input_ReleaseBuffer p_symbols->input_ReleaseBuffer
#   define input_ShareBuffer p_symbols->input_ShareBuffer
#   define input_NewPacket p_symbols->input_NewPacket
#   define input_DeletePacket p_symbols->input_DeletePacket
#   define input_NewPES p_symbols->input_NewPES
#   define input_DeletePES p_symbols->input_DeletePES
#   define input_FillBuffer p_symbols->input_FillBuffer
#   define input_Peek p_symbols->input_Peek
#   define input_SplitBuffer p_symbols->input_SplitBuffer
#   define input_AccessInit p_symbols->input_AccessInit
#   define input_AccessReinit p_symbols->input_AccessReinit
#   define input_AccessEnd p_symbols->input_AccessEnd
#endif

/*****************************************************************************
 * Create a NULL packet for padding in case of a data loss
 *****************************************************************************/
static __inline__ void input_NullPacket( input_thread_t * p_input,
                                         es_descriptor_t * p_es )
{
    data_packet_t *             p_pad_data;
    pes_packet_t *              p_pes;

    if( (p_pad_data = input_NewPacket( p_input->p_method_data,
                    PADDING_PACKET_SIZE )) == NULL )
    {
        intf_ErrMsg("Out of memory");
        p_input->b_error = 1;
        return;
    }

    memset( p_pad_data->p_payload_start, 0, PADDING_PACKET_SIZE );
    p_pad_data->b_discard_payload = 1;
    p_pes = p_es->p_pes;

    if( p_pes != NULL )
    {
        p_pes->b_discontinuity = 1;
        p_pes->p_last->p_next = p_pad_data;
        p_pes->p_last = p_pad_data;
        p_pes->i_nb_data++;
    }
    else
    {
        if( (p_pes = input_NewPES( p_input->p_method_data )) == NULL )
        {
            intf_ErrMsg("Out of memory");
            p_input->b_error = 1;
            return;
        }

        p_pes->i_rate = p_input->stream.control.i_rate;
        p_pes->p_first = p_pes->p_last = p_pad_data;
        p_pes->i_nb_data = 1;
        p_pes->b_discontinuity = 1;
        input_DecodePES( p_es->p_decoder_fifo, p_pes );
    }
}


/*
 * Optional MPEG demultiplexing
 */

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define TS_PACKET_SIZE      188                       /* Size of a TS packet */
#define PSI_SECTION_SIZE    4096            /* Maximum size of a PSI section */

#define PAT_UNINITIALIZED    (1 << 6)
#define PMT_UNINITIALIZED    (1 << 6)

#define PSI_IS_PAT          0x00
#define PSI_IS_PMT          0x01
#define UNKNOWN_PSI         0xff

/*****************************************************************************
 * psi_section_t
 *****************************************************************************
 * Describes a PSI section. Beware, it doesn't contain pointers to the TS
 * packets that contain it as for a PES, but the data themselves
 *****************************************************************************/
typedef struct psi_section_s
{
    byte_t                  buffer[PSI_SECTION_SIZE];

    u8                      i_section_number;
    u8                      i_last_section_number;
    u8                      i_version_number;
    u16                     i_section_length;
    u16                     i_read_in_section;
    
    /* the PSI is complete */
    boolean_t               b_is_complete;
    
    /* packet missed up ? */
    boolean_t               b_trash;

    /*about sections  */ 
    boolean_t               b_section_complete;

    /* where are we currently ? */
    byte_t                * p_current;

} psi_section_t;

/*****************************************************************************
 * es_ts_data_t: extension of es_descriptor_t
 *****************************************************************************/
typedef struct es_ts_data_s
{
    boolean_t               b_psi;   /* Does the stream have to be handled by
                                      *                    the PSI decoder ? */

    int                     i_psi_type;  /* There are different types of PSI */
    
    psi_section_t *         p_psi_section;                    /* PSI packets */

    /* Markers */
    int                     i_continuity_counter;
} es_ts_data_t;

/*****************************************************************************
 * pgrm_ts_data_t: extension of pgrm_descriptor_t
 *****************************************************************************/
typedef struct pgrm_ts_data_s
{
    u16                     i_pcr_pid;             /* PCR ES, for TS streams */
    int                     i_pmt_version;
} pgrm_ts_data_t;

/*****************************************************************************
 * stream_ts_data_t: extension of stream_descriptor_t
 *****************************************************************************/
typedef struct stream_ts_data_s
{
    int i_pat_version;          /* Current version of the PAT */
} stream_ts_data_t;

/*****************************************************************************
 * stream_ps_data_t: extension of stream_descriptor_t
 *****************************************************************************/
typedef struct stream_ps_data_s
{
    boolean_t               b_has_PSM;                 /* very rare, in fact */

    u8                      i_PSM_version;
} stream_ps_data_t;

/* PSM version is 5 bits, so -1 is not a valid value */
#define EMPTY_PSM_VERSION   -1


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#ifndef PLUGIN
void input_ParsePES  ( struct input_thread_s *, struct es_descriptor_s * );
void input_GatherPES ( struct input_thread_s *, struct data_packet_s *,
                       struct es_descriptor_s *, boolean_t, boolean_t );
es_descriptor_t * input_ParsePS( struct input_thread_s *,
                                 struct data_packet_s * );
void input_DemuxPS   ( struct input_thread_s *, struct data_packet_s * );
void input_DemuxTS   ( struct input_thread_s *, struct data_packet_s * );
void input_DemuxPSI  ( struct input_thread_s *, struct data_packet_s *,
                       struct es_descriptor_s *, boolean_t, boolean_t );
#else
#   define input_ParsePES p_symbols->input_ParsePES
#   define input_GatherPES p_symbols->input_GatherPES
#   define input_ParsePS p_symbols->input_ParsePS
#   define input_DemuxPS p_symbols->input_DemuxPS
#   define input_DemuxTS p_symbols->input_DemuxTS
#   define input_DemuxPSI p_symbols->input_DemuxPSI
#endif


/*
 * Optional standard file descriptor operations (input_ext-plugins.h)
 */

/*****************************************************************************
 * input_socket_t: private access plug-in data
 *****************************************************************************/
typedef struct input_socket_s
{
    /* Unbuffered file descriptor */
    int i_handle;
} input_socket_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#ifndef PLUGIN
void input_FDClose( struct input_thread_s * );
ssize_t input_FDRead( input_thread_t *, byte_t *, size_t );
int input_FDNetworkRead( input_thread_t *, byte_t *, size_t );
void input_FDSeek( struct input_thread_s *, off_t );
#else
#   define input_FDClose p_symbols->input_FDClose
#   define input_FDRead p_symbols->input_FDRead
#   define input_FDNetworkRead p_symbols->input_FDNetworkRead
#   define input_FDSeek p_symbols->input_FDSeek
#endif

