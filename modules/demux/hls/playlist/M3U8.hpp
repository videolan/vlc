/*
 * M3U8.hpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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

#ifndef M3U8_H_
#define M3U8_H_

#include "../../adaptive/playlist/AbstractPlaylist.hpp"

namespace hls
{
    namespace playlist
    {
        using namespace adaptive::playlist;

        class M3U8 : public AbstractPlaylist
        {
            public:
                M3U8(vlc_object_t *);
                virtual ~M3U8();

                virtual bool                    isLive() const;
                virtual void                    debug();

            private:
                std::string data;
        };
    }
}
#endif /* M3U8_H_ */
