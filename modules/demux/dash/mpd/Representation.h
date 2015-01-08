/*
 * Representation.h
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

#ifndef REPRESENTATION_H_
#define REPRESENTATION_H_

#include <string>

#include "mpd/CommonAttributesElements.h"
#include "mpd/TrickModeType.h"
#include "mpd/SegmentBase.h"
#include "mpd/SegmentList.h"
#include "mpd/SegmentInformation.hpp"
#include "mpd/BaseUrl.h"

namespace dash
{
    namespace mpd
    {
        class AdaptationSet;
        class MPD;

        class Representation : public CommonAttributesElements,
                               public SegmentInformation,
                               public UniqueNess<Representation>
        {
            public:
                Representation( AdaptationSet *, MPD *mpd );
                virtual ~Representation ();

                /*
                 *  @return The bitrate required for this representation
                 *          in bits per seconds.
                 *          Will be a valid value, as the parser refuses Representation
                 *          without bandwidth.
                 */
                uint64_t            getBandwidth            () const;
                void                setBandwidth            ( uint64_t bandwidth );
                int                 getQualityRanking       () const;
                void                setQualityRanking       ( int qualityRanking );
                const std::list<const Representation*>&     getDependencies() const;
                void                addDependency           ( const Representation* dep );
                /**
                 * @return  This SegmentInfo for this Representation.
                 *          It cannot be NULL, or without any Segments in it.
                 *          It can however have a NULL InitSegment
                 */
                TrickModeType*      getTrickModeType        () const;

                void                setTrickMode( TrickModeType *trickModeType );

                void                setWidth                (int width);
                int                 getWidth                () const;
                void                setHeight               (int height);
                int                 getHeight               () const;
                void                setBaseUrl              (BaseUrl *baseUrl);
                MPD*                getMPD                  () const;

                std::vector<std::string> toString(int = 0) const;
                virtual Url         getUrlSegment           () const; /* impl */

            private:
                MPD                                *mpd;
                AdaptationSet                      *adaptationSet;
                uint64_t                            bandwidth;
                int                                 qualityRanking;
                std::list<const Representation*>    dependencies;
                TrickModeType                       *trickModeType;
                BaseUrl                             *baseUrl;
                int                                 width;
                int                                 height;
        };
    }
}

#endif /* REPRESENTATION_H_ */
