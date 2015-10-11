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
#include "../HLSStreamFormat.hpp"
#include "M3U8.hpp"
#include "Tags.hpp"

#include <vlc_strings.h>
#include <vlc_stream.h>
#include <cstdio>
#include <sstream>
#include <map>
#include <cctype>
#include <algorithm>

using namespace adaptative;
using namespace adaptative::playlist;
using namespace hls::playlist;

M3U8Parser::M3U8Parser()
{
}

M3U8Parser::~M3U8Parser   ()
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

void M3U8Parser::setFormatFromCodecs(Representation *rep, const std::string codecsstring)
{
    std::list<std::string> codecs;
    std::list<std::string> tokens = Helper::tokenize(codecsstring, ',');
    std::list<std::string>::const_iterator it;
    for(it=tokens.begin(); it!=tokens.end(); ++it)
    {
        /* Truncate init data */
        std::size_t pos = (*it).find_first_of('.', 0);
        if(pos != std::string::npos)
            codecs.push_back((*it).substr(0, pos));
        else
            codecs.push_back(*it);
    }

    if(!codecs.empty())
    {
        if(codecs.size() == 1)
        {
            std::string codec = codecs.front();
            transform(codec.begin(), codec.end(), codec.begin(), (int (*)(int))std::tolower);
            if(codec == "mp4a")
                rep->streamFormat = StreamFormat(HLSStreamFormat::PACKEDAAC);
        }
        else
        {
            rep->streamFormat = StreamFormat(HLSStreamFormat::MPEG2TS);
        }
    }
}

void M3U8Parser::setFormatFromExtension(Representation *rep, const std::string &filename)
{
    std::size_t pos = filename.find_last_of('.');
    if(pos != std::string::npos)
    {
        std::string extension = filename.substr(pos + 1);
        transform(extension.begin(), extension.end(), extension.begin(), (int (*)(int))std::tolower);
        if(extension == "aac")
        {
            rep->streamFormat = StreamFormat(HLSStreamFormat::PACKEDAAC);
        }
        else if(extension == "ts" || extension == "mp2t" || extension == "mpeg")
        {
            rep->streamFormat = StreamFormat(HLSStreamFormat::MPEG2TS);
        }
    }
}

Representation * M3U8Parser::createRepresentation(BaseAdaptationSet *adaptSet, const AttributesTag * tag)
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
            rep->setID(uri);
            rep->setPlaylistUrl(uri);
            if(uri.find('/') != std::string::npos)
            {
                uri = Helper::getDirectoryPath(uri);
                if(!uri.empty())
                    rep->baseUrl.Set(new Url(uri.append("/")));
            }
        }

        if(bwAttr)
            rep->setBandwidth(bwAttr->decimal());

        if(codecsAttr)
            setFormatFromCodecs(rep, codecsAttr->quotedString());

        if(resAttr)
        {
            std::pair<int, int> res = resAttr->getResolution();
            if(res.first * res.second)
            {
                rep->setWidth(res.first);
                rep->setHeight(res.second);
            }
        }
    }

    return rep;
}

void M3U8Parser::createAndFillRepresentation(vlc_object_t *p_obj, BaseAdaptationSet *adaptSet,
                                             const AttributesTag *tag,
                                             const std::list<Tag *> &tagslist)
{
    Representation *rep  = createRepresentation(adaptSet, tag);
    if(rep)
    {
        parseSegments(p_obj, rep, tagslist);
        adaptSet->addRepresentation(rep);
    }
}

bool M3U8Parser::appendSegmentsFromPlaylistURI(vlc_object_t *p_obj, Representation *rep)
{
    void *p_data;
    const size_t i_data = Retrieve::HTTP(p_obj, rep->getPlaylistUrl().toString(), &p_data);
    if(p_data)
    {
        stream_t *substream = stream_MemoryNew(p_obj, (uint8_t *)p_data, i_data, false);
        if(substream)
        {
            std::list<Tag *> tagslist = parseEntries(substream);
            stream_Delete(substream);

            parseSegments(p_obj, rep, tagslist);

            releaseTagsList(tagslist);
            return true;
        }
    }
    return false;
}

