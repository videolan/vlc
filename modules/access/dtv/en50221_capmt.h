/*****************************************************************************
 * en50221_capmt.h:
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/
#ifndef EN50221_CAPMT_H
#define EN50221_CAPMT_H

typedef struct
{
    uint8_t  i_stream_type;
    uint16_t i_es_pid;
    size_t   i_descriptors;
    uint8_t *p_descriptors;
} en50221_capmt_es_info_t;

typedef struct en50221_capmt_info_s
{
    uint8_t  i_version;
    uint16_t i_program_number;
    size_t   i_program_descriptors;
    uint8_t *p_program_descriptors;
    size_t   i_es_count;
    en50221_capmt_es_info_t *p_es;
} en50221_capmt_info_t;

static inline void en50221_capmt_CADescriptorAppend( uint8_t **p_desc, size_t *pi_desc,
                                                     const uint8_t *p_data, uint8_t i_data )
{
    uint8_t *p_realloc = realloc( *p_desc, *pi_desc + i_data + 2 );
    if( likely(p_realloc) )
    {
        *p_desc = p_realloc;
        p_realloc[*pi_desc] = 0x09;
        p_realloc[*pi_desc + 1] = i_data;
        memcpy( &p_realloc[*pi_desc + 2], p_data, i_data );
        *pi_desc = *pi_desc + i_data + 2;
    }
}

static inline en50221_capmt_es_info_t *en50221_capmt_EsAdd( en50221_capmt_info_t *p_en,
                                                            uint8_t i_stream_type,
                                                            uint16_t i_es_pid )
{
    en50221_capmt_es_info_t *p_realloc = realloc( p_en->p_es, sizeof(en50221_capmt_es_info_t) *
                                                              (p_en->i_es_count + 1) );
    if( likely(p_realloc) )
    {
        p_en->p_es = p_realloc;
        en50221_capmt_es_info_t *p_es = &p_en->p_es[ p_en->i_es_count++ ];
        p_es->i_es_pid = i_es_pid;
        p_es->i_stream_type = i_stream_type;
        p_es->i_descriptors = 0;
        p_es->p_descriptors = NULL;
        return p_es;
    }
    return NULL;
}

static inline void en50221_capmt_AddESCADescriptor( en50221_capmt_es_info_t *p_es,
                                                    const uint8_t *p_data, uint8_t i_data )
{
    en50221_capmt_CADescriptorAppend( &p_es->p_descriptors, &p_es->i_descriptors,
                                       p_data, i_data );
}

static inline void en50221_capmt_AddCADescriptor( en50221_capmt_info_t *p_en,
                                                  const uint8_t *p_data, uint8_t i_data )
{
    en50221_capmt_CADescriptorAppend( &p_en->p_program_descriptors,
                                      &p_en->i_program_descriptors,
                                       p_data, i_data );
}

static inline void en50221_capmt_Delete( en50221_capmt_info_t *p_en )
{
    free( p_en->p_program_descriptors );
    for( size_t i=0; i<p_en->i_es_count; i++ )
        free( p_en->p_es[i].p_descriptors );
    free( p_en->p_es );
    free( p_en );
}

static inline en50221_capmt_info_t * en50221_capmt_New( uint8_t i_version, uint16_t i_program )
{
    en50221_capmt_info_t *p_en = malloc( sizeof(*p_en) );
    if( likely(p_en) )
    {
        p_en->i_version = i_version;
        p_en->i_program_number = i_program;
        p_en->i_program_descriptors = 0;
        p_en->p_program_descriptors = NULL;
        p_en->i_es_count = 0;
        p_en->p_es = NULL;
    }
    return p_en;
}

#endif
