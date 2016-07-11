/*
 * Tags.cpp
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Tags.hpp"
#include <sstream>
#include <stack>

#include <vlc_common.h>

using namespace hls::playlist;

Attribute::Attribute(const std::string &name_, const std::string &value_)
{
    name = name_;
    value = value_;
}

uint64_t Attribute::decimal() const
{
    std::istringstream is(value);
    is.imbue(std::locale("C"));
    uint64_t ret;
    is >> ret;
    return ret;
}

double Attribute::floatingPoint() const
{
    std::istringstream is(value);
    is.imbue(std::locale("C"));
    double ret;
    is >> ret;
    return ret;
}

std::vector<uint8_t> Attribute::hexSequence() const
{
    std::vector<uint8_t> ret;
    if(value.length() > 2 && (value.substr(0, 2) == "0X" || value.substr(0, 2) == "0x") )
    {
        for(size_t i=2; i<=(value.length() - 2); i+=2)
        {
            unsigned val;
            std::stringstream ss(value.substr(i, 2));
            ss.imbue(std::locale("C"));
            ss >> std::hex >> val;
            ret.push_back(val);
        }
    }
    return ret;
}

std::pair<std::size_t,std::size_t> Attribute::getByteRange() const
{
    std::size_t length = 0;
    std::size_t offset = 0;
    std::istringstream is(value);
    is.imbue(std::locale("C"));

    if(!is.eof())
    {
        is >> length;
        if(!is.eof())
        {
            char c = is.get();
            if(c == '@' && !is.eof())
                is >> offset;
        }
    }

    return std::make_pair(offset, length);
}

std::pair<int, int> Attribute::getResolution() const
{
    int w = 0, h = 0;

    std::istringstream is(value);
    is.imbue(std::locale("C"));
    if(!is.eof())
    {
        is >> w;
        if(!is.eof())
        {
            char c = is.get();
            if(c == 'x' && !is.eof())
                is >> h;
        }
    }

    return std::make_pair(w, h);
}

Attribute Attribute::unescapeQuotes() const
{
    return Attribute(this->name, quotedString());
}

std::string Attribute::quotedString() const
{
    if(value.length() < 2)
        return "";

    std::istringstream is(value.substr(1, value.length() - 2));
    std::ostringstream os;

    char c;
    while(is.get(c))
    {
        if(c == '\\')
        {
            if(!is.get(c))
                break;
        }

        os << c;
    }

    return os.str();
}

Tag::Tag(int type_)
{
    type = type_;
}

Tag::~Tag()
{
}

int Tag::getType() const
{
    return type;
}

SingleValueTag::SingleValueTag(int type, const std::string &v)
    : Tag(type), attr("", v)
{
}

SingleValueTag::~SingleValueTag()
{

}

const Attribute &SingleValueTag::getValue() const
{
    return attr;
}

AttributesTag::AttributesTag(int type, const std::string &v) : Tag(type)
{
    parseAttributes(v);
}

AttributesTag::~AttributesTag()
{
    std::list<Attribute *>::const_iterator it;
    for(it = attributes.begin(); it != attributes.end(); ++it)
        delete *it;
}

const Attribute * AttributesTag::getAttributeByName(const char *name) const
{
    std::list<Attribute *>::const_iterator it;
    for(it = attributes.begin(); it != attributes.end(); ++it)
        if((*it)->name == name)
            return *it;

    return NULL;
}

void AttributesTag::addAttribute(Attribute *attr)
{
    attributes.push_back(attr);
}

void AttributesTag::parseAttributes(const std::string &field)
{
    std::istringstream iss(field);
    std::ostringstream oss;

    while(!iss.eof())
    {
        /* parse attribute name */
        while(!iss.eof())
        {
            char c = iss.peek();
            if((c >= 'A' && c <= 'Z') || c == '-')
            {
                oss.put((char)iss.get());
            }
            else if(c == '=')
            {
                iss.get();
                break;
            }
            else /* out of range */
                return;
        }

        std::string attrname = oss.str();
        oss.str("");

        /* parse attributes value */
        bool b_quoted = false;
        while(!iss.eof())
        {
            char c = iss.peek();
            if(c == '\\' && b_quoted)
            {
                iss.get();
            }
            else if(c == ',' && !b_quoted)
            {
                iss.get();
                break;
            }
            else if(c == '"')
            {
                b_quoted = !b_quoted;
            }

            if(!iss.eof())
                oss.put((char)iss.get());
        }

        std::string attrvalue = oss.str();
        oss.str("");

        Attribute *attribute = new (std::nothrow) Attribute(attrname, attrvalue);
        if(attribute)
            attributes.push_back(attribute);
    }
}


