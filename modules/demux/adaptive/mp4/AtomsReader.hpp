/*
 * AtomsReader.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
#ifndef ATOMSREADER_HPP
#define ATOMSREADER_HPP

#include <vlc_common.h>
#include <vlc_stream.h>
extern "C" {
#include "../../demux/mp4/libmp4.h"
}

namespace adaptive
{
    namespace mp4
    {
        class AtomsReader
        {
            public:
                AtomsReader(vlc_object_t *);
                ~AtomsReader();
                void clean();
                bool parseBlock(block_t *);

            protected:
                vlc_object_t *object;
                MP4_Box_t *rootbox;
        };
    }
}

#endif // ATOMSREADER_HPP
