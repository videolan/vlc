/*
 * Url.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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
#ifndef URL_HPP
#define URL_HPP

#include <string>
#include <vector>

namespace dash
{
    namespace mpd
    {
        class Representation;
        class SegmentTemplate;

        class Url
        {
            public:
                class Component
                {
                    friend class Url;
                    public:
                        Component(const std::string &, const SegmentTemplate * = NULL);

                    protected:
                        std::string contextualize(size_t, const Representation *) const;
                        size_t getSegmentNumber(size_t, const Representation *) const;
                        std::string component;
                        const SegmentTemplate *templ;
                };

                Url();
                Url(const Component &);
                explicit Url(const std::string &);
                Url & prepend(const Component &);
                Url & append(const Component &);
                Url & append(const Url &);
                Url & prepend(const Url &);
                std::string toString(size_t, const Representation *) const;
                std::string toString() const;

            private:
                std::vector<Component> components;
        };
    }
}

#endif // URL_HPP
