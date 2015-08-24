/*
 * Parser.cpp
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

#include "Parser.hpp"
#include "HLSSegment.hpp"
#include "Representation.hpp"
#include "../adaptative/playlist/BasePeriod.h"
#include "../adaptative/playlist/BaseAdaptationSet.h"
#include "../adaptative/playlist/SegmentList.h"
#include "../adaptative/tools/Retrieve.hpp"
#include "../adaptative/tools/Helper.h"
#include "M3U8.hpp"
#include "Tags.hpp"

#include <vlc_strings.h>
#include <vlc_stream.h>
#include <cstdio>
#include <sstream>
#include <map>

using namespace adaptative;
using namespace adaptative::playlist;
using namespace hls::playlist;

Parser::Parser(stream_t *stream)
{
    p_stream = stream;
}

Parser::~Parser   ()
{
}

static std::list<Tag *> getTagsFromList(std::list<Tag *> &list, int tag)
{
    std::list<Tag *> ret;
    std::list<Tag *>::const_iterator it;
    for(it = list.begin(); it != list.end(); ++it)
    {
        if( (*it)->getType() == tag )
            ret.push_back(*it);
    }
    return ret;
}

static void releaseTagsList(std::list<Tag *> &list)
{
    std::list<Tag *>::const_iterator it;
    for(it = list.begin(); it != list.end(); ++it)
        delete *it;
    list.clear();
}

void Parser::parseAdaptationSet(BasePeriod *period, const AttributesTag *)
{
    BaseAdaptationSet *adaptSet = new (std::nothrow) BaseAdaptationSet(period);
    if(adaptSet)
    {
        period->addAdaptationSet(adaptSet);
    }
}

void Parser::parseRepresentation(BaseAdaptationSet *adaptSet, const AttributesTag * tag)
{
    if(!tag->getAttributeByName("URI"))
        return;

    Url url;
    if(tag->getType() == AttributesTag::EXTXMEDIA)
    {
        url = Url(tag->getAttributeByName("URI")->quotedString());
    }
    else
    {
        url = Url(tag->getAttributeByName("URI")->value);
    }

    if(!url.hasScheme())
        url = url.prepend(adaptSet->getUrlSegment());

    void *p_data;
    const size_t i_data = Retrieve::HTTP((vlc_object_t*)p_stream, url.toString(), &p_data);
    if(p_data)
    {
        stream_t *substream = stream_MemoryNew((vlc_object_t *)p_stream, (uint8_t *)p_data, i_data, false);
        if(substream)
        {
            std::list<Tag *> tagslist = parseEntries(substream);
            stream_Delete(substream);

            parseRepresentation(adaptSet, tag, tagslist);

            releaseTagsList(tagslist);
        }
    }
}

void Parser::parseRepresentation(BaseAdaptationSet *adaptSet, const AttributesTag * tag,
                                 const std::list<Tag *> &tagslist)
{
    const Attribute *uriAttr = tag->getAttributeByName("URI");
    const Attribute *bwAttr = tag->getAttributeByName("BANDWIDTH");
    const Attribute *codecsAttr = tag->getAttributeByName("CODECS");
    const Attribute *resAttr = tag->getAttributeByName("RESOLUTION");

    Representation *rep = new (std::nothrow) Representation(adaptSet);
    if(rep)
    {
        if(uriAttr)
        {
            std::string uri;
            if(tag->getType() == AttributesTag::EXTXMEDIA)
            {
                uri = uriAttr->quotedString();
            }
            else
            {
                uri = uriAttr->value;
            }
            size_t pos = uri.find_last_of('/');
            if(pos != std::string::npos)
                rep->baseUrl.Set(new Url(uri.substr(0, pos+1)));
        }

        if(bwAttr)
            rep->setBandwidth(bwAttr->decimal());

        if(codecsAttr)
        {
            std::list<std::string> list = Helper::tokenize(codecsAttr->quotedString(), ',');
            std::list<std::string>::const_iterator it;
            for(it=list.begin(); it!=list.end(); ++it)
            {
                std::size_t pos = (*it).find_first_of('.', 0);
                if(pos != std::string::npos)
                    rep->addCodec((*it).substr(0, pos));
                else
                    rep->addCodec(*it);
            }
        }

        /* if more than 1 codec, don't probe, can't be packed audio */
        if(rep->getCodecs().size() > 1)
            rep->setMimeType("video/mp2t");

        if(resAttr)
        {
            std::pair<int, int> res = resAttr->getResolution();
            if(res.first * res.second)
            {
                rep->setWidth(res.first);
                rep->setHeight(res.second);
            }
        }

        parseSegments(rep, tagslist);

        adaptSet->addRepresentation(rep);
    }
}

