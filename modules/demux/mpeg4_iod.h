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

#define ES_DESCRIPTOR_COUNT 255

typedef struct
{
    uint8_t                 i_objectTypeIndication;
    uint8_t                 i_streamType;

    unsigned                i_extra;
    uint8_t                 *p_extra;

} decoder_config_descriptor_t;

typedef struct
{
    bool                    b_ok;
    uint16_t                i_es_id;

    char                    *psz_url;

    decoder_config_descriptor_t    dec_descr;

} es_mpeg4_descriptor_t;

typedef struct
{
    /* IOD */
    char                    *psz_url;

    es_mpeg4_descriptor_t   es_descr[ES_DESCRIPTOR_COUNT];

} iod_descriptor_t;

iod_descriptor_t *IODNew( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data );
void IODFree( iod_descriptor_t *p_iod );
