/*****************************************************************************
 * cc.h
 *****************************************************************************
 * Copyright (C) 2007 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_CC_H_
#define VLC_CC_H_

#include <vlc_bits.h>

#define CC_PKT_BYTE0(field) (0xFC | (0x03 & field))

/* CC have a maximum rate of 9600 bit/s (per field?) */
#define CC_MAX_DATA_SIZE (2 * 3*600)
enum cc_payload_type_e
{
    CC_PAYLOAD_NONE,
    CC_PAYLOAD_RAW,
    CC_PAYLOAD_GA94,
    CC_PAYLOAD_DVD,
    CC_PAYLOAD_REPLAYTV,
    CC_PAYLOAD_SCTE20,
};
typedef struct
{
    /* Which channel are present */
    uint64_t i_708channels;
    uint8_t  i_608channels;

    /* */
    bool b_reorder;

    /* */
    enum cc_payload_type_e i_payload_type;
    int i_payload_other_count;

    /* CC data per field
     *  byte[x+0]: field (0/1)
     *  byte[x+1]: cc data 1
     *  byte[x+2]: cc data 2
     */
    size_t  i_data;
    uint8_t p_data[CC_MAX_DATA_SIZE];
} cc_data_t;

static inline void cc_Init( cc_data_t *c )
{
    c->i_608channels = 0;
    c->i_708channels = 0;
    c->i_data = 0;
    c->b_reorder = false;
    c->i_payload_type = CC_PAYLOAD_NONE;
    c->i_payload_other_count = 0;
}
static inline void cc_Exit( cc_data_t *c )
{
    VLC_UNUSED(c);
    return;
}
static inline void cc_Flush( cc_data_t *c )
{
    c->i_data = 0;
}

static inline void cc_AppendData( cc_data_t *c, uint8_t cc_preamble, const uint8_t cc[2] )
{
    uint8_t i_field = cc_preamble & 0x03;
    if( i_field == 0 || i_field == 1 )
        c->i_608channels |= (3 << (2 * i_field));
    else
        c->i_708channels |= 1;

    c->p_data[c->i_data++] = cc_preamble;
    c->p_data[c->i_data++] = cc[0];
    c->p_data[c->i_data++] = cc[1];
}