ValuesListTag::ValuesListTag(int type, const std::string &v) : AttributesTag(type, v)
{
    parseAttributes(v);
}

ValuesListTag::~ValuesListTag()
{
}

void ValuesListTag::parseAttributes(const std::string &field)
{
    std::size_t pos = field.find(',');
    if(pos != std::string::npos)
    {
        Attribute *attr = new (std::nothrow) Attribute("DURATION", field.substr(0, pos));
        if(attr)
            addAttribute(attr);

        attr = new (std::nothrow) Attribute("TITLE", field.substr(pos));
        if(attr)
            addAttribute(attr);
    }
}

Tag * TagFactory::createTagByName(const std::string &name, const std::string &value)
{
    struct
    {
        const char *psz;
        const int i;
    } const exttagmapping[] = {
        {"EXT-X-BYTERANGE",                 SingleValueTag::EXTXBYTERANGE},
        {"EXT-X-DISCONTINUITY",             Tag::EXTXDISCONTINUITY},
        {"EXT-X-KEY",                       AttributesTag::EXTXKEY},
        {"EXT-X-MAP",                       AttributesTag::EXTXMAP},
        {"EXT-X-PROGRAM-DATE-TIME",         SingleValueTag::EXTXPROGRAMDATETIME},
        {"EXT-X-TARGETDURATION",            SingleValueTag::EXTXTARGETDURATION},
        {"EXT-X-MEDIA-SEQUENCE",            SingleValueTag::EXTXMEDIASEQUENCE},
        {"EXT-X-DISCONTINUITY-SEQUENCE",    SingleValueTag::EXTXDISCONTINUITYSEQUENCE},
        {"EXT-X-ENDLIST",                   Tag::EXTXENDLIST},
        {"EXT-X-PLAYLIST-TYPE",             SingleValueTag::EXTXPLAYLISTTYPE},
        {"EXT-X-I-FRAMES-ONLY",             Tag::EXTXIFRAMESONLY},
        {"EXT-X-MEDIA",                     AttributesTag::EXTXMEDIA},
        {"EXT-X-STREAM-INF",                AttributesTag::EXTXSTREAMINF},
        {"EXTINF",                          ValuesListTag::EXTINF},
        {"",                                SingleValueTag::URI},
        {NULL,                              0},
    };


    for(int i=0; exttagmapping[i].psz; i++)
    {
        if(name != exttagmapping[i].psz)
            continue;

        switch(exttagmapping[i].i)
        {
        case Tag::EXTXDISCONTINUITY:
        case Tag::EXTXENDLIST:
        case Tag::EXTXIFRAMESONLY:
            return new (std::nothrow) Tag(exttagmapping[i].i);

        case SingleValueTag::URI:
        case SingleValueTag::EXTXVERSION:
        case SingleValueTag::EXTXBYTERANGE:
        case SingleValueTag::EXTXPROGRAMDATETIME:
        case SingleValueTag::EXTXTARGETDURATION:
        case SingleValueTag::EXTXMEDIASEQUENCE:
        case SingleValueTag::EXTXDISCONTINUITYSEQUENCE:
        case SingleValueTag::EXTXPLAYLISTTYPE:
            return new (std::nothrow) SingleValueTag(exttagmapping[i].i, value);

        case ValuesListTag::EXTINF:
            return new (std::nothrow) ValuesListTag(exttagmapping[i].i, value);

        case AttributesTag::EXTXKEY:
        case AttributesTag::EXTXMAP:
        case AttributesTag::EXTXMEDIA:
        case AttributesTag::EXTXSTREAMINF:
            return new (std::nothrow) AttributesTag(exttagmapping[i].i, value);
        }

    }


    return NULL;
}
