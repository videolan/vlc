/*****************************************************************************
 * opus.h : Opus demux helpers
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 *
 * Authors: Timothy B. Terriberry <tterribe@xiph.org>
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

/* Returns Opus frame duration in samples */

static inline unsigned opus_frame_duration(unsigned char *data, long len)
{
    static const int silk_fs_div[4] = { 6000, 3000, 1500, 1000 };
    unsigned toc;
    unsigned nframes;
    unsigned frame_size;
    unsigned nsamples;
    unsigned i_rate;
    if( len < 1 )
        return 0;
    toc = data[0];
    switch( toc&3 )
    {
        case 0:
            nframes = 1;
            break;
        case 1:
        case 2:
            nframes = 2;
            break;
        default:
            if( len < 2 )
                return 0;
            nframes = data[1]&0x3F;
            break;
    }
    i_rate = 48000;
    if( toc&0x80 )
        frame_size = (i_rate << (toc >> 3 & 3)) / 400;
    else if( ( toc&0x60 ) == 0x60 )
        frame_size = i_rate/(100 >> (toc >> 3 & 1));
    else
        frame_size = i_rate*60 / silk_fs_div[toc >> 3 & 3];
    nsamples = nframes*frame_size;
    if( nsamples*25 > i_rate*3 )
        return 0;
    return nsamples;
}
