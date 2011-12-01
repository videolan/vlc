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

#include "mpd/CommonAttributesElements.h"
#include "mpd/SegmentInfo.h"
#include "mpd/TrickModeType.h"
#include "mpd/ContentProtection.h"
#include "exceptions/AttributeNotPresentException.h"
#include "exceptions/ElementNotPresentException.h"

namespace dash
{
    namespace mpd
    {
        class Representation : public CommonAttributesElements
        {
            public:
                Representation          ( const std::map<std::string, std::string>&  attributes);
                virtual ~Representation ();

                std::string         getId                   () const throw(dash::exception::AttributeNotPresentException);
                /*
                 *  @return The bitrate required for this representation
                 *          in Bytes per seconds.
                 *          -1 if an error occurs.
                 */
                int                 getBandwidth            () const;
                std::string         getDependencyId         () const throw(dash::exception::AttributeNotPresentException);
                SegmentInfo*        getSegmentInfo          () const throw(dash::exception::ElementNotPresentException);
                TrickModeType*      getTrickModeType        () const throw(dash::exception::ElementNotPresentException);

                void    setSegmentInfo         (SegmentInfo *info);
                void    setTrickModeType       (TrickModeType *trickModeType);
                void    setContentProtection   (ContentProtection *protection);

            private:
                SegmentInfo                         *segmentInfo;
                TrickModeType                       *trickModeType;

        };
    }
}

#endif /* REPRESENTATION_H_ */