static inline void cc_Extract( cc_data_t *c, enum cc_payload_type_e i_payload_type,
                               bool b_top_field_first, const uint8_t *p_src, int i_src )
{
    if( c->i_payload_type != CC_PAYLOAD_NONE && c->i_payload_type != i_payload_type )
    {
        c->i_payload_other_count++;
        if( c->i_payload_other_count < 50 )
            return;
    }
    c->i_payload_type        = i_payload_type;
    c->i_payload_other_count = 0;

    if( i_payload_type == CC_PAYLOAD_RAW )
    {
        for( int i = 0; i + 2 < i_src; i += 3 )
        {
            if( c->i_data + 3 > CC_MAX_DATA_SIZE )
                break;

            const uint8_t *cc = &p_src[i];
            cc_AppendData( c, cc[0], &cc[1] );
        }
        c->b_reorder = true;
    }
    else if( i_payload_type == CC_PAYLOAD_GA94 )
    {
        /* cc_data()
         *          u1 reserved(1)
         *          u1 process_cc_data_flag
         *          u1 additional_data_flag
         *          u5 cc_count
         *          u8 reserved(1111 1111)
         *          for cc_count
         *              u5 marker bit(1111 1)
         *              u1 cc_valid
         *              u2 cc_type
         *              u8 cc_data_1
         *              u8 cc_data_2
         *          u8 marker bit(1111 1111)
         *          if additional_data_flag
         *              unknown
         */
        /* cc_type:
         *  0x00: field 1
         *  0x01: field 2
         */
        const uint8_t *cc = &p_src[0];
        const int i_count_cc = cc[0]&0x1f;
        int i;

        if( !(cc[0]&0x40) ) // process flag
            return;
        if( i_src < 1+1 + i_count_cc*3 + 1)  // broken packet
            return;
        if( i_count_cc <= 0 )   // no cc present
            return;
        if( cc[2+i_count_cc * 3] != 0xff )  // marker absent
            return;
        cc += 2;

        for( i = 0; i < i_count_cc; i++, cc += 3 )
        {
            if( c->i_data + 3 > CC_MAX_DATA_SIZE )
                break;

            cc_AppendData( c, cc[0], &cc[1] );
        }
        c->b_reorder = true;
    }
    else if( i_payload_type == CC_PAYLOAD_DVD )
    {
        /* user_data
         *          (u32 stripped earlier)
         *          u32 (0x43 0x43 0x01 0xf8)
         *          u1 caption_odd_field_first (CC1/CC2)
         *          u1 caption_filler
         *          u5 cc_block_count  (== cc_count / 2)
         *          u1 caption_extra_field_added (because odd cc_count)
         *          for cc_block_count * 2 + caption_extra_field_added
         *              u7 cc_filler_1
         *              u1 cc_field_is_odd
         *              u8 cc_data_1
         *              u8 cc_data_2
         */
        const int b_truncate = p_src[4] & 0x01;
        const int i_field_first = (p_src[4] & 0x80) ? 0 : 1;
        const int i_count_cc2 = (p_src[4] >> 1) & 0xf;
        const uint8_t *cc = &p_src[5];
        int i;

        if( i_src < 4+1+6*i_count_cc2 - ( b_truncate ? 3 : 0) )
            return;
        for( i = 0; i < i_count_cc2; i++ )
        {
            int j;
            for( j = 0; j < 2; j++, cc += 3 )
            {
                const int i_field = j == i_field_first ? 0 : 1;

                if( b_truncate && i == i_count_cc2 - 1 && j == 1 )
                    break;
                if( (cc[0] & 0xfe) != 0xfe )
                    continue;
                if( c->i_data + 3 > CC_MAX_DATA_SIZE )
                    continue;

                cc_AppendData( c, CC_PKT_BYTE0(i_field), &cc[1] );
            }
        }
        c->b_reorder = false;
    }
    else if( i_payload_type == CC_PAYLOAD_REPLAYTV )
    {
        const uint8_t *cc = &p_src[0];
        for( int i_cc_count = i_src >> 2; i_cc_count > 0;
             i_cc_count--, cc += 4 )
        {
            if( c->i_data + 3 > CC_MAX_DATA_SIZE )
                return;
            uint8_t i_field = (cc[0] & 0x02) >> 1;
            cc_AppendData( c, CC_PKT_BYTE0(i_field), &cc[2] );
        }
        c->b_reorder = false;
    }
    else /* CC_PAYLOAD_SCTE20 */
    {
        /* user_data(2)
         *          (u32 stripped earlier)
         *          u16 p_cc_scte20
         *          u5 cc_count
         *          for cc_count
         *              u2 cc_priority
         *              u2 cc_field_num
         *              u5 cc_line_offset
         *              u8 cc_data_1[1:8]
         *              u8 cc_data_2[1:8]
         *              u1 marker bit
         *          un additional_realtimevideodata
         *          un reserved
         */
        bs_t s;
        bs_init( &s, &p_src[2], i_src - 2 );
        const int i_cc_count = bs_read( &s, 5 );
        for( int i = 0; i < i_cc_count; i++ )
        {
            bs_skip( &s, 2 );
            const int i_field_idx = bs_read( &s, 2 );
            bs_skip( &s, 5 );
            uint8_t cc[2];
            for( int j = 0; j < 2; j++ )
            {
                cc[j] = 0;
                for( int k = 0; k < 8; k++ )
                    cc[j] |= bs_read( &s, 1 ) << k;
            }
            bs_skip( &s, 1 );

            if( i_field_idx == 0 )
                continue;
            if( c->i_data + 2*3 > CC_MAX_DATA_SIZE )
                continue;

            /* 1,2,3 -> 0,1,0. I.E. repeated field 3 is merged with field 1 */
            int i_field = ((i_field_idx - 1) & 1);
            if (!b_top_field_first)
                i_field ^= 1;

            cc_AppendData( c, CC_PKT_BYTE0(i_field), &cc[0] );
        }
        c->b_reorder = true;
    }
}


