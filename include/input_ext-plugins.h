/*****************************************************************************
 * input_ext-plugins.h: structures of the input not exported to other modules,
 *                      but exported to plug-ins
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id: input_ext-plugins.h,v 1.33 2002/07/31 20:56:50 sam Exp $
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
VLC_EXPORT( int,  input_InitStream,( input_thread_t *, size_t ) );
VLC_EXPORT( void, input_EndStream, ( input_thread_t * ) );
VLC_EXPORT( pgrm_descriptor_t *, input_FindProgram,( input_thread_t *, u16 ) );
VLC_EXPORT( pgrm_descriptor_t *, input_AddProgram, ( input_thread_t *, u16, size_t ) );
VLC_EXPORT( void, input_DelProgram,( input_thread_t *, pgrm_descriptor_t * ) );
VLC_EXPORT( int, input_SetProgram,( input_thread_t *, pgrm_descriptor_t * ) );
VLC_EXPORT( input_area_t *, input_AddArea,( input_thread_t * ) );
VLC_EXPORT( void, input_DelArea,   ( input_thread_t *, input_area_t * ) );
VLC_EXPORT( es_descriptor_t *, input_FindES,( input_thread_t *, u16 ) );
VLC_EXPORT( es_descriptor_t *, input_AddES, ( input_thread_t *, pgrm_descriptor_t *, u16, size_t ) );
VLC_EXPORT( void, input_DelES,     ( input_thread_t *, es_descriptor_t * ) );
VLC_EXPORT( int,  input_SelectES,  ( input_thread_t *, es_descriptor_t * ) );
VLC_EXPORT( int,  input_UnselectES,( input_thread_t *, es_descriptor_t * ) );

/*****************************************************************************
 * Prototypes from input_dec.c
 *****************************************************************************/
//decoder_capabilities_t * input_ProbeDecoder( void );
decoder_fifo_t * input_RunDecoder( input_thread_t *, es_descriptor_t * );
void input_EndDecoder( input_thread_t *, es_descriptor_t * );
VLC_EXPORT( void, input_DecodePES, ( decoder_fifo_t *, pes_packet_t * ) );
void input_EscapeDiscontinuity( input_thread_t * );
void input_EscapeAudioDiscontinuity( input_thread_t * );

/*****************************************************************************
 * Prototypes from input_clock.c
 *****************************************************************************/
void input_ClockInit( pgrm_descriptor_t * );
VLC_EXPORT( int,  input_ClockManageControl, ( input_thread_t *, pgrm_descriptor_t *, mtime_t ) );
VLC_EXPORT( void, input_ClockManageRef, ( input_thread_t *, pgrm_descriptor_t *, mtime_t ) );
VLC_EXPORT( mtime_t, input_ClockGetTS, ( input_thread_t *, pgrm_descriptor_t *, mtime_t ) );

/*****************************************************************************
 * Prototypes from input_info.c
 *****************************************************************************/
VLC_EXPORT( input_info_category_t *, input_InfoCategory, ( input_thread_t *, char * ) );
VLC_EXPORT( int, input_AddInfo, ( input_info_category_t *, char *, char *, ... ) );
int input_DelInfo( input_thread_t * p_input ); /* no need to export this */
/*****************************************************************************
 * Prototypes from input_ext-plugins.h (buffers management)
 *****************************************************************************/
#define input_BuffersInit(a) __input_BuffersInit(VLC_OBJECT(a))
void * __input_BuffersInit( vlc_object_t * );
VLC_EXPORT( void, input_BuffersEnd, ( input_thread_t *, input_buffers_t * ) );

VLC_EXPORT( data_buffer_t *, input_NewBuffer,   ( input_buffers_t *, size_t ) );
VLC_EXPORT( void, input_ReleaseBuffer,          ( input_buffers_t *, data_buffer_t * ) );
VLC_EXPORT( data_packet_t *, input_ShareBuffer, ( input_buffers_t *, data_buffer_t * ) );
VLC_EXPORT( data_packet_t *, input_NewPacket,   ( input_buffers_t *, size_t ) );
VLC_EXPORT( void, input_DeletePacket,           ( input_buffers_t *, data_packet_t * ) );
VLC_EXPORT( pes_packet_t *, input_NewPES, ( input_buffers_t * ) );
VLC_EXPORT( void, input_DeletePES,        ( input_buffers_t *, pes_packet_t * ) );
VLC_EXPORT( ssize_t, input_FillBuffer,  ( input_thread_t * ) );
VLC_EXPORT( ssize_t, input_Peek,        ( input_thread_t *, byte_t **, size_t ) );
VLC_EXPORT( ssize_t, input_SplitBuffer, ( input_thread_t *, data_packet_t **, size_t ) );
VLC_EXPORT( int, input_AccessInit,      ( input_thread_t * ) );
VLC_EXPORT( void, input_AccessReinit,   ( input_thread_t * ) );
VLC_EXPORT( void, input_AccessEnd,      ( input_thread_t * ) );

/*****************************************************************************
 * Create a NULL packet for padding in case of a data loss
 *****************************************************************************/
