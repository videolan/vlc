/*
 * ICanonicalUrl.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC Authors
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
#ifndef CANONICALURL_HPP
#define CANONICALURL_HPP

#include "Url.hpp"

namespace adaptive
{
    namespace playlist
    {
        class ICanonicalUrl
        {
            public:
                ICanonicalUrl( const ICanonicalUrl *parent = NULL ) { setParent(parent); }
                virtual ~ICanonicalUrl() = default;
                virtual Url getUrlSegment() const = 0;
                void setParent( const ICanonicalUrl *parent ) { parentUrlMember = parent; }

            protected:
                Url getParentUrlSegment() const {
                    return (parentUrlMember) ? parentUrlMember->getUrlSegment()
                                             : Url();
                }

            private:
                const ICanonicalUrl *parentUrlMember;
        };
    }
}

#endif // CANONICALURL_HPP
