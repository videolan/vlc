/*****************************************************************************
 * CommonAttributesElements.h: Defines the common attributes and elements
 *                             for some Dash elements.
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#ifndef COMMONATTRIBUTESELEMENTS_H
#define COMMONATTRIBUTESELEMENTS_H

#include "../tools/Properties.hpp"
#include <string>

namespace adaptive
{
    namespace playlist
    {
        class CommonAttributesElements
        {
            public:
                CommonAttributesElements(CommonAttributesElements * = nullptr);
                virtual ~CommonAttributesElements();
                virtual const std::string&      getMimeType() const;
                void                            setMimeType( const std::string &mimeType );
                int                             getWidth() const;
                void                            setWidth( int width );
                int                             getHeight() const;
                void                            setHeight( int height );
                const AspectRatio &             getAspectRatio() const;
                void                            setAspectRatio(const AspectRatio &);
                const Rate &                    getFrameRate() const;
                void                            setFrameRate(const Rate &);
                const Rate &                    getSampleRate() const;
                void                            setSampleRate(const Rate &);

            protected:
                std::string                         mimeType;
                int                                 width;
                int                                 height;
                AspectRatio                         aspectRatio;
                Rate                                frameRate;
                Rate                                sampleRate;

            private:
                CommonAttributesElements           *parentCommonAttributes;
        };
    }
}

#endif // COMMONATTRIBUTESELEMENTS_H
