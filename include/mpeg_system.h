/*****************************************************************************
 * mpeg_system.h: structures of the input used to parse MPEG-1, MPEG-2 PS
 * and TS system layers
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: mpeg_system.h,v 1.1 2001/02/08 04:43:27 sam Exp $
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
 * Constants
 *****************************************************************************/
#define TS_PACKET_SIZE      188                       /* Size of a TS packet */
#define PSI_SECTION_SIZE    4096            /* Maximum size of a PSI section */


/*****************************************************************************
 * psi_section_t
 *****************************************************************************
 * Describes a PSI section. Beware, it doesn't contain pointers to the TS
 * packets that contain it as for a PES, but the data themselves
 *****************************************************************************/
typedef struct psi_section_s
{
    byte_t                  buffer[PSI_SECTION_SIZE];

    /* Is there a section being decoded ? */
    boolean_t               b_running_section;

    u16                     i_length;
    u16                     i_current_position;
} psi_section_t;

/*****************************************************************************
 * es_ts_data_t: extension of es_descriptor_t
 *****************************************************************************/
typedef struct es_ts_data_s
{
    boolean_t               b_psi;   /* Does the stream have to be handled by
                                      *                    the PSI decoder ? */
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
} pgrm_ts_data_t;

/*****************************************************************************
 * stream_ts_data_t: extension of stream_descriptor_t
 *****************************************************************************/
typedef struct stream_ts_data_s
{
    /* Program Association Table status */
    u8                      i_PAT_version;                 /* version number */
    boolean_t               b_is_PAT_complete;      /* Is the PAT complete ? */
    u8                      i_known_PAT_sections;
                                     /* Number of section we received so far */
    byte_t                  a_known_PAT_sections[32];
                                                /* Already received sections */

    /* Program Map Table status */
    boolean_t               b_is_PMT_complete;      /* Is the PMT complete ? */
    u8                      i_known_PMT_sections;
                                     /* Number of section we received so far */
    byte_t                  a_known_PMT_sections[32];
                                                /* Already received sections */

    /* Service Description Table status */
    u8                      i_SDT_version;                 /* version number */
    boolean_t               b_is_SDT_complete;      /* Is the SDT complete ? */
    u8                      i_known_SDT_sections;
                                     /* Number of section we received so far */
    byte_t                  a_known_SDT_sections[32];
                                                /* Already received sections */
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
void input_ParsePES( struct input_thread_s *, struct es_descriptor_s * );
void input_GatherPES( struct input_thread_s *, struct data_packet_s *,
                      struct es_descriptor_s *, boolean_t, boolean_t );
es_descriptor_t * input_ParsePS( struct input_thread_s *,
                                 struct data_packet_s * );
void input_DemuxPS( struct input_thread_s *, struct data_packet_s * );
void input_DemuxTS( struct input_thread_s *, struct data_packet_s * );
