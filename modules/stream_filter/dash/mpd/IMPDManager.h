/*
 * IMPDManager.h
 *
 *  Created on: Apr 22, 2011
 *      Author: Christopher Müller
 */

#ifndef IMPDMANAGER_H_
#define IMPDMANAGER_H_

#include "mpd/Period.h"
#include "mpd/Representation.h"
#include "mpd/ISegment.h"

namespace dash
{
    namespace mpd
    {
        enum Profile
        {
            NotValid,
            Full2011,
            Basic,
            BasicCM,
        };
        class IMPDManager
        {
            public:
                virtual std::vector<Period *>   getPeriods              ()                              = 0;
                virtual Period*                 getFirstPeriod          ()                              = 0;
                virtual Period*                 getNextPeriod           (Period *period)                = 0;
                virtual Representation*         getBestRepresentation   (Period *period)                = 0;
                virtual std::vector<ISegment *> getSegments             (Representation *rep)           = 0;
                virtual Representation*         getRepresentation       (Period *period, long bitrate)  = 0;
                virtual ~IMPDManager(){}
        };
    }
}
#endif /* IMPDMANAGER_H_ */
