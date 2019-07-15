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

#include "Manifest.hpp"
#include "Representation.hpp"
#include "ForgedInitSegment.hpp"
#include "SmoothSegment.hpp"
#include "../../adaptive/playlist/BasePeriod.h"
#include "../../adaptive/playlist/BaseAdaptationSet.h"
#include "../../adaptive/playlist/SegmentTimeline.h"
#include "../../adaptive/playlist/SegmentList.h"
#include "../../adaptive/xml/DOMHelper.h"
#include "../../adaptive/xml/Node.h"
#include "../../adaptive/tools/Helper.h"
#include "../../adaptive/tools/Conversions.hpp"

using namespace smooth::playlist;
using namespace adaptive::xml;

ManifestParser::ManifestParser(Node *root_, vlc_object_t *p_object_,
                               stream_t *stream, const std::string & streambaseurl_)
{
    root = root_;
    p_object = p_object_;
    p_stream = stream;
    playlisturl = streambaseurl_;
}

ManifestParser::~ManifestParser()
{
}

static SegmentTimeline *createTimeline(Node *streamIndexNode, uint64_t timescale)
{
    SegmentTimeline *timeline = new (std::nothrow) SegmentTimeline(timescale);
    if(timeline)
    {
        std::vector<Node *> chunks = DOMHelper::getElementByTagName(streamIndexNode, "c", true);
        std::vector<Node *>::const_iterator it;

        struct
        {
            uint64_t number;
            uint64_t duration;
            uint64_t time;
            uint64_t repeat;
        } cur = {0,0,0,0}, prev = {0,0,0,0};

        for(it = chunks.begin(); it != chunks.end(); ++it)
        {
            const Node *chunk = *it;
             /* Detect repeats, as r attribute only has been added in late smooth */
            bool b_cur_is_repeat = true; /* If our current chunk has repeated previous content */

            if(chunk->hasAttribute("n"))
            {
                cur.number = Integer<uint64_t>(chunk->getAttributeValue("n"));
                b_cur_is_repeat &= (cur.number == prev.number + 1 + prev.repeat);
            }
            else
            {
                cur.number = prev.number + prev.repeat + 1;
            }

            if(chunk->hasAttribute("d"))
            {
                cur.duration = Integer<uint64_t>(chunk->getAttributeValue("d"));
                b_cur_is_repeat &= (cur.duration == prev.duration);
            }
            else
            {
                if(it != chunks.end())
                {
                    const Node *nextchunk = *(it + 1);
                    cur.duration = Integer<uint64_t>(nextchunk->getAttributeValue("t"))
                                 - Integer<uint64_t>(chunk->getAttributeValue("t"));
                    b_cur_is_repeat &= (cur.duration == prev.duration);
                }
            }

            if(chunk->hasAttribute("t"))
            {
                cur.time = Integer<uint64_t>(chunk->getAttributeValue("t"));
                b_cur_is_repeat &= (cur.time == prev.time + (prev.duration * (prev.repeat + 1)));
            }
            else
            {
                cur.time = prev.time + (prev.duration * (prev.repeat + 1));
            }

            uint64_t explicit_repeat_count = 0;
            if(chunk->hasAttribute("r"))
            {
                explicit_repeat_count = Integer<uint64_t>(chunk->getAttributeValue("r"));
                /* #segments = repeat count ! as MS has a really broken notion of repetition */
                if(explicit_repeat_count > 0)
                    explicit_repeat_count -= 1;
            }

            if(it == chunks.begin())
            {
                prev = cur;
                prev.repeat = explicit_repeat_count;
            }
            else if(b_cur_is_repeat)
            {
                prev.repeat += (1 + explicit_repeat_count);
            }
            else
            {
                timeline->addElement(prev.number, prev.duration, prev.repeat, prev.time);
                cur.repeat = explicit_repeat_count;
                prev = cur;
                cur.repeat = cur.number = 0;
            }
        }

        if(chunks.size() > 0)
            timeline->addElement(prev.number, prev.duration, prev.repeat, prev.time);
    }
    return timeline;
}

