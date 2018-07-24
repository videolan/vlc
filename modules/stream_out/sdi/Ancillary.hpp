/*****************************************************************************
 * Ancillary.hpp: SDI Ancillary
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
#ifndef ANCILLARY_HPP
#define ANCILLARY_HPP

#include <vlc_common.h>

namespace sdi
{

    class Ancillary
    {
        public:
            virtual void FillBuffer(uint8_t *, size_t) = 0;
    };

    class AFD : public Ancillary
    {
        public:
            AFD(uint8_t afdcode, uint8_t ar);
            virtual ~AFD();
            virtual void FillBuffer(uint8_t *, size_t);

        private:
            uint8_t afdcode;
            uint8_t ar;
    };

    class Captions : public Ancillary
    {
        public:
            Captions(const uint8_t *, size_t, unsigned, unsigned);
            virtual ~Captions();
            virtual void FillBuffer(uint8_t *, size_t);

        private:
            const uint8_t *p_data;
            size_t i_data;
            unsigned rate;
    };
}

#endif // ANCILLARY_HPP
