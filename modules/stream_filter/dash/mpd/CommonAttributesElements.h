/*****************************************************************************
 * CommonAttributesElements.h: Defines the common attributes and elements
 *                             for some Dash elements.
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <beauze.h@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef COMMONATTRIBUTESELEMENTS_H
#define COMMONATTRIBUTESELEMENTS_H

#include "exceptions/AttributeNotPresentException.h"
#include "exceptions/ElementNotPresentException.h"

#include <map>
#include <string>

#include "mpd/ContentProtection.h"

namespace dash
{
    namespace mpd
    {
        class CommonAttributesElements
        {
            public:
                CommonAttributesElements( const std::map<std::string, std::string>& attributes );
                virtual ~CommonAttributesElements();
                int                 getWidth                () const;
                int                 getHeight               () const;
                int                 getParX                 () const;
                int                 getParY                 () const;
                std::string         getLang                 () const throw(dash::exception::AttributeNotPresentException);
                int                 getFrameRate            () const;
                std::string         getNumberOfChannels     () const throw(dash::exception::AttributeNotPresentException);
                std::string         getSamplingRate         () const throw(dash::exception::AttributeNotPresentException);
                ContentProtection*  getContentProtection    () const throw(dash::exception::ElementNotPresentException);

            protected:
                std::map<std::string, std::string>  attributes;
                ContentProtection                   *contentProtection;
        };
    }
}

#endif // COMMONATTRIBUTESELEMENTS_H