static inline void input_NullPacket( input_thread_t * p_input,
                                     es_descriptor_t * p_es )
{
    data_packet_t *             p_pad_data;
    pes_packet_t *              p_pes;

    if( (p_pad_data = input_NewPacket( p_input->p_method_data,
                    PADDING_PACKET_SIZE )) == NULL )
    {
        msg_Err( p_input, "no new packet" );
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
            msg_Err( p_input, "no PES packet" );
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
#define TS_SYNC_CODE        0x47                /* First byte of a TS packet */
#define PSI_SECTION_SIZE    4096            /* Maximum size of a PSI section */

#define PAT_UNINITIALIZED    (1 << 6)
#define PMT_UNINITIALIZED    (1 << 6)

#define PSI_IS_PAT          0x00
#define PSI_IS_PMT          0x01
#define UNKNOWN_PSI         0xff

/****************************************************************************
 * psi_callback_t
 ****************************************************************************
 * Used by TS demux to handle a PSI, either with the builtin decoder, either
 * with a library such as libdvbpsi
 ****************************************************************************/
typedef void( * psi_callback_t )( 
        input_thread_t  * p_input,
        data_packet_t   * p_data,
        es_descriptor_t * p_es,
        vlc_bool_t        b_unit_start );


/*****************************************************************************
 * psi_section_t
 *****************************************************************************
 * Describes a PSI section. Beware, it doesn't contain pointers to the TS
 * packets that contain it as for a PES, but the data themselves
 *****************************************************************************/
typedef struct psi_section_t
{
    byte_t                  buffer[PSI_SECTION_SIZE];

    u8                      i_section_number;
    u8                      i_last_section_number;
    u8                      i_version_number;
    u16                     i_section_length;
    u16                     i_read_in_section;
    
    /* the PSI is complete */
    vlc_bool_t              b_is_complete;
    
    /* packet missed up ? */
    vlc_bool_t              b_trash;

    /*about sections  */ 
    vlc_bool_t              b_section_complete;

    /* where are we currently ? */
    byte_t                * p_current;

} psi_section_t;

/*****************************************************************************
 * es_ts_data_t: extension of es_descriptor_t
 *****************************************************************************/
typedef struct es_ts_data_t
{
    vlc_bool_t              b_psi;   /* Does the stream have to be handled by
                                      *                    the PSI decoder ? */

    int                     i_psi_type;  /* There are different types of PSI */
    
    psi_section_t *         p_psi_section;                    /* PSI packets */

    /* Markers */
    int                     i_continuity_counter;
} es_ts_data_t;

/*****************************************************************************
 * pgrm_ts_data_t: extension of pgrm_descriptor_t
 *****************************************************************************/
typedef struct pgrm_ts_data_t
{
    u16                     i_pcr_pid;             /* PCR ES, for TS streams */
    int                     i_pmt_version;
    /* libdvbpsi pmt decoder handle */
    void *                  p_pmt_handle;
} pgrm_ts_data_t;

/*****************************************************************************
 * stream_ts_data_t: extension of stream_descriptor_t
 *****************************************************************************/
typedef struct stream_ts_data_t
{
    int i_pat_version;          /* Current version of the PAT */
    /* libdvbpsi pmt decoder handle */
    void *                  p_pat_handle;
} stream_ts_data_t;

/*****************************************************************************
 * stream_ps_data_t: extension of stream_descriptor_t
 *****************************************************************************/
typedef struct stream_ps_data_t
{
    vlc_bool_t              b_has_PSM;                 /* very rare, in fact */

    u8                      i_PSM_version;
} stream_ps_data_t;

/* PSM version is 5 bits, so -1 is not a valid value */
#define EMPTY_PSM_VERSION   -1


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void, input_ParsePES,  ( input_thread_t *, es_descriptor_t * ) );
VLC_EXPORT( void, input_GatherPES, ( input_thread_t *, data_packet_t *, es_descriptor_t *, vlc_bool_t, vlc_bool_t ) );
VLC_EXPORT( ssize_t, input_ReadPS, ( input_thread_t *, data_packet_t ** ) );
VLC_EXPORT( es_descriptor_t *, input_ParsePS, ( input_thread_t *, data_packet_t * ) );
VLC_EXPORT( ssize_t, input_ReadTS, ( input_thread_t *, data_packet_t ** ) );
VLC_EXPORT( void, input_DemuxPS,   ( input_thread_t *, data_packet_t * ) );
VLC_EXPORT( void, input_DemuxTS,   ( input_thread_t *, data_packet_t *, void(*) ( input_thread_t *, data_packet_t *, es_descriptor_t *, vlc_bool_t ) ) );

/*
 * Optional standard file descriptor operations (input_ext-plugins.h)
 */

/*****************************************************************************
 * input_socket_t: private access plug-in data
 *****************************************************************************/
struct input_socket_t
{
    /* Unbuffered file descriptor */
    int i_handle;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void, __input_FDClose, ( vlc_object_t * ) );
#define input_FDClose(a) __input_FDClose(VLC_OBJECT(a))
VLC_EXPORT( void, __input_FDNetworkClose, ( vlc_object_t * ) );
#define input_FDNetworkClose(a) __input_FDNetworkClose(VLC_OBJECT(a))
VLC_EXPORT( ssize_t, input_FDRead, ( input_thread_t *, byte_t *, size_t ) );
VLC_EXPORT( ssize_t, input_FDNetworkRead, ( input_thread_t *, byte_t *, size_t ) );
VLC_EXPORT( void, input_FDSeek, ( input_thread_t *, off_t ) );

