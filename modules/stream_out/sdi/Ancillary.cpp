/*****************************************************************************
 * Ancillary.cpp: SDI Ancillary
 *****************************************************************************
 * Copyright © 2014-2016 VideoLAN and VideoLAN Authors
 *                  2018 VideoLabs
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Ancillary.hpp"

using namespace sdi;

AFD::AFD(uint8_t afdcode, uint8_t ar)
{
    this->afdcode = afdcode;
    this->ar = ar;
}

AFD::~AFD()
{

}

static inline void put_le32(uint8_t **p, uint32_t d)
{
    SetDWLE(*p, d);
    (*p) += 4;
}

void AFD::FillBuffer(uint8_t *p_buf, size_t i_buf)
{
    const size_t len = 6 /* vanc header */ + 8 /* AFD data */ + 1 /* csum */;
    const size_t s = ((len + 5) / 6) * 6; // align for v210

    if(s * 6 >= i_buf / 16)
        return;

    uint16_t afd[s];

    afd[0] = 0x000;
    afd[1] = 0x3ff;
    afd[2] = 0x3ff;
    afd[3] = 0x41; // DID
    afd[4] = 0x05; // SDID
    afd[5] = 8; // Data Count

    int bar_data_flags = 0;
    int bar_data_val1 = 0;
    int bar_data_val2 = 0;

    afd[ 6] = ((afdcode & 0x0F) << 3) | ((ar & 0x01) << 2); /* SMPTE 2016-1 */
    afd[ 7] = 0; // reserved
    afd[ 8] = 0; // reserved
    afd[ 9] = bar_data_flags << 4;
    afd[10] = bar_data_val1 << 8;
    afd[11] = bar_data_val1 & 0xff;
    afd[12] = bar_data_val2 << 8;
    afd[13] = bar_data_val2 & 0xff;

    /* parity bit */
    for (size_t i = 3; i < len - 1; i++)
        afd[i] |= vlc_parity((unsigned)afd[i]) ? 0x100 : 0x200;

    /* vanc checksum */
    uint16_t vanc_sum = 0;
    for (size_t i = 3; i < len - 1; i++) {
        vanc_sum += afd[i];
        vanc_sum &= 0x1ff;
    }

    afd[len - 1] = vanc_sum | ((~vanc_sum & 0x100) << 1);

    /* pad */
    for (size_t i = len; i < s; i++)
        afd[i] = 0x040;

    /* convert to v210 and write into VANC */
    for (size_t w = 0; w < s / 6 ; w++) {
        put_le32(&p_buf, afd[w*6+0] << 10);
        put_le32(&p_buf, afd[w*6+1] | (afd[w*6+2] << 20));
        put_le32(&p_buf, afd[w*6+3] << 10);
        put_le32(&p_buf, afd[w*6+4] | (afd[w*6+5] << 20));
    }
}