static inline void cc_ProbeAndExtract( cc_data_t *c, bool b_top_field_first, const uint8_t *p_src, int i_src )
{
    static const uint8_t p_cc_ga94[4] = { 0x47, 0x41, 0x39, 0x34 };
    static const uint8_t p_cc_dvd[4] = { 0x43, 0x43, 0x01, 0xf8 }; /* ascii 'CC', type_code, cc_block_size */
    static const uint8_t p_cc_replaytv4a[2] = { 0xbb, 0x02 };/* RTV4K, BB02xxxxCC02 */
    static const uint8_t p_cc_replaytv4b[2] = { 0xcc, 0x02 };/* see DVR-ClosedCaption in samples */
    static const uint8_t p_cc_replaytv5a[2] = { 0x99, 0x02 };/* RTV5K, 9902xxxxAA02 */
    static const uint8_t p_cc_replaytv5b[2] = { 0xaa, 0x02 };/* see DVR-ClosedCaption in samples */
    static const uint8_t p_cc_scte20[2] = { 0x03, 0x81 };    /* user_data_type_code, SCTE 20 */
    static const uint8_t p_cc_scte20_old[2] = { 0x03, 0x01 };/* user_data_type_code, old, Note 1 */

    if( i_src < 4 )
        return;

    enum cc_payload_type_e i_payload_type;
    if( !memcmp( p_cc_ga94, p_src, 4 ) && i_src >= 5+1+1+1 && p_src[4] == 0x03 )
    {
        /* CC from DVB/ATSC TS */
        i_payload_type = CC_PAYLOAD_GA94;
        i_src -= 5;
        p_src += 5;
    }
    else if( !memcmp( p_cc_dvd, p_src, 4 ) && i_src > 4+1 )
    {
        i_payload_type = CC_PAYLOAD_DVD;
    }
    else if( i_src >= 2+2 + 2+2 &&
             ( ( !memcmp( p_cc_replaytv4a, &p_src[0], 2 ) && !memcmp( p_cc_replaytv4b, &p_src[4], 2 ) ) ||
               ( !memcmp( p_cc_replaytv5a, &p_src[0], 2 ) && !memcmp( p_cc_replaytv5b, &p_src[4], 2 ) ) ) )
    {
        i_payload_type = CC_PAYLOAD_REPLAYTV;
    }
    else if( ( !memcmp( p_cc_scte20, p_src, 2 ) ||
               !memcmp( p_cc_scte20_old, p_src, 2 ) ) && i_src > 2 )
    {
        i_payload_type = CC_PAYLOAD_SCTE20;
    }
    else if (p_src[0] == 0x03 && p_src[1] == i_src - 2) /* DIRECTV */
    {
        i_payload_type = CC_PAYLOAD_GA94;
        i_src -= 2;
        p_src += 2;
    }
    else
    {
#if 0
#define V(x) ( ( x < 0x20 || x >= 0x7f ) ? '?' : x )
        fprintf( stderr, "-------------- unknown user data " );
        for( int i = 0; i < i_src; i++ )
            fprintf( stderr, "%2.2x ", p_src[i] );
        for( int i = 0; i < i_src; i++ )
            fprintf( stderr, "%c ", V(p_src[i]) );
        fprintf( stderr, "\n" );
#undef V
#endif
        return;
    }

    cc_Extract( c, i_payload_type, b_top_field_first, p_src, i_src );
}

#endif /* _CC_H */

