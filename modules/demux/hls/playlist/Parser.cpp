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
#include "../../adaptive/SharedResources.hpp"
#include "../../adaptive/playlist/BasePeriod.h"
#include "../../adaptive/playlist/BaseAdaptationSet.h"
#include "../../adaptive/playlist/SegmentList.h"
#include "../../adaptive/encryption/Keyring.hpp"
#include "../../adaptive/tools/Retrieve.hpp"
#include "../../adaptive/tools/Helper.h"
#include "../../adaptive/tools/Conversions.hpp"
#include "M3U8.hpp"
#include "Tags.hpp"

#include <vlc_strings.h>
#include <vlc_stream.h>
#include <cstdio>
#include <sstream>
#include <map>
#include <cctype>
#include <algorithm>

using namespace adaptive;
using namespace adaptive::playlist;
using namespace hls::playlist;

M3U8Parser::M3U8Parser(SharedResources *res)
{
    resources = res;
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

static Tag * getTagFromList(std::list<Tag *> &list, int tag)
{
    std::list<Tag *>::const_iterator it;
    for(it = list.begin(); it != list.end(); ++it)
    {
        if( (*it)->getType() == tag )
            return *it;
    }
    return NULL;
}

static void releaseTagsList(std::list<Tag *> &list)
{
    std::list<Tag *>::const_iterator it;
    for(it = list.begin(); it != list.end(); ++it)
        delete *it;
    list.clear();
}

Representation * M3U8Parser::createRepresentation(BaseAdaptationSet *adaptSet, const AttributesTag * tag)
{
    const Attribute *uriAttr = tag->getAttributeByName("URI");
    const Attribute *bwAttr = tag->getAttributeByName("BANDWIDTH");
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

        if(tag->getAttributeByName("CODECS"))
            rep->addCodecs(tag->getAttributeByName("CODECS")->quotedString());

        if(resAttr)
        {
            std::pair<int, int> res = resAttr->getResolution();
            if(res.first && res.second)
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
    block_t *p_block = Retrieve::HTTP(resources, rep->getPlaylistUrl().toString());
    if(p_block)
    {
        stream_t *substream = vlc_stream_MemoryNew(p_obj, p_block->p_buffer, p_block->i_buffer, true);
        if(substream)
        {
            std::list<Tag *> tagslist = parseEntries(substream);
            vlc_stream_Delete(substream);

            parseSegments(p_obj, rep, tagslist);

            releaseTagsList(tagslist);
        }
        block_Release(p_block);
        return true;
    }
    return false;
}

static bool parseEncryption(const AttributesTag *keytag, const Url &playlistUrl,
                            CommonEncryption &encryption)
{
    if( keytag->getAttributeByName("METHOD") &&
        keytag->getAttributeByName("METHOD")->value == "AES-128" &&
        keytag->getAttributeByName("URI") )
    {
        encryption.method = CommonEncryption::Method::AES_128;
        encryption.uri.clear();

        Url keyurl(keytag->getAttributeByName("URI")->quotedString());
        if(!keyurl.hasScheme())
        {
            keyurl.prepend(Helper::getDirectoryPath(playlistUrl.toString()).append("/"));
        }

        encryption.uri = keyurl.toString();

        if(keytag->getAttributeByName("IV"))
        {
            encryption.iv.clear();
            encryption.iv = keytag->getAttributeByName("IV")->hexSequence();
        }
        return true;
    }
    else
    {
        /* unsupported or invalid */
        encryption.method = CommonEncryption::Method::NONE;
        encryption.uri.clear();
        encryption.iv.clear();
        return false;
    }
}

void M3U8Parser::parseSegments(vlc_object_t *, Representation *rep, const std::list<Tag *> &tagslist)
{
    SegmentList *segmentList = new (std::nothrow) SegmentList(rep);

    rep->setTimescale(100);
    rep->b_loaded = true;

    vlc_tick_t totalduration = 0;
    vlc_tick_t nzStartTime = 0;
    vlc_tick_t absReferenceTime = VLC_TICK_INVALID;
    uint64_t sequenceNumber = 0;
    bool discontinuity = false;
    std::size_t prevbyterangeoffset = 0;
    const SingleValueTag *ctx_byterange = NULL;
    CommonEncryption encryption;
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

                /* Need to use EXTXTARGETDURATION as default as some can't properly set segment one */
                vlc_tick_t nzDuration = vlc_tick_from_sec(rep->targetDuration);
                if(ctx_extinf)
                {
                    const Attribute *durAttribute = ctx_extinf->getAttributeByName("DURATION");
                    if(durAttribute)
                        nzDuration = vlc_tick_from_sec(durAttribute->floatingPoint());
                    ctx_extinf = NULL;
                }
                segment->duration.Set(rep->getTimescale().ToScaled(nzDuration));
                segment->startTime.Set(rep->getTimescale().ToScaled(nzStartTime));
                nzStartTime += nzDuration;
                totalduration += nzDuration;
                if(absReferenceTime != VLC_TICK_INVALID)
                {
                    segment->utcTime = absReferenceTime;
                    absReferenceTime += nzDuration;
                }

                segmentList->addSegment(segment);

                if(ctx_byterange)
                {
                    std::pair<std::size_t,std::size_t> range = ctx_byterange->getValue().getByteRange();
                    if(range.first == 0) /* first == size, second = offset */
                        range.first = prevbyterangeoffset;
                    prevbyterangeoffset = range.first + range.second;
                    segment->setByteRange(range.first, prevbyterangeoffset - 1);
                    ctx_byterange = NULL;
                }

                if(discontinuity)
                {
                    segment->discontinuity = true;
                    discontinuity = false;
                }

                if(encryption.method != CommonEncryption::Method::NONE)
                    segment->setEncryption(encryption);
            }
            break;

            case SingleValueTag::EXTXTARGETDURATION:
                rep->targetDuration = static_cast<const SingleValueTag *>(tag)->getValue().decimal();
                break;

            case SingleValueTag::EXTXPLAYLISTTYPE:
                rep->b_live = (static_cast<const SingleValueTag *>(tag)->getValue().value != "VOD");
                break;

            case SingleValueTag::EXTXBYTERANGE:
                ctx_byterange = static_cast<const SingleValueTag *>(tag);
                break;

            case SingleValueTag::EXTXPROGRAMDATETIME:
                rep->b_consistent = false;
                absReferenceTime = VLC_TICK_0 +
                        UTCTime(static_cast<const SingleValueTag *>(tag)->getValue().value).mtime();
                break;

            case AttributesTag::EXTXKEY:
                parseEncryption(static_cast<const AttributesTag *>(tag),
                                rep->getPlaylistUrl(), encryption);
            break;

            case AttributesTag::EXTXMAP:
            {
                const AttributesTag *keytag = static_cast<const AttributesTag *>(tag);
                const Attribute *uriAttr;
                if(keytag && (uriAttr = keytag->getAttributeByName("URI")) &&
                   !segmentList->initialisationSegment.Get()) /* FIXME: handle discontinuities */
                {
                    InitSegment *initSegment = new (std::nothrow) InitSegment(rep);
                    if(initSegment)
                    {
                        initSegment->setSourceUrl(uriAttr->quotedString());
                        const Attribute *byterangeAttr = keytag->getAttributeByName("BYTERANGE");
                        if(byterangeAttr)
                        {
                            const std::pair<std::size_t,std::size_t> range = byterangeAttr->unescapeQuotes().getByteRange();
                            initSegment->setByteRange(range.first, range.first + range.second - 1);
                        }
                        segmentList->initialisationSegment.Set(initSegment);
                    }
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
    else if(totalduration > rep->getPlaylist()->duration.Get())
    {
        rep->getPlaylist()->duration.Set(totalduration);
    }

    rep->updateSegmentList(segmentList, true);
}
M3U8 * M3U8Parser::parse(vlc_object_t *p_object, stream_t *p_stream, const std::string &playlisturl)
{
    char *psz_line = vlc_stream_ReadLine(p_stream);
    if(!psz_line || strncmp(psz_line, "#EXTM3U", 7) ||
       (psz_line[7] && !std::isspace(psz_line[7])))
    {
        free(psz_line);
        return NULL;
    }
    free(psz_line);

    M3U8 *playlist = new (std::nothrow) M3U8(p_object);
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

        /* Preload Session Key */
        Tag *sessionKey = getTagFromList(tagslist, AttributesTag::EXTXSESSIONKEY);
        if(sessionKey)
        {
            CommonEncryption sessionEncryption;
            if(parseEncryption(static_cast<const AttributesTag *>(sessionKey),
                                playlist->getUrlSegment(), sessionEncryption) &&
               !sessionEncryption.uri.empty())
            {
                resources->getKeyring()->getKey(resources, sessionEncryption.uri);
            }
        }

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
            /* adaptSet->setSegmentAligned(true); FIXME: based on streamformat */
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

                if(adaptSet->description.Get().empty() &&
                   tag && tag->getAttributeByName("NAME"))
                {
                    adaptSet->description.Set(tag->getAttributeByName("NAME")->quotedString());
                }
            }
            if(!adaptSet->getRepresentations().empty())
                period->addAdaptationSet(adaptSet);
            else
                delete adaptSet;
        }

        /* Finally add all groups */
        unsigned set_id = 1;
        std::map<std::string, AttributesTag *>::const_iterator groupsit;
        for(groupsit = groupsmap.begin(); groupsit != groupsmap.end(); ++groupsit)
        {
            std::pair<std::string, AttributesTag *> pair = *groupsit;
            if(!pair.second->getAttributeByName("TYPE"))
                continue;

            BaseAdaptationSet *altAdaptSet = new (std::nothrow) BaseAdaptationSet(period);
            if(altAdaptSet)
            {
                Representation *rep  = createRepresentation(altAdaptSet, pair.second);
                if(rep)
                {
                    altAdaptSet->addRepresentation(rep);
                }

                std::string desc;
                if(pair.second->getAttributeByName("GROUP-ID"))
                    desc = pair.second->getAttributeByName("GROUP-ID")->quotedString();
                if(pair.second->getAttributeByName("NAME"))
                {
                    if(!desc.empty())
                        desc += " ";
                    desc += pair.second->getAttributeByName("NAME")->quotedString();
                }

                if(pair.second->getAttributeByName("CODECS"))
                {
                    rep->addCodecs(pair.second->getAttributeByName("CODECS")->quotedString());
                }
                else
                {
                    if(pair.second->getAttributeByName("TYPE")->value == "AUDIO")
                        rep->addCodecs("mp4a");
                    else if(pair.second->getAttributeByName("TYPE")->value == "VIDEO")
                        rep->addCodecs("avc1");
                    else if(pair.second->getAttributeByName("TYPE")->value == "SUBTITLES")
                        rep->addCodecs("wvtt");
                }

                if(!desc.empty())
                {
                    altAdaptSet->description.Set(desc);
                    altAdaptSet->setID(ID(desc));
                }
                else altAdaptSet->setID(ID(set_id++));

                if(pair.second->getAttributeByName("DEFAULT"))
                {
                    if(pair.second->getAttributeByName("DEFAULT")->value == "YES")
                        altAdaptSet->setRole(Role(Role::ROLE_MAIN));
                    else
                        altAdaptSet->setRole(Role(Role::ROLE_ALTERNATE));
                }

                if(pair.second->getAttributeByName("AUTOSELECT"))
                {
                    if(pair.second->getAttributeByName("AUTOSELECT")->value == "NO" &&
                       !pair.second->getAttributeByName("DEFAULT"))
                        altAdaptSet->setRole(Role(Role::ROLE_SUPPLEMENTARY));
                }

                /* Subtitles unsupported for now */
                const Attribute *typeattr = pair.second->getAttributeByName("TYPE");
                if(typeattr->value == "SUBTITLES")
                {
                    altAdaptSet->setRole(Role(Role::ROLE_SUBTITLE));
                    rep->streamFormat = StreamFormat(StreamFormat::UNSUPPORTED);
                }
                else if(typeattr->value != "AUDIO" && typeattr->value != "VIDEO")
                {
                    rep->streamFormat = StreamFormat(StreamFormat::UNSUPPORTED);
                }

                if(pair.second->getAttributeByName("LANGUAGE"))
                    altAdaptSet->setLang(pair.second->getAttributeByName("LANGUAGE")->quotedString());

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
            AttributesTag *tag = new AttributesTag(AttributesTag::EXTXSTREAMINF, "");
            tag->addAttribute(new Attribute("URI", playlisturl));
            createAndFillRepresentation(p_object, adaptSet, tag, tagslist);
            if(!adaptSet->getRepresentations().empty())
            {
                adaptSet->getRepresentations().front()->
                    scheduleNextUpdate(std::numeric_limits<uint64_t>::max(), true);
                period->addAdaptationSet(adaptSet);
            }
            else
                delete adaptSet;
            delete tag;
        }
    }

    playlist->addPeriod(period);

    auto xstart = std::find_if(tagslist.cbegin(), tagslist.cend(),
                               [](auto t) {return t->getType() == AttributesTag::EXTXSTART;});
    if(xstart != tagslist.end())
    {
        auto xstartTag = static_cast<const AttributesTag *>(*xstart);
        if(xstartTag->getAttributeByName("TIME-OFFSET"))
        {
            float offset = xstartTag->getAttributeByName("TIME-OFFSET")->floatingPoint();
            if(offset > 0)
                playlist->suggestedPresentationDelay.Set(CLOCK_FREQ * offset);
        }
    }

    releaseTagsList(tagslist);

    playlist->debug();
    return playlist;
}

std::list<Tag *> M3U8Parser::parseEntries(stream_t *stream)
{
    std::list<Tag *> entrieslist;
    Tag *lastTag = NULL;
    char *psz_line;

    while((psz_line = vlc_stream_ReadLine(stream)))
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
