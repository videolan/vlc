/*
 * AbstractAdaptationLogic.h
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

#ifndef ABSTRACTADAPTATIONLOGIC_H_
#define ABSTRACTADAPTATIONLOGIC_H_

#include "adaptationlogic/IAdaptationLogic.h"
#include "xml/Node.h"
#include "http/Chunk.h"
#include "mpd/MPD.h"
#include "mpd/IMPDManager.h"
#include "mpd/Period.h"
#include "mpd/Representation.h"
#include "mpd/Segment.h"

struct stream_t;

namespace dash
{
    namespace logic
    {
        class AbstractAdaptationLogic : public IAdaptationLogic
        {
            public:
                AbstractAdaptationLogic             (dash::mpd::IMPDManager *mpdManager, stream_t *stream);
                virtual ~AbstractAdaptationLogic    ();

                virtual void                downloadRateChanged     (uint64_t bpsAvg, uint64_t bpsLastChunk);
                virtual void                bufferLevelChanged      (mtime_t bufferedMicroSec, int bufferedPercent);

                uint64_t                    getBpsAvg               () const;
                uint64_t                    getBpsLastChunk         () const;
                int                         getBufferPercent        () const;

            private:
                int                     bpsAvg;
                long                    bpsLastChunk;
                dash::mpd::IMPDManager  *mpdManager;
                stream_t                *stream;
                mtime_t                 bufferedMicroSec;
                int                     bufferedPercent;
        };
    }
}

#endif /* ABSTRACTADAPTATIONLOGIC_H_ */
