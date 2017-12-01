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
#include "../adaptive/playlist/BasePeriod.h"
#include "../adaptive/playlist/BaseAdaptationSet.h"
#include "../adaptive/playlist/SegmentList.h"
#include "../adaptive/tools/Retrieve.hpp"
#include "../adaptive/tools/Helper.h"
#include "../adaptive/tools/Conversions.hpp"
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

M3U8Parser::M3U8Parser( AuthStorage *auth_ )
{
    auth = auth_;
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

void M3U8Parser::setFormatFromExtension(Representation *rep, const std::string &filename)
{
    std::size_t pos = filename.find_last_of('.');
    if(pos != std::string::npos)
    {
        std::string extension = Helper::getFileExtension(filename);
        transform(extension.begin(), extension.end(), extension.begin(), (int (*)(int))std::tolower);
        if(extension == "aac")
        {
            rep->streamFormat = StreamFormat(StreamFormat::PACKEDAAC);
        }
        else if(extension == "ts" || extension == "mp2t" || extension == "mpeg" || extension == "m2ts")
        {
            rep->streamFormat = StreamFormat(StreamFormat::MPEG2TS);
        }
        else if(extension == "mp4" || extension == "m4s" || extension == "mov" || extension == "m4v")
        {
            rep->streamFormat = StreamFormat(StreamFormat::MP4);
        }
        else if(extension == "vtt" || extension == "wvtt" || extension == "webvtt")
        {
            rep->streamFormat = StreamFormat(StreamFormat::WEBVTT);
        }
        else
        {
            rep->streamFormat = StreamFormat(StreamFormat::UNSUPPORTED);
        }
    }
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
        if(rep->isLive())
        {
            /* avoid update playlist immediately */
            uint64_t startseq = rep->getLiveStartSegmentNumber(0);
            rep->scheduleNextUpdate(startseq);
        }
        adaptSet->addRepresentation(rep);
    }
}

bool M3U8Parser::appendSegmentsFromPlaylistURI(vlc_object_t *p_obj, Representation *rep)
{
    block_t *p_block = Retrieve::HTTP(p_obj, auth, rep->getPlaylistUrl().toString());
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

void M3U8Parser::parseSegments(vlc_object_t *, Representation *rep, const std::list<Tag *> &tagslist)
{
    SegmentList *segmentList = new (std::nothrow) SegmentList(rep);

    rep->setTimescale(100);
    rep->b_loaded = true;

    mtime_t totalduration = 0;
    mtime_t nzStartTime = 0;
    mtime_t absReferenceTime = VLC_TS_INVALID;
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
                if((unsigned)rep->getStreamFormat() == StreamFormat::UNKNOWN)
                    setFormatFromExtension(rep, uritag->getValue().value);

                /* Need to use EXTXTARGETDURATION as default as some can't properly set segment one */
                double duration = rep->targetDuration;
                if(ctx_extinf)
                {
                    const Attribute *durAttribute = ctx_extinf->getAttributeByName("DURATION");
                    if(durAttribute)
                        duration = durAttribute->floatingPoint();
                    ctx_extinf = NULL;
                }
                const mtime_t nzDuration = CLOCK_FREQ * duration;
                segment->duration.Set(duration * (uint64_t) rep->getTimescale());
                segment->startTime.Set(rep->getTimescale().ToScaled(nzStartTime));
                nzStartTime += nzDuration;
                totalduration += nzDuration;
                if(absReferenceTime > VLC_TS_INVALID)
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

                if(encryption.method != SegmentEncryption::NONE)
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
                absReferenceTime = VLC_TS_0 +
                        UTCTime(static_cast<const SingleValueTag *>(tag)->getValue().value).mtime();
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

                    Url keyurl(keytag->getAttributeByName("URI")->quotedString());
                    if(!keyurl.hasScheme())
                    {
                        keyurl.prepend(Helper::getDirectoryPath(rep->getPlaylistUrl().toString()).append("/"));
                    }

                    M3U8 *m3u8 = dynamic_cast<M3U8 *>(rep->getPlaylist());
                    if(likely(m3u8))
                        encryption.key = m3u8->getEncryptionKey(keyurl.toString());
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

    rep->appendSegmentList(segmentList, true);
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

    M3U8 *playlist = new (std::nothrow) M3U8(p_object, auth);
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
        unsigned set_id = 1;
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

                std::string desc;
                if(pair.second->getAttributeByName("GROUP-ID"))
                    desc = pair.second->getAttributeByName("GROUP-ID")->quotedString();
                if(pair.second->getAttributeByName("NAME"))
                {
                    if(!desc.empty())
                        desc += " ";
                    desc += pair.second->getAttributeByName("NAME")->quotedString();
                }

                if(!desc.empty())
                {
                    altAdaptSet->description.Set(desc);
                    altAdaptSet->setID(ID(desc));
                }
                else altAdaptSet->setID(ID(set_id++));

                /* Subtitles unsupported for now */
                if(pair.second->getAttributeByName("TYPE")->value != "AUDIO" &&
                   pair.second->getAttributeByName("TYPE")->value != "VIDEO" &&
                   pair.second->getAttributeByName("TYPE")->value != "SUBTITLES" )
                {
                    rep->streamFormat = StreamFormat(StreamFormat::UNSUPPORTED);
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
            createAndFillRepresentation(p_object, adaptSet, tag, tagslist);
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
