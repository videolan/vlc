/*
 * CodecDescription.hpp
 *****************************************************************************
 * Copyright (C) 2021 - VideoLabs, VideoLAN and VLC Authors
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
#ifndef CODECDESCRIPTION_HPP
#define CODECDESCRIPTION_HPP

#include "../tools/Properties.hpp"
#include <vlc_es.h>
#include <string>
#include <list>

namespace adaptive
{
    namespace playlist
    {
        class CodecDescription
        {
            public:
                CodecDescription();
                CodecDescription(const std::string &);
                CodecDescription(const CodecDescription &) = delete;
                void operator=(const CodecDescription&) = delete;
                virtual ~CodecDescription();
                const es_format_t *getFmt() const;
                void setDescription(const std::string &);
                void setLanguage(const std::string &);
                void setAspectRatio(const AspectRatio &);
                void setFrameRate(const Rate &);
                void setSampleRate(const Rate &);

            protected:
                es_format_t fmt;
        };

        class CodecDescriptionList : public std::list<CodecDescription *>
        {
            public:
                CodecDescriptionList();
                ~CodecDescriptionList();
                CodecDescriptionList(const CodecDescriptionList &) = delete;
                void operator=(const CodecDescriptionList&) = delete;
        };
    }
}

#endif // CODECDESCRIPTION_HPP
