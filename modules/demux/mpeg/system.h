/*****************************************************************************
 * system.h: MPEG demultiplexing.
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id: system.h,v 1.4 2003/01/08 16:40:29 fenrir Exp $
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

/* ES streams types - see ISO/IEC 13818-1 table 2-29 numbers.
 * these values are used in mpeg_system.c, and in
 * the following plugins: mpeg_ts, mpeg_ts_dvbpsi, satellite. */
#define MPEG1_VIDEO_ES      0x01
#define MPEG2_VIDEO_ES      0x02
#define MPEG1_AUDIO_ES      0x03
#define MPEG2_AUDIO_ES      0x04
#define A52DVB_AUDIO_ES     0x06

#define MPEG4_VIDEO_ES      0x10
#define MPEG4_AUDIO_ES      0x11

#define A52_AUDIO_ES        0x81
/* These ones might violate the usage : */
#define DVD_SPU_ES          0x82
#define LPCM_AUDIO_ES       0x83
#define SDDS_AUDIO_ES       0x84
#define DTS_AUDIO_ES        0x85
/* These ones are only here to work around a bug in VLS - VLS doesn't
 * skip the first bytes of the PES payload (stream private ID) when
 * streaming. This is incompatible with all equipments. 'B' is for
 * buggy. Please note that they are associated with FOURCCs '***b'.
 * --Meuuh 2002-08-30
 */
#define A52B_AUDIO_ES       0x91
#define DVDB_SPU_ES         0x92
#define LPCMB_AUDIO_ES      0x93

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
 * decoder_descriptor_t
 *****************************************************************************/
typedef struct decoder_config_descriptor_s
{
    uint8_t                 i_objectTypeIndication;
    uint8_t                 i_streamType;
    vlc_bool_t              b_upStream;
    uint32_t                i_bufferSizeDB;
    uint32_t                i_maxBitrate;
    uint32_t                i_avgBitrate;

    int                     i_decoder_specific_info_len;
    uint8_t                 *p_decoder_specific_info;

} decoder_config_descriptor_t;

/*****************************************************************************
 * sl_descriptor_t:
 *****************************************************************************/
typedef struct sl_config_descriptor_s
{
    vlc_bool_t              b_useAccessUnitStartFlag;
    vlc_bool_t              b_useAccessUnitEndFlag;
    vlc_bool_t              b_useRandomAccessPointFlag;
    vlc_bool_t              b_useRandomAccessUnitsOnlyFlag;
    vlc_bool_t              b_usePaddingFlag;
    vlc_bool_t              b_useTimeStampsFlags;
    vlc_bool_t              b_useIdleFlag;
    vlc_bool_t              b_durationFlag;
    uint32_t                i_timeStampResolution;
    uint32_t                i_OCRResolution;
    uint8_t                 i_timeStampLength;
    uint8_t                 i_OCRLength;
    uint8_t                 i_AU_Length;
    uint8_t                 i_instantBitrateLength;
    uint8_t                 i_degradationPriorityLength;
    uint8_t                 i_AU_seqNumLength;
    uint8_t                 i_packetSeqNumLength;

    uint32_t                i_timeScale;
    uint16_t                i_accessUnitDuration;
    uint16_t                i_compositionUnitDuration;

    uint64_t                i_startDecodingTimeStamp;
    uint64_t                i_startCompositionTimeStamp;

} sl_config_descriptor_t;

/*****************************************************************************
 * es_mpeg4_descriptor_t: XXX it's not complete but should be enough
 *****************************************************************************/
typedef struct es_mpeg4_descriptor_s
{
    vlc_bool_t              b_ok;
    uint16_t                i_es_id;

    vlc_bool_t              b_streamDependenceFlag;
    vlc_bool_t              b_OCRStreamFlag;
    uint8_t                 i_streamPriority;

    char                    *psz_url;

    uint16_t                i_dependOn_es_id;
    uint16_t                i_OCR_es_id;

    decoder_config_descriptor_t    dec_descr;
    sl_config_descriptor_t         sl_descr;
} es_mpeg4_descriptor_t;

/*****************************************************************************
 * iod_descriptor_t: XXX it's not complete but should be enough
 *****************************************************************************/
typedef struct iod_descriptor_s
{
    uint8_t                i_iod_label;

    /* IOD */
    uint16_t                i_od_id;
    char                    *psz_url;

    uint8_t                 i_ODProfileLevelIndication;
    uint8_t                 i_sceneProfileLevelIndication;
    uint8_t                 i_audioProfileLevelIndication;
    uint8_t                 i_visualProfileLevelIndication;
    uint8_t                 i_graphicsProfileLevelIndication;

    es_mpeg4_descriptor_t   es_descr[255];

} iod_descriptor_t;

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

    /* mpeg4 in TS data specific */
    int                     b_mpeg4;
    uint16_t                i_es_id;

    es_mpeg4_descriptor_t   *p_es_descr;   /* es_descr of IOD */

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

    /* mpeg4 in TS data specific */
    vlc_bool_t              b_mpeg4;
    iod_descriptor_t        iod;

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

