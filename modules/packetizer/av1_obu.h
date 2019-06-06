/*****************************************************************************
 * av1_obu: AV1 OBU parser
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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
#ifndef VLC_AV1_OBU_H
#define VLC_AV1_OBU_H

static inline uint64_t leb128(const uint8_t *p_buf, size_t i_buf, uint8_t *pi_len)
{
    uint64_t i_val = 0;
    *pi_len = 0;
    for(size_t i=0; i<8; i++)
    {
        if(i >= i_buf)
            break;
        i_val |= ((uint64_t)(p_buf[i] & 0x7F) << (i * 7));
        if((p_buf[i] & 0x80) == 0)
        {
            *pi_len = i + 1;
            break;
        }
    }
    return *pi_len ? i_val : 0;
}

enum av1_obu_type_e
{
    AV1_OBU_RESERVED_0             = 0,
    AV1_OBU_SEQUENCE_HEADER        = 1,
    AV1_OBU_TEMPORAL_DELIMITER     = 2,
    AV1_OBU_FRAME_HEADER           = 3,
    AV1_OBU_TILE_GROUP             = 4,
    AV1_OBU_METADATA               = 5,
    AV1_OBU_FRAME                  = 6,
    AV1_OBU_REDUNDANT_FRAME_HEADER = 7,
    AV1_OBU_TILE_LIST              = 8,
    AV1_OBU_RESERVED_START_9       = 9,
    AV1_OBU_RESERVED_END_14        = 14,
    AV1_OBU_PADDING                = 15,
};

static inline enum av1_obu_type_e AV1_OBUGetType(const uint8_t *p_buf)
{
    return (enum av1_obu_type_e)((p_buf[0] >> 3) & 0x0F);
}

static inline bool AV1_OBUHasSizeField(const uint8_t *p_buf)
{
    return p_buf[0] & 0x02;
}

static inline bool AV1_OBUHasExtensionField(const uint8_t *p_buf)
{
    return p_buf[0] & 0x04;
}

static inline bool AV1_OBUIsValid(const uint8_t *p_buf, size_t i_buf)
{
    return (i_buf > 0 && (p_buf[0] & 0x81) == 0);
}

static inline bool AV1_OBUIsBaseLayer(const uint8_t *p_buf, size_t i_buf)
{
    return !AV1_OBUHasExtensionField(p_buf) || (i_buf < 2) || !(p_buf[1] >> 3);
}

static uint32_t AV1_OBUSize(const uint8_t *p_buf, size_t i_buf, uint8_t *pi_len)
{
    if(!AV1_OBUHasSizeField(p_buf))
    {
        if(AV1_OBUHasExtensionField(p_buf) && i_buf < 2)
            return false;
        return i_buf - 1 - AV1_OBUHasExtensionField(p_buf);
    }

    if(AV1_OBUHasExtensionField(p_buf))
    {
        if(i_buf == 1)
        {
            *pi_len = 0;
            return 0;
        }
        /* skip extension header */
        p_buf += 1;
        i_buf -= 1;
    }
    uint64_t i_size = leb128(&p_buf[1], i_buf - 1, pi_len);
    if(i_size > (INT64_C(1) << 32) - 1)
    {
        *pi_len = 0;
        return 0;
    }
    return i_size;
}

static bool AV1_OBUSkipHeader(const uint8_t **pp_buf, size_t *pi_buf)
{
    if(*pi_buf < 1)
        return false;
    size_t i_header = 1 + !!AV1_OBUHasExtensionField(*pp_buf);
    if(AV1_OBUHasSizeField(*pp_buf))
    {
        uint8_t i_len;
        (void) AV1_OBUSize(*pp_buf, *pi_buf, &i_len);
        if(i_len == 0)
            return false;
        i_header += i_len;
    }
    if(i_header > *pi_buf)
        return false;
    *pp_buf += i_header;
    *pi_buf -= i_header;
    return true;
}

/* METADATA properties */
enum av1_obu_metadata_type_e
{
    AV1_METADATA_TYPE_RESERVED             = 0,
    AV1_METADATA_TYPE_HDR_CLL              = 1,
    AV1_METADATA_TYPE_HDR_MDCV             = 2,
    AV1_METADATA_TYPE_SCALABILITY          = 3,
    AV1_METADATA_TYPE_ITUT_T35             = 4,
    AV1_METADATA_TYPE_TIMECODE             = 5,
    AV1_METADATA_TYPE_USER_PRIVATE_START_6 = 6,
    AV1_METADATA_TYPE_USER_PRIVATE_END_31  = 31,
    AV1_METADATA_TYPE_RESERVED_START_32    = 32,
};