void Parser::parseSegments(Representation *rep, const std::list<Tag *> &tagslist)
{
    SegmentList *segmentList = new (std::nothrow) SegmentList(rep);
    rep->setSegmentList(segmentList);

    rep->timescale.Set(100);

    int64_t totalduration = 0;
    int64_t nzStartTime = 0;
    uint64_t sequenceNumber = 0;
    std::size_t prevbyterangeoffset = 0;
    const SingleValueTag *ctx_byterange = NULL;
    SegmentEncryption encryption;

    std::list<Tag *>::const_iterator it;
    for(it = tagslist.begin(); it != tagslist.end(); ++it)
    {
        const Tag *tag = *it;
        switch(tag->getType())
        {
            /* using static cast as attribute type permits avoiding class check */
            case SingleValueTag::EXTXMEDIASEQUENCE:
            {
                sequenceNumber = (static_cast<const SingleValueTag*>(tag))->getValue().decimal();
            }
            break;

            case URITag::EXTINF:
            {
                const URITag *uritag = static_cast<const URITag *>(tag);
                HLSSegment *segment = new (std::nothrow) HLSSegment(rep, sequenceNumber++);
                if(!segment)
                    break;

                if(uritag->getAttributeByName("URI"))
                    segment->setSourceUrl(uritag->getAttributeByName("URI")->value);

                if(uritag->getAttributeByName("DURATION"))
                {
                    segment->duration.Set(uritag->getAttributeByName("DURATION")->floatingPoint() * rep->timescale.Get());
                    segment->startTime.Set(nzStartTime);
                    nzStartTime += segment->duration.Get();
                    totalduration += segment->duration.Get();
                }

                segmentList->addSegment(segment);

                if(ctx_byterange)
                {
                    std::pair<std::size_t,std::size_t> range = ctx_byterange->getValue().getByteRange();
                    if(range.first == 0)
                        range.first = prevbyterangeoffset;
                    prevbyterangeoffset = range.first + range.second;
                    segment->setByteRange(range.first, prevbyterangeoffset);
                    ctx_byterange = NULL;
                }

                if(encryption.method != SegmentEncryption::NONE)
                    segment->setEncryption(encryption);
            }
            break;

            case SingleValueTag::EXTXPLAYLISTTYPE:
                rep->b_live = (static_cast<const SingleValueTag *>(tag)->getValue().value != "VOD");
                break;

            case SingleValueTag::EXTXBYTERANGE:
                ctx_byterange = static_cast<const SingleValueTag *>(tag);
                break;

            case AttributesTag::EXTXKEY:
            {
                const AttributesTag *keytag = static_cast<const AttributesTag *>(tag);
                if( keytag->getAttributeByName("METHOD") &&
                    keytag->getAttributeByName("METHOD")->value == "AES-128" &&
                    keytag->getAttributeByName("URI") )
                {
                    encryption.method = SegmentEncryption::AES_128;
                    encryption.key.clear();
                    uint8_t *p_data;
                    const uint64_t read = Retrieve::HTTP(VLC_OBJECT(p_stream),
                                                         keytag->getAttributeByName("URI")->quotedString(),
                                                         (void **) &p_data);
                    if(p_data)
                    {
                        if(read == 16)
                        {
                            encryption.key.resize(16);
                            memcpy(&encryption.key[0], p_data, 16);
                        }
                        free(p_data);
                    }

                    if(keytag->getAttributeByName("IV"))
                    {
                        encryption.iv.clear();
                        encryption.iv = keytag->getAttributeByName("IV")->hexSequence();
                    }
                }
                else
                {
                    /* unsupported or invalid */
                    encryption.method = SegmentEncryption::NONE;
                    encryption.key.clear();
                    encryption.iv.clear();
                }
            }
            break;

            case Tag::EXTXENDLIST:
                rep->b_live = false;
                break;
        }
    }

    if(rep->isLive())
    {
        rep->getPlaylist()->duration.Set(0);
    }
    else if(totalduration > rep->getPlaylist()->duration.Get())
    {
        rep->getPlaylist()->duration.Set(CLOCK_FREQ * totalduration / rep->timescale.Get());
    }
}

