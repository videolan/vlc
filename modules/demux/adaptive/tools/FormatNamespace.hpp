/*
 * FormatNameSpace.hpp
 *****************************************************************************
 * Copyright © 2019 VideoLabs, VideoLAN and VLC Authors
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
#ifndef FORMATNAMESPACE_HPP_
#define FORMATNAMESPACE_HPP_

#include <vlc_common.h>
#include <vlc_es.h>
#include <vector>
#include <string>

namespace adaptive
{
    class FormatNamespace
    {
        public:
            FormatNamespace(const std::string &);
            ~FormatNamespace();
            const es_format_t * getFmt() const;

        private:
            void ParseString(const std::string &);
            void Parse(vlc_fourcc_t, const std::vector<std::string> &);
            void ParseMPEG4Elements(const std::vector<std::string> &);
            es_format_t fmt;
    };
}

#endif
