/*****************************************************************************
 * cc.h
 *****************************************************************************
 * Copyright (C) 2007 Laurent Aimar
 * $Id$
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

#ifndef _CC_H
#define _CC_H 1

#include <vlc_bits.h>

/* CC have a maximum rate of 9600 bit/s (per field?) */
#define CC_MAX_DATA_SIZE (2 * 3*600)
enum
{
    CC_PAYLOAD_NONE,
    CC_PAYLOAD_GA94,
    CC_PAYLOAD_DVD,
    CC_PAYLOAD_REPLAYTV,
    CC_PAYLOAD_SCTE20,
};
typedef struct
{
    /* Which channel are present */
    bool pb_present[4];

    /* */
    bool b_reorder;

    /* */
    int i_payload_type;
    int i_payload_other_count;

    /* CC data per field
     *  byte[x+0]: field (0/1)
     *  byte[x+1]: cc data 1
     *  byte[x+2]: cc data 2
     */
    int     i_data;
    uint8_t p_data[CC_MAX_DATA_SIZE];
} cc_data_t;

static inline void cc_Init( cc_data_t *c )
{
    int i;

    for( i = 0; i < 4; i++ )
        c-> pb_present[i] = false; 
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

static inline void cc_AppendData( cc_data_t *c, int i_field, const uint8_t cc[2] )
{
    if( i_field == 0 || i_field == 1 )
    {
        c->pb_present[2*i_field+0] =
        c->pb_present[2*i_field+1] = true;
    }

    c->p_data[c->i_data++] = i_field;
    c->p_data[c->i_data++] = cc[0];
    c->p_data[c->i_data++] = cc[1];
}

static inline void cc_Extract( cc_data_t *c, bool b_top_field_first, const uint8_t *p_src, int i_src )
{
    static const uint8_t p_cc_ga94[4] = { 0x47, 0x41, 0x39, 0x34 };
    static const uint8_t p_cc_dvd[4] = { 0x43, 0x43, 0x01, 0xf8 };
    static const uint8_t p_cc_replaytv4a[2] = { 0xbb, 0x02 };
    static const uint8_t p_cc_replaytv4b[2] = { 0xcc, 0x02 };
    static const uint8_t p_cc_replaytv5a[2] = { 0x99, 0x02 };
    static const uint8_t p_cc_replaytv5b[2] = { 0xaa, 0x02 };
    static const uint8_t p_cc_scte20[2] = { 0x03, 0x81 };
    static const uint8_t p_cc_scte20_old[2] = { 0x03, 0x01 };

    if( i_src < 4 )
        return;

    int i_payload_type;
    if( !memcmp( p_cc_ga94, p_src, 4 ) && i_src >= 5+1+1+1 && p_src[4] == 0x03 )
    {
        /* CC from DVB/ATSC TS */
        i_payload_type = CC_PAYLOAD_GA94;
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

    if( c->i_payload_type != CC_PAYLOAD_NONE && c->i_payload_type != i_payload_type )
    {
        c->i_payload_other_count++;
        if( c->i_payload_other_count < 50 )
            return;
    }
    c->i_payload_type        = i_payload_type;
    c->i_payload_other_count = 0;

    if( i_payload_type == CC_PAYLOAD_GA94 )
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
        const uint8_t *cc = &p_src[5];
        const int i_count_cc = cc[0]&0x1f;
        int i;

        if( !(cc[0]&0x40) ) // process flag
            return;
        if( i_src < 5 + 1+1 + i_count_cc*3 + 1)  // broken packet
            return;
        if( i_count_cc <= 0 )   // no cc present
            return;
        if( cc[2+i_count_cc * 3] != 0xff )  // marker absent
            return;
        cc += 2;

        for( i = 0; i < i_count_cc; i++, cc += 3 )
        {
            int i_field = cc[0] & 0x03;
            if( ( cc[0] & 0xfc ) != 0xfc )
                continue;
            if( i_field != 0 && i_field != 1 )
                continue;
            if( c->i_data + 3 > CC_MAX_DATA_SIZE )
                continue;

            cc_AppendData( c, i_field, &cc[1] );
        }
        c->b_reorder = true;
    }
    else if( i_payload_type == CC_PAYLOAD_DVD )
    {
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
                if( cc[0] != 0xff && cc[0] != 0xfe )
                    continue;
                if( c->i_data + 3 > CC_MAX_DATA_SIZE )
                    continue;

                cc_AppendData( c, i_field, &cc[1] );
            }
        }
        c->b_reorder = false;
    }
    else if( i_payload_type == CC_PAYLOAD_REPLAYTV )
    {
        const uint8_t *cc = &p_src[0];
        int i;
        if( c->i_data + 2*3 > CC_MAX_DATA_SIZE )
            return;

        for( i = 0; i < 2; i++, cc += 4 )
        {
            const int i_field = i == 0 ? 1 : 0;

            cc_AppendData( c, i_field, &cc[2] );
        }
        c->b_reorder = false;
    }
    else
    {
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

            cc_AppendData( c, i_field, &cc[0] );
        }
        c->b_reorder = true;
    }
}

#endif /* _CC_H */