M3U8 * Parser::parse(const std::string &playlisturl)
{
    char *psz_line = stream_ReadLine(p_stream);
    if(!psz_line || strcmp(psz_line, "#EXTM3U"))
    {
        free(psz_line);
        return NULL;
    }
    free(psz_line);

    M3U8 *playlist = new (std::nothrow) M3U8(p_stream);
    if(!playlist)
        return NULL;

    if(!playlisturl.empty())
    {
        size_t pos = playlisturl.find_last_of('/');
        if(pos != std::string::npos)
            playlist->addBaseUrl(playlisturl.substr(0, pos + 1));
    }

    BasePeriod *period = new (std::nothrow) BasePeriod( playlist );
    if(!period)
        return playlist;

    std::list<Tag *> tagslist = parseEntries(p_stream);
    bool b_masterplaylist = !getTagsFromList(tagslist, AttributesTag::EXTXSTREAMINF).empty();
    if(b_masterplaylist)
    {
        std::list<Tag *>::const_iterator it;
        std::map<std::string, AttributesTag *> groupsmap;

        /* We'll need to create an adaptation set for each media group / alternative rendering
         * we create a list of playlist being and alternative/group */
        std::list<Tag *> mediainfotags = getTagsFromList(tagslist, AttributesTag::EXTXMEDIA);
        for(it = mediainfotags.begin(); it != mediainfotags.end(); ++it)
        {
            AttributesTag *tag = dynamic_cast<AttributesTag *>(*it);
            if(tag && tag->getAttributeByName("URI"))
            {
                std::pair<std::string, AttributesTag *> pair(tag->getAttributeByName("URI")->quotedString(), tag);
                groupsmap.insert(pair);
            }
        }

        /* Then we parse all playlists uri and add them, except when alternative */
        BaseAdaptationSet *adaptSet = new (std::nothrow) BaseAdaptationSet(period);
        if(adaptSet)
        {
            std::list<Tag *> streaminfotags = getTagsFromList(tagslist, AttributesTag::EXTXSTREAMINF);
            for(it = streaminfotags.begin(); it != streaminfotags.end(); ++it)
            {
                AttributesTag *tag = dynamic_cast<AttributesTag *>(*it);
                if(tag && tag->getAttributeByName("URI"))
                {
                    if(groupsmap.find(tag->getAttributeByName("URI")->value) == groupsmap.end())
                    {
                        /* not a group, belong to default adaptation set */
                        parseRepresentation(adaptSet, tag);
                    }
                }
            }
            if(!adaptSet->getRepresentations().empty())
                period->addAdaptationSet(adaptSet);
            else
                delete adaptSet;
        }

        /* Finally add all groups */
        std::map<std::string, AttributesTag *>::const_iterator groupsit;
        for(groupsit = groupsmap.begin(); groupsit != groupsmap.end(); ++groupsit)
        {
            BaseAdaptationSet *altAdaptSet = new (std::nothrow) BaseAdaptationSet(period);
            if(altAdaptSet)
            {
                std::pair<std::string, AttributesTag *> pair = *groupsit;
                parseRepresentation(altAdaptSet, pair.second);

                if(pair.second->getAttributeByName("NAME"))
                   altAdaptSet->description.Set(pair.second->getAttributeByName("NAME")->quotedString());

                if(pair.second->getAttributeByName("LANGUAGE"))
                {
                    std::string lang = pair.second->getAttributeByName("LANGUAGE")->quotedString();
                    std::size_t pos = lang.find_first_of('-');
                    if(pos != std::string::npos && pos > 0)
                        altAdaptSet->addLang(lang.substr(0, pos));
                    else if (lang.size() < 4)
                        altAdaptSet->addLang(lang);
                }

                if(!altAdaptSet->getRepresentations().empty())
                    period->addAdaptationSet(altAdaptSet);
                else
                    delete altAdaptSet;
            }
        }

    }
    else
    {
        BaseAdaptationSet *adaptSet = new (std::nothrow) BaseAdaptationSet(period);
        if(adaptSet)
        {
            period->addAdaptationSet(adaptSet);
            AttributesTag *tag = new AttributesTag(AttributesTag::EXTXSTREAMINF, "");
            parseRepresentation(adaptSet, tag, tagslist);
            delete tag;
        }
    }

    playlist->addPeriod(period);

    releaseTagsList(tagslist);

    playlist->debug();
    return playlist;
}

std::list<Tag *> Parser::parseEntries(stream_t *stream)
{
    std::list<Tag *> entrieslist;
    Tag *lastTag = NULL;
    char *psz_line;

    while((psz_line = stream_ReadLine(stream)))
    {
        if(*psz_line == '#')
        {
            if(!strncmp(psz_line, "#EXT", 4)) //tag
            {
                std::string key;
                std::string attributes;
                const char *split = strchr(psz_line, ':');
                if(split)
                {
                    key = std::string(psz_line + 1, split - psz_line - 1);
                    attributes = std::string(split + 1);
                }
                else
                {
                    key = std::string(psz_line + 1);
                }

                if(!key.empty())
                {
                    Tag *tag = TagFactory::createTagByName(key, attributes);
                    if(tag)
                        entrieslist.push_back(tag);
                    lastTag = tag;
                }
            }
        }
        else if(*psz_line && lastTag)
        {
            AttributesTag *attrTag = dynamic_cast<AttributesTag *>(lastTag);
            if(attrTag)
            {
                Attribute *uriAttr = new (std::nothrow) Attribute("URI", std::string(psz_line));
                if(uriAttr)
                    attrTag->addAttribute(uriAttr);
            }
            lastTag = NULL;
        }
        else // drop
        {
            lastTag = NULL;
        }

        free(psz_line);
    }

    return entrieslist;
}
