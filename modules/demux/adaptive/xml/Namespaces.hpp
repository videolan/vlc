/*
 * Namespaces.hpp
 *****************************************************************************
 * Copyright (C) 2024 - VideoLabs, VideoLAN and VLC Authors
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
#ifndef NAMESPACES_H_
#define NAMESPACES_H_

#include <vector>
#include <string>
#include <memory>

namespace adaptive
{
    namespace xml
    {
        class Namespaces
        {
            public:
                using Entry = std::string;
                using Ptr = std::shared_ptr<Entry>;

                Namespaces() = default;
                Ptr registerNamespace(const char *);
                Ptr getNamespace(const std::string &);
                Ptr getNamespace(const char *);

            private:
                std::vector<Ptr> nss;
        };
    }
}

#endif /* NAMESPACES_HPP_ */
