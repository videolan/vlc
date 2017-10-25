/*
 * DASHManager.h
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#ifndef DASHMANAGER_H_
#define DASHMANAGER_H_

#include "../adaptive/PlaylistManager.h"
#include "../adaptive/logic/AbstractAdaptationLogic.h"
#include "mpd/MPD.h"

namespace adaptive
{
    namespace xml
    {
        class Node;
    }
}

namespace dash
{
    using namespace adaptive;

    class DASHManager : public PlaylistManager
    {
        public:
            DASHManager( demux_t *,
                         AuthStorage *,
                         mpd::MPD *mpd,
                         AbstractStreamFactory *,
                         logic::AbstractAdaptationLogic::LogicType type);
            virtual ~DASHManager    ();

            virtual bool needsUpdate() const; /* reimpl */
            virtual bool updatePlaylist(); /* reimpl */
            virtual void scheduleNextUpdate();/* reimpl */
            static bool isDASH(xml::Node *);
            static bool mimeMatched(const std::string &);

        protected:
            virtual int doControl(int, va_list); /* reimpl */
    };

}

#endif /* DASHMANAGER_H_ */
