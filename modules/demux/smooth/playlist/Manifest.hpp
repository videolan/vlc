/*
 * Manifest.hpp
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
#ifndef MANIFEST_HPP
#define MANIFEST_HPP

#include "../../adaptive/playlist/AbstractPlaylist.hpp"
#include "../../adaptive/playlist/Inheritables.hpp"
#include "../../adaptive/Time.hpp"

namespace smooth
{
    namespace playlist
    {
        using namespace adaptive::playlist;

        class Manifest : public AbstractPlaylist,
                         public TimescaleAble
        {
            friend class ManifestParser;

            public:
                Manifest(vlc_object_t *);
                virtual ~Manifest();

                virtual bool                    isLive() const;
                virtual void                    debug();

            private:
                bool b_live;
        };
    }
}

#endif // MANIFEST_HPP
