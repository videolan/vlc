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

namespace dash
{
    namespace mpd
    {
        class MPD;

        enum Profile
        {
            UnknownProfile,
            Full2011,
            Basic,
            BasicCM
        };
        class IMPDManager
        {
            public:
                virtual const std::vector<Period *>&   getPeriods              () const                 = 0;
                virtual Period*                 getFirstPeriod          ()                              = 0;
                virtual Period*                 getNextPeriod           (Period *period)                = 0;
                virtual Representation*         getBestRepresentation   (Period *period)                = 0;
                virtual std::vector<const Segment *> getSegments        (Representation *rep)           = 0;
                virtual Representation*         getRepresentation       (Period *period, long bitrate)  = 0;
                virtual const MPD*              getMPD                  () const = 0;
                virtual ~IMPDManager(){}
        };
    }
}
#endif /* IMPDMANAGER_H_ */
