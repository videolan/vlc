/*
 * Segment.h
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

#ifndef SEGMENT_H_
#define SEGMENT_H_

#include <map>
#include <string>

#include "exceptions/AttributeNotPresentException.h"
#include "mpd/ISegment.h"

namespace dash
{
    namespace mpd
    {
        class Segment : public ISegment
        {
            public:
                Segment             (std::map<std::string, std::string>  attributes);
                virtual ~Segment    ();

                std::string getSourceUrl() throw(dash::exception::AttributeNotPresentException);

            private:
                std::map<std::string, std::string>  attributes;
        };
    }
}

#endif /* SEGMENT_H_ */
