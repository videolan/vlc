/*****************************************************************************
 * ID3Tag.h : ID3v2 Parsing Helper
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef ID3TAG_H
#define ID3TAG_H

static uint32_t ID3TAG_ReadSize( const uint8_t *p_buffer, bool b_syncsafe )
{
    if( !b_syncsafe )
        return GetDWBE( p_buffer );
    return ( (uint32_t)p_buffer[3] & 0x7F ) |
            (( (uint32_t)p_buffer[2] & 0x7F ) << 7) |
            (( (uint32_t)p_buffer[1] & 0x7F ) << 14) |
            (( (uint32_t)p_buffer[0] & 0x7F ) << 21);
}

static bool ID3TAG_IsTag( const uint8_t *p_buffer, bool b_footer )
{
    return( memcmp(p_buffer, (b_footer) ? "3DI" : "ID3", 3) == 0 &&
            p_buffer[3] < 0xFF &&
            p_buffer[4] < 0xFF &&
           ((GetDWBE(&p_buffer[6]) & 0x80808080) == 0) );
}

static size_t ID3TAG_Parse( const uint8_t *p_peek, size_t i_peek,
                            int (*pf_callback)(uint32_t, const uint8_t *, size_t, void *), void *p_priv )
{
    size_t i_total_size = 0;
    uint32_t i_ID3size = 0;
    if( i_peek > 10 && ID3TAG_IsTag( p_peek, false ) )
    {
        const bool b_syncsafe = p_peek[5] & 0x80;
        i_ID3size = ID3TAG_ReadSize( &p_peek[6], true );
        if( i_ID3size > i_peek - 10 )
            return 0;
        i_total_size = i_ID3size + 10;
        const uint8_t *p_frame = &p_peek[10];
        while( i_ID3size > 10 )
        {
            uint32_t i_tagname = VLC_FOURCC( p_frame[0], p_frame[1], p_frame[2], p_frame[3] );
            uint32_t i_framesize = ID3TAG_ReadSize( &p_frame[4], b_syncsafe ) + 10;
            if( i_framesize > i_ID3size )
                return 0;

            if( i_framesize > 10 &&
                pf_callback( i_tagname, &p_frame[10], i_framesize - 10, p_priv ) != VLC_SUCCESS )
                break;

            p_frame += i_framesize;
            i_ID3size -= i_framesize;
        }
    }

    /* Count footer if any */
    if( i_total_size && i_peek - i_total_size >= 10 &&
        ID3TAG_IsTag( &p_peek[i_total_size], true ) )
    {
        i_total_size += 10;
    }

    return i_total_size;
}

#endif // ID3TAG_H
