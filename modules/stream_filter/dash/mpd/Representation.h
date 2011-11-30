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
                Representation          ( const std::map<std::string, std::string>&  attributes);
                virtual ~Representation ();

                std::string         getWidth                () const throw(dash::exception::AttributeNotPresentException);
                std::string         getHeight               () const throw(dash::exception::AttributeNotPresentException);
                std::string         getParX                 () const throw(dash::exception::AttributeNotPresentException);
                std::string         getParY                 () const throw(dash::exception::AttributeNotPresentException);
                std::string         getLang                 () const throw(dash::exception::AttributeNotPresentException);
                std::string         getFrameRate            () const throw(dash::exception::AttributeNotPresentException);
                std::string         getId                   () const throw(dash::exception::AttributeNotPresentException);
                std::string         getBandwidth            () const throw(dash::exception::AttributeNotPresentException);
                std::string         getDependencyId         () const throw(dash::exception::AttributeNotPresentException);
                std::string         getNumberOfChannels     () const throw(dash::exception::AttributeNotPresentException);
                std::string         getSamplingRate         () const throw(dash::exception::AttributeNotPresentException);
                SegmentInfo*        getSegmentInfo          () const throw(dash::exception::ElementNotPresentException);
                TrickModeType*      getTrickModeType        () const throw(dash::exception::ElementNotPresentException);
                ContentProtection*  getContentProtection    () const throw(dash::exception::ElementNotPresentException);

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
