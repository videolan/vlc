/*****************************************************************************
 * mpeg4_iod.h: ISO 14496-1 IOD and parsers
 *****************************************************************************
 * Copyright (C) 2004-2015 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef VLC_MPEG4_IOD_H
#define VLC_MPEG4_IOD_H

#define ES_DESCRIPTOR_COUNT 255
typedef enum
{
    USE_ACCESS_UNIT_START_FLAG = 1 << 7,
    USE_ACCESS_UNIT_END_FLAG   = 1 << 6,
    USE_RANDOM_ACCESS_POINT_FLAG  = 1 << 5,
    HAS_RANDOM_ACCESS_UNITS_ONLY_FLAG  = 1 << 4,
    USE_PADDING_FLAG  = 1 << 3,
    USE_TIMESTAMPS_FLAG  = 1 << 2,
    USE_IDLE_FLAG  = 1 << 1,
    USE_DURATION_FLAG  = 1
} sl_config_flags;

typedef struct
{
    uint32_t i_timestamp_resolution;
    uint32_t i_OCR_resolution;
    uint8_t i_flags;
    uint8_t i_timestamp_length;
    uint8_t i_OCR_length;
    uint8_t i_AU_length;
    uint8_t i_instant_bitrate_length;
    uint8_t i_degradation_priority_length;
    uint8_t i_AU_seqnum_length;
    uint8_t i_packet_seqnum_length;

    uint32_t i_timescale;
    uint16_t i_accessunit_duration;
    uint16_t i_compositionunit_duration;

    uint64_t i_startdecoding_timestamp;
    uint64_t i_startcomposition_timestamp;
} sl_config_descriptor_t;

typedef struct
{
    unsigned i_size;
    bool b_au_start;
    bool b_au_end;
    vlc_tick_t i_dts;
    vlc_tick_t i_pts;
} sl_header_data;

typedef struct es_mpeg4_descriptor_t es_mpeg4_descriptor_t;
typedef struct decoder_config_descriptor_t decoder_config_descriptor_t;

struct decoder_config_descriptor_t
{
    uint8_t                 i_objectTypeIndication;
    uint8_t                 i_streamType;

    unsigned                i_extra;
    uint8_t                 *p_extra;

};

struct es_mpeg4_descriptor_t
{
    bool                    b_ok;
    uint16_t                i_es_id;

    char                    *psz_url;

    decoder_config_descriptor_t    dec_descr;
    sl_config_descriptor_t         sl_descr;

};

typedef struct
{
    uint16_t                i_ID;
    char                    *psz_url;

    es_mpeg4_descriptor_t   es_descr[ES_DESCRIPTOR_COUNT];

} od_descriptor_t;

typedef struct
{
    int i_version;
    DECL_ARRAY(od_descriptor_t *) objects;
} od_descriptors_t;

od_descriptor_t *IODNew( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data );
void ODFree( od_descriptor_t *p_od );

sl_header_data DecodeSLHeader( unsigned i_data, const uint8_t *p_data,
                               const sl_config_descriptor_t *sl );
void DecodeODCommand( vlc_object_t *p_object, od_descriptors_t *,
                      unsigned i_data, const uint8_t *p_data );
#endif