static inline enum av1_obu_metadata_type_e
        AV1_OBUGetMetadataType(const uint8_t *p_buf, size_t i_buf)
{
    if(!AV1_OBUSkipHeader(&p_buf, &i_buf) || i_buf < 1)
        return AV1_METADATA_TYPE_RESERVED;

    uint8_t i_len;
    uint64_t i_type = leb128(p_buf, i_buf, &i_len);
    if(i_len == 0 || i_type > ((INT64_C(1) << 32) - 1))
        return AV1_METADATA_TYPE_RESERVED;
    return (enum av1_obu_metadata_type_e) i_type;
}



/* SEQUENCE_HEADER properties */
typedef struct av1_OBU_sequence_header_t av1_OBU_sequence_header_t;
av1_OBU_sequence_header_t * AV1_OBU_parse_sequence_header(const uint8_t *, size_t);
void AV1_release_sequence_header(av1_OBU_sequence_header_t *);
void AV1_get_frame_max_dimensions(const av1_OBU_sequence_header_t *, unsigned *, unsigned *);
void AV1_get_profile_level(const av1_OBU_sequence_header_t *, int *, int *, int *);
bool AV1_get_colorimetry( const av1_OBU_sequence_header_t *,
                          video_color_primaries_t *, video_transfer_func_t *,
                          video_color_space_t *, video_color_range_t *);
bool AV1_get_frame_rate(const av1_OBU_sequence_header_t *, unsigned *, unsigned *);



/* FRAME_HEADER properties */
typedef struct av1_OBU_frame_header_t av1_OBU_frame_header_t;
enum av1_frame_type_e
{
    AV1_FRAME_TYPE_KEY        = 0,
    AV1_FRAME_TYPE_INTER      = 1,
    AV1_FRAME_TYPE_INTRA_ONLY = 2,
    AV1_FRAME_TYPE_SWITCH     = 3,
};

av1_OBU_frame_header_t * AV1_OBU_parse_frame_header(const uint8_t *p_data, size_t i_data,
                                                    const av1_OBU_sequence_header_t *);
void AV1_release_frame_header(av1_OBU_frame_header_t *);
enum av1_frame_type_e AV1_get_frame_type(const av1_OBU_frame_header_t *);
bool AV1_get_frame_visibility(const av1_OBU_frame_header_t *);



/* ISOBMFF Mapping */

size_t AV1_create_DecoderConfigurationRecord(uint8_t **,
                                             const av1_OBU_sequence_header_t *,
                                             size_t, const uint8_t *[], const size_t []);

/* OBU Iterator */

typedef struct
{
    const uint8_t *p_head;
    const uint8_t *p_tail;
} AV1_OBU_iterator_ctx_t;

static inline void AV1_OBU_iterator_init(AV1_OBU_iterator_ctx_t *p_ctx,
                                         const uint8_t *p_data, size_t i_data)
{
    p_ctx->p_head = p_data;
    p_ctx->p_tail = p_data + i_data;
}

static inline bool AV1_OBU_iterate_next(AV1_OBU_iterator_ctx_t *p_ctx,
                                        const uint8_t **pp_start, size_t *pi_size)
{
    const size_t i_remain = p_ctx->p_tail - p_ctx->p_head;
    if(!AV1_OBUIsValid(p_ctx->p_head, i_remain))
        return false;
    if(!AV1_OBUHasSizeField(p_ctx->p_head))
    {
        *pp_start = p_ctx->p_head;
        *pi_size = i_remain;
        p_ctx->p_head = p_ctx->p_tail;
        return true;
    }

    uint8_t i_obu_size_len;
    const uint32_t i_obu_size = AV1_OBUSize(p_ctx->p_head, i_remain, &i_obu_size_len);
    const size_t i_obu = i_obu_size + i_obu_size_len + !!AV1_OBUHasExtensionField(p_ctx->p_head) + 1;
    if(i_obu_size_len == 0 || i_obu > i_remain)
        return false;
    *pi_size = i_obu;
    *pp_start = p_ctx->p_head;
    p_ctx->p_head += i_obu;
    return true;
}

#endif
