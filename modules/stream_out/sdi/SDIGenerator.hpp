/*****************************************************************************
 * SDIGenerator.hpp: SDI Image and sound generators
 *****************************************************************************
 * Copyright © 2014-2016 VideoLAN and VideoLAN Authors
 *             2018-2019 VideoLabs
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
#ifndef SDIGENERATOR_HPP
#define SDIGENERATOR_HPP

namespace sdi
{
    class Generator
    {
        public:
            static picture_t * Picture(vlc_object_t *, const char*,
                                       const video_format_t *);
    };
}

#endif
