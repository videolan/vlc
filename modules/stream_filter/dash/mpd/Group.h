/*
 * Group.h
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

#ifndef GROUP_H_
#define GROUP_H_

#include <vector>
#include <string>
#include <map>

#include "mpd/Representation.h"
#include "mpd/ContentProtection.h"
#include "mpd/Accessibility.h"
#include "mpd/Viewpoint.h"
#include "mpd/Rating.h"
#include "exceptions/AttributeNotPresentException.h"
#include "exceptions/ElementNotPresentException.h"

namespace dash
{
    namespace mpd
    {
        class Group
        {
            public:
                Group           (std::map<std::string, std::string>  attributes);
                virtual ~Group  ();

                std::string                     getWidth                () throw(dash::exception::AttributeNotPresentException);
                std::string                     getHeight               () throw(dash::exception::AttributeNotPresentException);
                std::string                     getParX                 () throw(dash::exception::AttributeNotPresentException);
                std::string                     getParY                 () throw(dash::exception::AttributeNotPresentException);
                std::string                     getLang                 () throw(dash::exception::AttributeNotPresentException);
                std::string                     getMimeType             () throw(dash::exception::AttributeNotPresentException);
                std::string                     getFrameRate            () throw(dash::exception::AttributeNotPresentException);
                std::string                     getNumberOfChannels     () throw(dash::exception::AttributeNotPresentException);
                std::string                     getSamplingRate         () throw(dash::exception::AttributeNotPresentException);
                std::string                     getSubSegmentAlignment  () throw(dash::exception::AttributeNotPresentException);
                std::vector<Representation *>   getRepresentations      ();
                Viewpoint*                      getViewpoint            () throw(dash::exception::ElementNotPresentException);
                ContentProtection*              getContentProtection    () throw(dash::exception::ElementNotPresentException);
                Accessibility*                  getAccessibility        () throw(dash::exception::ElementNotPresentException);
                Rating*                         getRating               () throw(dash::exception::ElementNotPresentException);

                void addRepresentation      (Representation *rep);
                void setViewpoint           (Viewpoint *viewpoint);
                void setContentProtection   (ContentProtection *protection);
                void setAccessibility       (Accessibility *accessibility);
                void setRating              (Rating *rating);

            private:
                std::map<std::string, std::string>  attributes;
                std::vector<Representation *>       representations;
                ContentProtection                   *contentProtection;
                Accessibility                       *accessibility;
                Viewpoint                           *viewpoint;
                Rating                              *rating;
        };
    }
}

#endif /* GROUP_H_ */