static void ParseQualityLevel(BaseAdaptationSet *adaptSet, Node *qualNode, const std::string &type, unsigned id, unsigned trackid)
{
    Representation *rep = new (std::nothrow) Representation(adaptSet);
    if(rep)
    {
        rep->setID(ID(id));

        if(qualNode->hasAttribute("Bitrate"))
            rep->setBandwidth(Integer<uint64_t>(qualNode->getAttributeValue("Bitrate")));

        if(qualNode->hasAttribute("MaxWidth"))
            rep->setWidth(Integer<uint64_t>(qualNode->getAttributeValue("MaxWidth")));
        else if(qualNode->hasAttribute("Width"))
            rep->setWidth(Integer<uint64_t>(qualNode->getAttributeValue("Width")));

        if(qualNode->hasAttribute("MaxHeight"))
            rep->setHeight(Integer<uint64_t>(qualNode->getAttributeValue("MaxHeight")));
        else if(qualNode->hasAttribute("Height"))
            rep->setHeight(Integer<uint64_t>(qualNode->getAttributeValue("Height")));

        if(qualNode->hasAttribute("FourCC"))
            rep->addCodecs(qualNode->getAttributeValue("FourCC"));

        ForgedInitSegment *initSegment = new (std::nothrow)
                ForgedInitSegment(rep, type,
                                  adaptSet->inheritTimescale(),
                                  adaptSet->getPlaylist()->duration.Get());
        if(initSegment)
        {
            initSegment->setTrackID(trackid);

            if(!adaptSet->getLang().empty())
                initSegment->setLanguage(adaptSet->getLang());

            if(rep->getWidth() > 0 && rep->getHeight() > 0)
                initSegment->setVideoSize(rep->getWidth(), rep->getHeight());

            if(qualNode->hasAttribute("FourCC"))
                initSegment->setFourCC(qualNode->getAttributeValue("FourCC"));

            if(qualNode->hasAttribute("PacketSize"))
                initSegment->setPacketSize(Integer<uint16_t>(qualNode->getAttributeValue("PacketSize")));

            if(qualNode->hasAttribute("Channels"))
                initSegment->setChannels(Integer<uint16_t>(qualNode->getAttributeValue("Channels")));

            if(qualNode->hasAttribute("SamplingRate"))
                initSegment->setSamplingRate(Integer<uint32_t>(qualNode->getAttributeValue("SamplingRate")));

            if(qualNode->hasAttribute("BitsPerSample"))
                initSegment->setBitsPerSample(Integer<uint32_t>(qualNode->getAttributeValue("BitsPerSample")));

            if(qualNode->hasAttribute("CodecPrivateData"))
                initSegment->setCodecPrivateData(qualNode->getAttributeValue("CodecPrivateData"));

            if(qualNode->hasAttribute("AudioTag"))
                initSegment->setAudioTag(Integer<uint16_t>(qualNode->getAttributeValue("AudioTag")));

            if(qualNode->hasAttribute("WaveFormatEx"))
                initSegment->setWaveFormatEx(qualNode->getAttributeValue("WaveFormatEx"));

            initSegment->setSourceUrl("forged://");

            rep->initialisationSegment.Set(initSegment);

            adaptSet->addRepresentation(rep);
        }
    }
}

static void ParseStreamIndex(BasePeriod *period, Node *streamIndexNode, unsigned id)
{
    BaseAdaptationSet *adaptSet = new (std::nothrow) BaseAdaptationSet(period);
    if(adaptSet)
    {
        adaptSet->setID(ID(id));
        if(streamIndexNode->hasAttribute("Language"))
            adaptSet->setLang(streamIndexNode->getAttributeValue("Language"));

        if(streamIndexNode->hasAttribute("Name"))
            adaptSet->description.Set(streamIndexNode->getAttributeValue("Name"));

        if(streamIndexNode->hasAttribute("TimeScale"))
            adaptSet->setTimescale(Integer<uint64_t>(streamIndexNode->getAttributeValue("TimeScale")));

        const std::string url = streamIndexNode->getAttributeValue("Url");
        if(!url.empty())
        {
            /* SmoothSegment is a template holder */
            SmoothSegment *templ = new SmoothSegment(adaptSet);
            if(templ)
            {
                templ->setSourceUrl(url);
                SegmentTimeline *timeline = createTimeline(streamIndexNode, adaptSet->inheritTimescale());
                templ->setSegmentTimeline(timeline);
                adaptSet->setSegmentTemplate(templ);
            }

            unsigned nextid = 1;
            const std::string type = streamIndexNode->getAttributeValue("Type");
            std::vector<Node *> qualLevels = DOMHelper::getElementByTagName(streamIndexNode, "QualityLevel", true);
            std::vector<Node *>::const_iterator it;
            for(it = qualLevels.begin(); it != qualLevels.end(); ++it)
                ParseQualityLevel(adaptSet, *it, type, nextid++, id);
        }
        if(!adaptSet->getRepresentations().empty())
            period->addAdaptationSet(adaptSet);
        else
            delete adaptSet;
    }
}

Manifest * ManifestParser::parse()
{
    Manifest *manifest = new (std::nothrow) Manifest(p_object);
    if(!manifest)
        return NULL;

    manifest->setPlaylistUrl(Helper::getDirectoryPath(playlisturl).append("/"));

    if(root->hasAttribute("TimeScale"))
        manifest->setTimescale(Integer<uint64_t>(root->getAttributeValue("TimeScale")));

    if(root->hasAttribute("Duration"))
    {
        stime_t time = Integer<stime_t>(root->getAttributeValue("Duration"));
        manifest->duration.Set(manifest->getTimescale().ToTime(time));
    }

    if(root->hasAttribute("DVRWindowLength"))
    {
        stime_t time = Integer<stime_t>(root->getAttributeValue("DVRWindowLength"));
        manifest->timeShiftBufferDepth.Set(manifest->getTimescale().ToTime(time));
    }

    if(root->hasAttribute("IsLive") && root->getAttributeValue("IsLive") == "TRUE")
        manifest->b_live = true;

    /* Need a default Period */
    BasePeriod *period = new (std::nothrow) BasePeriod(manifest);
    if(period)
    {
        period->setTimescale(manifest->getTimescale());
        period->duration.Set(manifest->duration.Get());
        unsigned nextid = 1;
        std::vector<Node *> streamIndexes = DOMHelper::getElementByTagName(root, "StreamIndex", true);
        std::vector<Node *>::const_iterator it;
        for(it = streamIndexes.begin(); it != streamIndexes.end(); ++it)
            ParseStreamIndex(period, *it, nextid++);
        manifest->addPeriod(period);
    }

    return manifest;
}
