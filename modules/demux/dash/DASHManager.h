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

#include "../adaptative/PlaylistManager.h"
#include "../adaptative/logic/AbstractAdaptationLogic.h"
#include "mpd/MPD.h"

namespace dash
{
    using namespace adaptative;

    class DASHStreamOutputFactory : public AbstractStreamOutputFactory
    {
        public:
            virtual AbstractStreamOutput *create(demux_t*, const StreamFormat &) const;
    };

    class DASHManager : public PlaylistManager
    {
        public:
            DASHManager( demux_t *, mpd::MPD *mpd,
                         AbstractStreamOutputFactory *,
                         logic::AbstractAdaptationLogic::LogicType type);
            virtual ~DASHManager    ();

            virtual bool updatePlaylist(); //reimpl
            virtual AbstractAdaptationLogic *createLogic(AbstractAdaptationLogic::LogicType); //reimpl

            static bool isDASH(stream_t *);

        protected:
            virtual int doControl(int, va_list); /* reimpl */
    };

}

#endif /* DASHMANAGER_H_ */
