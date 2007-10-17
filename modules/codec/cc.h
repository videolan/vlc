/*****************************************************************************
 * cc.h
 *****************************************************************************
 * Copyright (C) 2007 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _CC_H
#define _C_H 1

/* CC have a maximum rate of 9600 bit/s (per field?) */
#define CC_MAX_DATA_SIZE (2 * 3*600)
typedef struct
{
    /* Which channel are present */
    vlc_bool_t pb_present[4];

    /* CC data per field
     *  byte[x+0]: field (0/1)
     *  byte[x+1]: cc data 1
     *  byte[x+2]: cc data 2
     */
    int     i_data;
    uint8_t p_data[CC_MAX_DATA_SIZE];
} cc_data_t;

static inline int cc_Channel( int i_field, const uint8_t p_data[2] )
{
    const uint8_t d = p_data[0] & 0x7f;
    if( i_field != 0 && i_field != 1 )
        return -1;
    if( d == 0x14 )
        return 2*i_field + 0;
    else if( d == 0x1c )
        return 2*i_field + 1;
    /* unknown(middle of a command)  or not cc channel */
    return -1;
}
static inline void cc_Init( cc_data_t *c )
{
    int i;

    for( i = 0; i < 4; i++ )
        c-> pb_present[i] = VLC_FALSE; 
    c->i_data = 0;
}
static inline void cc_Exit( cc_data_t *c )
{
    return;
}
static inline void cc_Flush( cc_data_t *c )
{
    c->i_data = 0;
}
static inline void cc_Extract( cc_data_t *c, const uint8_t *p_src, int i_src )
{
    static const uint8_t p_cc_ga94[4] = { 0x47, 0x41, 0x39, 0x34 };
    //static const uint8_t p_afd_start[4] = { 0x44, 0x54, 0x47, 0x31 };

    if( i_src < 4 )
        return;

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
    if( !memcmp( p_cc_ga94, p_src, 4 ) && i_src >= 5+1+1+1 && p_src[4] == 0x03 )
    {
        /* Extract CC from DVB/ATSC TS */
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
            int i_channel;
            if( ( cc[0] & 0xfc ) != 0xfc )
                continue;
            if( i_field != 0 && i_field != 1 )
                continue;
            if( c->i_data + 3 > CC_MAX_DATA_SIZE )
                continue;

            i_channel = cc_Channel( i_field, &cc[1] );
            if( i_channel >= 0 && i_channel < 4 )
                c->pb_present[i_channel] = VLC_TRUE;

            c->p_data[c->i_data++] = i_field;
            c->p_data[c->i_data++] = cc[1];
            c->p_data[c->i_data++] = cc[2];
        }
    }
    else
    {
        /* TODO DVD CC, ... */
    }
}

#endif /* _CC_H */

