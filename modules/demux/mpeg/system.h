/*****************************************************************************
 * system.h: MPEG demultiplexing.
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id: system.h,v 1.1 2002/08/07 00:29:36 sam Exp $
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


/****************************************************************************
 * mpeg_demux_t
 ****************************************************************************
 * Demux callbacks exported by the helper plugin
 ****************************************************************************/
typedef struct mpeg_demux_t
{
    module_t * p_module;

    ssize_t           (*pf_read_ps)  ( input_thread_t *, data_packet_t ** );
    es_descriptor_t * (*pf_parse_ps) ( input_thread_t *, data_packet_t * );
    void              (*pf_demux_ps) ( input_thread_t *, data_packet_t * );

    ssize_t           (*pf_read_ts)  ( input_thread_t *, data_packet_t ** );
    void              (*pf_demux_ts) ( input_thread_t *, data_packet_t *,
                                       psi_callback_t );
} mpeg_demux_t;

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

