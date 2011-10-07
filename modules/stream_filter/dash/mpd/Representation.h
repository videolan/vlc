/*
 * Representation.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef REPRESENTATION_H_
#define REPRESENTATION_H_

#include <string>
#include <vector>
#include <map>

#include "mpd/SegmentInfo.h"
#include "mpd/TrickModeType.h"
#include "mpd/ContentProtection.h"
#include "exceptions/AttributeNotPresentException.h"
#include "exceptions/ElementNotPresentException.h"

namespace dash
{
    namespace mpd
    {
        class Representation
        {
            public:
                Representation          (std::map<std::string, std::string>  attributes);
                virtual ~Representation ();

                std::string         getWidth                () throw(dash::exception::AttributeNotPresentException);
                std::string         getHeight               () throw(dash::exception::AttributeNotPresentException);
                std::string         getParX                 () throw(dash::exception::AttributeNotPresentException);
                std::string         getParY                 () throw(dash::exception::AttributeNotPresentException);
                std::string         getLang                 () throw(dash::exception::AttributeNotPresentException);
                std::string         getFrameRate            () throw(dash::exception::AttributeNotPresentException);
                std::string         getId                   () throw(dash::exception::AttributeNotPresentException);
                std::string         getBandwidth            () throw(dash::exception::AttributeNotPresentException);
                std::string         getDependencyId         () throw(dash::exception::AttributeNotPresentException);
                std::string         getNumberOfChannels     () throw(dash::exception::AttributeNotPresentException);
                std::string         getSamplingRate         () throw(dash::exception::AttributeNotPresentException);
                SegmentInfo*        getSegmentInfo          () throw(dash::exception::ElementNotPresentException);
                TrickModeType*      getTrickModeType        () throw(dash::exception::ElementNotPresentException);
                ContentProtection*  getContentProtection    () throw(dash::exception::ElementNotPresentException);

                void    setSegmentInfo         (SegmentInfo *info);
                void    setTrickModeType       (TrickModeType *trickModeType);
                void    setContentProtection   (ContentProtection *protection);

            private:
                std::map<std::string, std::string>  attributes;
                SegmentInfo                         *segmentInfo;
                TrickModeType                       *trickModeType;
                ContentProtection                   *contentProtection;

        };
    }
}

#endif /* REPRESENTATION_H_ */
