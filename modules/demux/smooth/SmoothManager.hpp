/*
 * SmoothManager.hpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC authors
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
#ifndef SMOOTHMANAGER_HPP
#define SMOOTHMANAGER_HPP

#include "../adaptive/PlaylistManager.h"
#include "../adaptive/logic/AbstractAdaptationLogic.h"
#include "playlist/Manifest.hpp"

namespace adaptive
{
    namespace xml
    {
        class Node;
    }
}

namespace smooth
{
    using namespace adaptive;

    class SmoothManager : public PlaylistManager
    {
        public:
            SmoothManager( demux_t *, SharedResources *, playlist::Manifest *,
                        AbstractStreamFactory *,
                        logic::AbstractAdaptationLogic::LogicType type );
            virtual ~SmoothManager();

            virtual bool needsUpdate() const; /* reimpl */
            virtual void scheduleNextUpdate(); /* reimpl */
            virtual bool updatePlaylist(); /* reimpl */

            static bool isSmoothStreaming(xml::Node *);
            static bool mimeMatched(const std::string &);

        protected:
            virtual bool reactivateStream(AbstractStream *); /* reimpl */

        private:
            bool updatePlaylist(bool);
            playlist::Manifest * fetchManifest();
    };

}

#endif // SMOOTHMANAGER_HPP
