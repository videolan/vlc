/*
 * Tags.hpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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
#ifndef TAGS_HPP
#define TAGS_HPP

#include <stdint.h>

#include <string>
#include <vector>
#include <list>
#include <utility>

namespace hls
{

    namespace playlist
    {

        class Attribute
        {
            public:
                Attribute(const std::string &, const std::string &);

                Attribute unescapeQuotes() const;
                uint64_t decimal() const;
                std::string quotedString() const;
                double floatingPoint() const;
                std::vector<uint8_t> hexSequence() const;
                std::pair<std::size_t,std::size_t> getByteRange() const;
                std::pair<int, int> getResolution() const;

                std::string name;
                std::string value;
        };

        class Tag
        {
            public:
                enum
                {
                    EXTXDISCONTINUITY = 0,
                    EXTXENDLIST,
                    EXTXIFRAMESONLY,
                };
                Tag(int);
                virtual ~Tag();
                int getType() const;

            private:
                int type;
        };

        class SingleValueTag : public Tag
        {
            public:
                enum
                {
                    URI = 10,
                    EXTXVERSION,
                    EXTXBYTERANGE,
                    EXTXPROGRAMDATETIME,
                    EXTXTARGETDURATION,
                    EXTXMEDIASEQUENCE,
                    EXTXDISCONTINUITYSEQUENCE,
                    EXTXPLAYLISTTYPE,
                };
                SingleValueTag(int, const std::string &);
                virtual ~SingleValueTag();
                const Attribute & getValue() const;
            private:
                Attribute attr;
        };

        class AttributesTag : public Tag
        {
            public:
                enum
                {
                    EXTXKEY = 20,
                    EXTXMAP,
                    EXTXMEDIA,
                    EXTXSTREAMINF,
                };
                AttributesTag(int, const std::string &);
                virtual ~AttributesTag();
                const Attribute * getAttributeByName(const char *) const;
                void addAttribute(Attribute *);

            protected:
                virtual void parseAttributes(const std::string &);
                std::list<Attribute *> attributes;
        };

        class ValuesListTag : public AttributesTag
        {
            public:
                enum
                {
                    EXTINF = 30
                };
                ValuesListTag(int, const std::string &);
                virtual ~ValuesListTag();

            protected:
                virtual void parseAttributes(const std::string &);
        };

        class TagFactory
        {
            public:
                static Tag * createTagByName(const std::string &, const std::string &);
                static Attribute * createAttributeByName(const std::string &);
        };
    }
}
#endif // TAGS_HPP
