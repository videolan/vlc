/*****************************************************************************
 * input_ext-plugins.h: structures of the input not exported to other modules,
 *                      but exported to plug-ins
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: input_ext-plugins.h,v 1.5 2001/11/15 17:39:12 sam Exp $
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
vlc_thread_t input_RunDecoder( struct input_thread_s *,
                               struct es_descriptor_s * );
void input_EndDecoder( struct input_thread_s *, struct es_descriptor_s * );
void input_DecodePES ( struct decoder_fifo_s *, struct pes_packet_s * );
void input_EscapeDiscontinuity( struct input_thread_s *,
                                struct pgrm_descriptor_s * );
void input_EscapeAudioDiscontinuity( struct input_thread_s * );

/*****************************************************************************
 * Prototypes from input_clock.c
 *****************************************************************************/
void input_ClockInit( struct pgrm_descriptor_s * );
int  input_ClockManageControl( struct input_thread_s *,
                               struct pgrm_descriptor_s *, mtime_t );
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


/*
 * Optional netlist management
 */

/*****************************************************************************
 * netlist_t: structure to manage a netlist
 *****************************************************************************/
typedef struct netlist_s
{
    vlc_mutex_t             lock;

    size_t                  i_buffer_size;

    /* Buffers */
    byte_t *                p_buffers;                 /* Big malloc'ed area */
    data_packet_t *         p_data;                        /* malloc'ed area */
    pes_packet_t *          p_pes;                         /* malloc'ed area */

    /* FIFOs of free packets */
    data_packet_t **        pp_free_data;
    pes_packet_t **         pp_free_pes;
    struct iovec *          p_free_iovec;
    
    /* FIFO size */
    unsigned int            i_nb_iovec;
    unsigned int            i_nb_pes;
    unsigned int            i_nb_data;

    /* Index */
    unsigned int            i_iovec_start, i_iovec_end;
    unsigned int            i_data_start, i_data_end;
    unsigned int            i_pes_start, i_pes_end;

    /* Reference counters for iovec */
    unsigned int *          pi_refcount;

    /* Number of blocs read once by readv */
    unsigned int            i_read_once;
} netlist_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int                     input_NetlistInit( struct input_thread_s *,
                                           int i_nb_iovec,
                                           int i_nb_data,
                                           int i_nb_pes,
                                           size_t i_buffer_size,
                                           int i_read_once );

struct iovec * 	        input_NetlistGetiovec( void * p_method_data );
void                    input_NetlistMviovec( void * , int,
                                              struct data_packet_s **);
struct data_packet_s *  input_NetlistNewPtr( void * );
struct data_packet_s *  input_NetlistNewPacket( void *, size_t );
struct pes_packet_s *   input_NetlistNewPES( void * );
void                    input_NetlistDeletePacket( void *,
                                                   struct data_packet_s * );
void                    input_NetlistDeletePES( void *,
                                                struct pes_packet_s * );
void                    input_NetlistEnd( struct input_thread_s * );


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
void input_ParsePES  ( struct input_thread_s *, struct es_descriptor_s * );
void input_GatherPES ( struct input_thread_s *, struct data_packet_s *,
                       struct es_descriptor_s *, boolean_t, boolean_t );
es_descriptor_t * input_ParsePS( struct input_thread_s *,
                                 struct data_packet_s * );
void input_DemuxPS   ( struct input_thread_s *, struct data_packet_s * );
void input_DemuxTS   ( struct input_thread_s *, struct data_packet_s * );
void input_DemuxPSI  ( struct input_thread_s *, struct data_packet_s *,
                       struct es_descriptor_s *, boolean_t, boolean_t );

