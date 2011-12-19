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

#include <list>
#include <string>

#include "exceptions/ElementNotPresentException.h"
#include "mpd/ContentProtection.h"

namespace dash
{
    namespace mpd
    {
        class CommonAttributesElements
        {
            public:
                CommonAttributesElements();
                virtual ~CommonAttributesElements();
                const std::string&              getMimeType() const;
                void                            setMimeType( const std::string &mimeType );
                int                             getWidth() const;
                void                            setWidth( int width );
                int                             getHeight() const;
                void                            setHeight( int height );
                int                             getParX() const;
                void                            setParX( int parX );
                int                             getParY() const;
                void                            setParY( int parY );
                int                             getFrameRate() const;
                void                            setFrameRate( int frameRate );
                const std::list<std::string>&   getLang() const;
                void                            addLang( const std::string &lang );
                const std::list<std::string>&   getNumberOfChannels() const;
                void                            addChannel( const std::string &channel );
                const std::list<int>&           getSamplingRates() const;
                void                            addSampleRate( int sampleRate );
                ContentProtection*              getContentProtection() const throw(dash::exception::ElementNotPresentException);

            protected:
                std::string                         mimeType;
                int                                 width;
                int                                 height;
                int                                 parX;
                int                                 parY;
                int                                 frameRate;
                std::list<std::string>              lang;
                std::list<std::string>              channels;
                std::list<int>                      sampleRates;
                ContentProtection                   *contentProtection;
        };
    }
}

#endif // COMMONATTRIBUTESELEMENTS_H
