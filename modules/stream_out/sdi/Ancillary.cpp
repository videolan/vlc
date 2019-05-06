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
#include "V210.hpp"
#include <cassert>

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
    V210::Convert(afd, s, p_buf);
}

Captions::Captions(const uint8_t *p, size_t s,
                   unsigned num, unsigned den)
{
    p_data = p;
    i_data = s;
    vlc_ureduce(&num, &den, num, den, 0);
    if (num == 24000 && den == 1001) {
        rate = 1;
    } else if (num == 24 && den == 1) {
        rate = 2;
    } else if (num == 25 && den == 1) {
        rate = 3;
    } else if (num == 30000 && den == 1001) {
        rate = 4;
    } else if (num == 30 && den == 1) {
        rate = 5;
    } else if (num == 50 && den == 1) {
        rate = 6;
    } else if (num == 60000 && den == 1001) {
        rate = 7;
    } else if (num == 60 && den == 1) {
        rate = 8;
    } else {
        rate = 1;
    }
}

Captions::~Captions()
{

}

void Captions::FillBuffer(uint8_t *p_buf, size_t i_buf)
{
    uint8_t cc_count = i_data / 3;
    if (cc_count == 0)
        return;

    uint16_t len = 6 /* vanc header */ + 9 /* cdp header */ + 3 * cc_count +/* cc_data */
        4 /* cdp footer */ + 1 /* vanc checksum */;

    static uint16_t hdr = 0; /* cdp counter */
    size_t s = ((len + 5) / 6) * 6; /* align to 6 for v210 conversion */

    if(i_buf < s / 6 * 16)
        return;

    uint16_t *cdp = new uint16_t[s];

    uint16_t cdp_header[6+9] = {
        /* VANC header = 6 words */
        0x000, 0x3ff, 0x3ff, /* Ancillary Data Flag */

        /* following words need parity bits */

        0x61, /* Data ID */
        0x01, /* Secondary Data I D= CEA-708 */
        (uint16_t)(len - 6 - 1), /* Data Count (not including VANC header) */

        /* cdp header */

        0x96, // header id
        0x69,
        (uint16_t)(len - 6 - 1),
        (uint16_t)((rate << 4) | 0x0f),
        0x43, // cc_data_present | caption_service_active | reserved
        (uint16_t)(hdr >> 8),
        (uint16_t)(hdr & 0xff),
        0x72, // ccdata_id
        (uint16_t)(0xe0 | cc_count), // cc_count
    };

    /* cdp header */
    memcpy(cdp, cdp_header, sizeof(cdp_header));

    /* cdp data */
    for (size_t i = 0; i < cc_count; i++) { // copy cc_data
        cdp[6+9+3*i+0] = p_data[3*i+0] /*| 0xfc*/; // marker bits + cc_valid
        cdp[6+9+3*i+1] = p_data[3*i+1];
        cdp[6+9+3*i+2] = p_data[3*i+2];
    }

    /* cdp footer */
    cdp[len-5] = 0x74; // footer id
    cdp[len-4] = hdr >> 8;
    cdp[len-3] = hdr & 0xff;
    hdr++;

    /* cdp checksum */
    uint8_t sum = 0;
    for (uint16_t i = 6; i < len - 2; i++) {
        sum += cdp[i];
        sum &= 0xff;
    }
    cdp[len-2] = sum ? 256 - sum : 0;

    /* parity bit */
    for (uint16_t i = 3; i < len - 1; i++)
        cdp[i] |= vlc_parity(cdp[i]) ? 0x100 : 0x200;

    /* vanc checksum */
    uint16_t vanc_sum = 0;
    for (uint16_t i = 3; i < len - 1; i++) {
        vanc_sum += cdp[i];
        vanc_sum &= 0x1ff;
    }
    cdp[len - 1] = vanc_sum | ((~vanc_sum & 0x100) << 1);

    /* pad */
    for (size_t i = len; i < s; i++)
        cdp[i] = 0x040;

    /* convert to v210 and write into VBI line 15 of VANC */
    V210::Convert(cdp, s, p_buf);

    delete[] cdp;
}