void M3U8Parser::parseSegments(vlc_object_t *p_obj, Representation *rep, const std::list<Tag *> &tagslist)
{
    SegmentList *segmentList = new (std::nothrow) SegmentList(rep);

    rep->timescale.Set(100);
    rep->b_loaded = true;

    stime_t totalduration = 0;
    stime_t nzStartTime = 0;
    uint64_t sequenceNumber = 0;
    bool discontinuity = false;
    std::size_t prevbyterangeoffset = 0;
    const SingleValueTag *ctx_byterange = NULL;
    SegmentEncryption encryption;
    const ValuesListTag *ctx_extinf = NULL;

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

            case ValuesListTag::EXTINF:
            {
                ctx_extinf = static_cast<const ValuesListTag *>(tag);
            }
            break;

            case SingleValueTag::URI:
            {
                const SingleValueTag *uritag = static_cast<const SingleValueTag *>(tag);
                if(uritag->getValue().value.empty())
                {
                    ctx_extinf = NULL;
                    ctx_byterange = NULL;
                    break;
                }

                HLSSegment *segment = new (std::nothrow) HLSSegment(rep, sequenceNumber++);
                if(!segment)
                    break;

                segment->setSourceUrl(uritag->getValue().value);
                if((unsigned)rep->getStreamFormat() == HLSStreamFormat::UNKNOWN)
                    setFormatFromExtension(rep, uritag->getValue().value);

                if(ctx_extinf)
                {
                    if(ctx_extinf->getAttributeByName("DURATION"))
                    {
                        segment->duration.Set(ctx_extinf->getAttributeByName("DURATION")->floatingPoint() * rep->timescale.Get());
                        segment->startTime.Set(nzStartTime);
                        nzStartTime += segment->duration.Get();
                        totalduration += segment->duration.Get();
                    }
                    ctx_extinf = NULL;
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

                if(discontinuity)
                {
                    segment->discontinuity = true;
                    discontinuity = false;
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

                    Url keyurl(keytag->getAttributeByName("URI")->quotedString());
                    if(!keyurl.hasScheme())
                    {
                        keyurl.prepend(Helper::getDirectoryPath(rep->getPlaylistUrl().toString()).append("/"));
                    }

                    const uint64_t read = Retrieve::HTTP(p_obj, keyurl.toString(), (void **) &p_data);
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

            case Tag::EXTXDISCONTINUITY:
                discontinuity  = true;
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
    else if(totalduration * CLOCK_FREQ / rep->timescale.Get() > (uint64_t) rep->getPlaylist()->duration.Get())
    {
        rep->getPlaylist()->duration.Set(totalduration * CLOCK_FREQ / rep->timescale.Get());
    }

    rep->setSegmentList(segmentList);
}
M3U8 * M3U8Parser::parse(stream_t *p_stream, const std::string &playlisturl)
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
        playlist->setPlaylistUrl( Helper::getDirectoryPath(playlisturl).append("/") );

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
                        Representation *rep  = createRepresentation(adaptSet, tag);
                        if(rep)
                        {
                            adaptSet->addRepresentation(rep);
                        }
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
                Representation *rep  = createRepresentation(altAdaptSet, pair.second);
                if(rep)
                {
                    altAdaptSet->addRepresentation(rep);
                }

                if(pair.second->getAttributeByName("NAME"))
                   altAdaptSet->description.Set(pair.second->getAttributeByName("NAME")->quotedString());

                /* Subtitles unsupported for now */
                if(pair.second->getAttributeByName("TYPE")->value != "AUDIO" &&
                   pair.second->getAttributeByName("TYPE")->value != "VIDEO")
                {
                    rep->streamFormat = StreamFormat(HLSStreamFormat::UNSUPPORTED);
                }

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
    else /* Non master playlist (opened directly subplaylist or HLS v1) */
    {
        BaseAdaptationSet *adaptSet = new (std::nothrow) BaseAdaptationSet(period);
        if(adaptSet)
        {
            period->addAdaptationSet(adaptSet);
            AttributesTag *tag = new AttributesTag(AttributesTag::EXTXSTREAMINF, "");
            tag->addAttribute(new Attribute("URI", playlisturl));
            createAndFillRepresentation(VLC_OBJECT(p_stream), adaptSet, tag, tagslist);
            delete tag;
        }
    }

    playlist->addPeriod(period);

    releaseTagsList(tagslist);

    playlist->debug();
    return playlist;
}

std::list<Tag *> M3U8Parser::parseEntries(stream_t *stream)
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
        else if(*psz_line)
        {
            /* URI */
            if(lastTag && lastTag->getType() == AttributesTag::EXTXSTREAMINF)
            {
                AttributesTag *streaminftag = static_cast<AttributesTag *>(lastTag);
                /* master playlist uri, merge as attribute */
                Attribute *uriAttr = new (std::nothrow) Attribute("URI", std::string(psz_line));
                if(uriAttr)
                    streaminftag->addAttribute(uriAttr);
            }
            else /* playlist tag, will take modifiers */
            {
                Tag *tag = TagFactory::createTagByName("", std::string(psz_line));
                if(tag)
                    entrieslist.push_back(tag);
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
