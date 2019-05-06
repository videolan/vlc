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

static inline uint16_t withparitybit(uint16_t v)
{
    return v | (vlc_parity(v) ? 0x100 : 0x200);
}

Ancillary::Data10bitPacket::Data10bitPacket(uint8_t did, uint8_t sdid,
                                            const AbstractPacket<uint8_t> &other)
{
    payload.reserve(6 + 1 + other.size());
    payload.resize(6);
    payload[0] = 0x000;
    payload[1] = 0x3ff;
    payload[2] = 0x3ff;
    payload[3] = withparitybit(did);
    payload[4] = withparitybit(sdid);
    payload[5] = withparitybit(other.size());
    for(std::size_t i = 0; i<other.size(); i++)
        payload.push_back(withparitybit(other.data()[i]));
    uint16_t sum = 0;
    for(std::size_t i = 3; i<payload.size(); i++)
        sum = (sum + payload[i]) & 0x1ff;
    payload.push_back(sum | ((~sum & 0x100) << 1));
}

void Ancillary::Data10bitPacket::pad()
{
    while(payload.size() % V210::ALIGNMENT_U16)
        payload.push_back(0x40); /* padding */
}

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

AFD::AFDData::AFDData(uint8_t code, uint8_t ar)
{
    payload.resize(8);

    int bar_data_flags = 0;
    int bar_data_val1 = 0;
    int bar_data_val2 = 0;

    payload[0] = ((code & 0x0F) << 3) | ((ar & 0x01) << 2); /* SMPTE 2016-1 */
    payload[1] = 0; // reserved
    payload[2] = 0; // reserved
    payload[3] = bar_data_flags << 4;
    payload[4] = bar_data_val1 << 8;
    payload[5] = bar_data_val1 & 0xff;
    payload[6] = bar_data_val2 << 8;
    payload[7] = bar_data_val2 & 0xff;
}

void AFD::FillBuffer(uint8_t *p_buf, size_t i_buf)
{
    AFDData afd(afdcode, ar);

    Data10bitPacket packet(0x41, 0x05, afd);
    packet.pad();

    if(packet.size() > i_buf)
        return;

    /* convert to v210 and write into VANC */
    V210::Convert((uint16_t *)packet.data(), packet.size() / 2, p_buf);
}

Captions::CDP::CDP(const uint8_t * buf, std::size_t bufsize,
                   uint8_t rate, uint16_t cdp_counter)
{
    std::size_t cc_count = bufsize / 3;
    payload.reserve(9 + cc_count * 3 + 3 + 1);
    payload.resize(9);
    payload[0] = 0x96; // header id
    payload[1] = 0x69;
    payload[2] = 9 + 3 + 1 + cc_count * 3; // len
    payload[3] = ((rate << 4) | 0x0f);
    payload[4] = 0x43; // cc_data_present | caption_service_active | reserved
    payload[5] = cdp_counter >> 8;
    payload[6] = cdp_counter & 0xff;
    payload[7] = 0x72; // ccdata_id
    payload[8] = 0xe0 | cc_count; // cc_count
    // cc data
    for(std::size_t i=0; i<cc_count * 3; i++)
        payload.push_back(buf[i]);
    // cdp footer
    payload.push_back(0x74); // footer id
    payload.push_back(cdp_counter >> 8);
    payload.push_back(cdp_counter & 0xff);
    uint16_t sum = 0;
    for(std::size_t i=0; i<payload.size(); i++)
        sum = (sum + payload[i]) & 0xff;
    payload.push_back(sum ? 256 - sum : sum); /* cdpsum */
}

Captions::Captions(const uint8_t *p, size_t s,
                   unsigned num, unsigned den)
{
    cdp_counter = 0;
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

    CDP cdp(p_data, i_data, rate, cdp_counter++);

    Data10bitPacket packet(0x61, 0x01, cdp);
    packet.pad();

    if(packet.size() > i_buf)
        return;

    /* convert to v210 and write into VBI line 15 of VANC */
    V210::Convert((uint16_t *)packet.data(), packet.size() / 2, p_buf);
}
