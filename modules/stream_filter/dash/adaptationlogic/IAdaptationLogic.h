/*
 * IAdaptationLogic.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
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

#ifndef IADAPTATIONLOGIC_H_
#define IADAPTATIONLOGIC_H_

#include <http/Chunk.h>
#include <adaptationlogic/IDownloadRateObserver.h>
#include "mpd/Representation.h"
#include "buffer/IBufferObserver.h"

namespace dash
{
    namespace logic
    {
        class IAdaptationLogic : public IDownloadRateObserver, public dash::buffer::IBufferObserver
        {
            public:

                enum LogicType
                {
                    Default,
                    AlwaysBest,
                    AlwaysLowest,
                    RateBased
                };

                virtual dash::http::Chunk*                  getNextChunk            ()          = 0;
                virtual const dash::mpd::Representation*    getCurrentRepresentation() const    = 0;
                /**
                 *  \return     The average bitrate in bits per second.
                 */
                virtual uint64_t                getBpsAvg               () const = 0;
                virtual uint64_t                getBpsLastChunk         () const = 0;
        };
    }
}

#endif /* IADAPTATIONLOGIC_H_ */
