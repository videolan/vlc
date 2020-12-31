/*
 * IsoffMainParser.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2012
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#include "IsoffMainParser.h"
#include "../../adaptive/playlist/SegmentTemplate.h"
#include "../../adaptive/playlist/Segment.h"
#include "../../adaptive/playlist/SegmentBase.h"
#include "../../adaptive/playlist/SegmentList.h"
#include "../../adaptive/playlist/SegmentTimeline.h"
#include "../../adaptive/playlist/SegmentInformation.hpp"
#include "../../adaptive/playlist/BasePeriod.h"
#include "MPD.h"
#include "Representation.h"
#include "AdaptationSet.h"
#include "ProgramInformation.h"
#include "DASHSegment.h"
#include "../../adaptive/xml/DOMHelper.h"
#include "../../adaptive/tools/Helper.h"
#include "../../adaptive/tools/Debug.hpp"
#include "../../adaptive/tools/Conversions.hpp"
#include <vlc_stream.h>
#include <cstdio>
#include <limits>

using namespace dash::mpd;
using namespace adaptive::xml;
using namespace adaptive::playlist;

IsoffMainParser::IsoffMainParser    (Node *root_, vlc_object_t *p_object_,
                                     stream_t *stream, const std::string & streambaseurl_)
{
    root = root_;
    p_stream = stream;
    p_object = p_object_;
    playlisturl = streambaseurl_;
}

IsoffMainParser::~IsoffMainParser   ()
{
}

template <class T>
static void parseAvailability(MPD *mpd, Node *node, T *s)
{
    if(node->hasAttribute("availabilityTimeOffset"))
    {
        double val = Integer<double>(node->getAttributeValue("availabilityTimeOffset"));
        s->addAttribute(new AvailabilityTimeOffsetAttr(val * CLOCK_FREQ));
    }
    if(node->hasAttribute("availabilityTimeComplete"))
    {
        bool b = (node->getAttributeValue("availabilityTimeComplete") == "false");
        s->addAttribute(new AvailabilityTimeCompleteAttr(!b));
        if(b)
            mpd->setLowLatency(b);
    }
}

void IsoffMainParser::parseMPDBaseUrl(MPD *mpd, Node *root)
{
    std::vector<Node *> baseUrls = DOMHelper::getChildElementByTagName(root, "BaseURL");

    for(size_t i = 0; i < baseUrls.size(); i++)
        mpd->addBaseUrl(baseUrls.at(i)->getText());

    mpd->setPlaylistUrl( Helper::getDirectoryPath(playlisturl).append("/") );
}

MPD * IsoffMainParser::parse()
{
    MPD *mpd = new (std::nothrow) MPD(p_object, getProfile());
    if(mpd)
    {
        parseMPDAttributes(mpd, root);
        parseProgramInformation(DOMHelper::getFirstChildElementByName(root, "ProgramInformation"), mpd);
        parseMPDBaseUrl(mpd, root);
        parsePeriods(mpd, root);
        mpd->debug();
    }
    return mpd;
}

void    IsoffMainParser::parseMPDAttributes   (MPD *mpd, xml::Node *node)
{
    const std::map<std::string, std::string> & attr = node->getAttributes();

    std::map<std::string, std::string>::const_iterator it;

    it = attr.find("mediaPresentationDuration");
    if(it != attr.end())
        mpd->duration.Set(IsoTime(it->second));

    it = attr.find("minBufferTime");
    if(it != attr.end())
        mpd->setMinBuffering(IsoTime(it->second));

    it = attr.find("minimumUpdatePeriod");
    if(it != attr.end())
    {
        mpd->b_needsUpdates = true;
        vlc_tick_t minupdate = IsoTime(it->second);
        if(minupdate > 0)
            mpd->minUpdatePeriod.Set(minupdate);
    }
    else mpd->b_needsUpdates = false;

    it = attr.find("maxSegmentDuration");
    if(it != attr.end())
        mpd->maxSegmentDuration.Set(IsoTime(it->second));

    it = attr.find("type");
    if(it != attr.end())
        mpd->setType(it->second);

    it = attr.find("availabilityStartTime");
    if(it != attr.end())
        mpd->availabilityStartTime.Set(UTCTime(it->second).mtime());

    it = attr.find("availabilityEndTime");
    if(it != attr.end())
        mpd->availabilityEndTime.Set(UTCTime(it->second).mtime());

    it = attr.find("timeShiftBufferDepth");
        if(it != attr.end())
            mpd->timeShiftBufferDepth.Set(IsoTime(it->second));

    it = attr.find("suggestedPresentationDelay");
    if(it != attr.end())
        mpd->suggestedPresentationDelay.Set(IsoTime(it->second));
}

void IsoffMainParser::parsePeriods(MPD *mpd, Node *root)
{
    std::vector<Node *> periods = DOMHelper::getElementByTagName(root, "Period", false);
    std::vector<Node *>::const_iterator it;
    uint64_t nextid = 0;

    for(it = periods.begin(); it != periods.end(); ++it)
    {
        BasePeriod *period = new (std::nothrow) BasePeriod(mpd);
        if (!period)
            continue;
        parseSegmentInformation(mpd, *it, period, &nextid);
        if((*it)->hasAttribute("start"))
            period->startTime.Set(IsoTime((*it)->getAttributeValue("start")));
        if((*it)->hasAttribute("duration"))
            period->duration.Set(IsoTime((*it)->getAttributeValue("duration")));
        std::vector<Node *> baseUrls = DOMHelper::getChildElementByTagName(*it, "BaseURL");
        if(!baseUrls.empty())
        {
            period->baseUrl.Set( new Url( baseUrls.front()->getText() ) );
            parseAvailability<BasePeriod>(mpd, baseUrls.front(), period);
        }

        parseAdaptationSets(mpd, *it, period);
        mpd->addPeriod(period);
    }
}

void IsoffMainParser::parseSegmentBaseType(MPD *, Node *node,
                                           AbstractSegmentBaseType *base,
                                           SegmentInformation *parent)
{
    parseInitSegment(DOMHelper::getFirstChildElementByName(node, "Initialization"), base, parent);

    if(node->hasAttribute("indexRange"))
    {
        size_t start = 0, end = 0;
        if (std::sscanf(node->getAttributeValue("indexRange").c_str(), "%zu-%zu", &start, &end) == 2)
        {
            IndexSegment *index = new (std::nothrow) DashIndexSegment(parent);
            if(index)
            {
                index->setByteRange(start, end);
                base->indexSegment.Set(index);
                /* index must be before data, so data starts at index end */
                if(dynamic_cast<SegmentBase *>(base))
                    dynamic_cast<SegmentBase *>(base)->setByteRange(end + 1, 0);
            }
        }
    }

    if(node->hasAttribute("timescale"))
    {
        TimescaleAttr *prop = new TimescaleAttr(Timescale(Integer<uint64_t>(node->getAttributeValue("timescale"))));
        base->addAttribute(prop);
    }
}

void IsoffMainParser::parseMultipleSegmentBaseType(MPD *mpd, Node *node,
                                                   AbstractMultipleSegmentBaseType *base,
                                                   SegmentInformation *parent)
{
    parseSegmentBaseType(mpd, node, base, parent);

    if(node->hasAttribute("duration"))
        base->addAttribute(new DurationAttr(Integer<stime_t>(node->getAttributeValue("duration"))));

    if(node->hasAttribute("startNumber"))
        base->addAttribute(new StartnumberAttr(Integer<uint64_t>(node->getAttributeValue("startNumber"))));

    parseTimeline(DOMHelper::getFirstChildElementByName(node, "SegmentTimeline"), base);
}

size_t IsoffMainParser::parseSegmentTemplate(MPD *mpd, Node *templateNode, SegmentInformation *info)
{
    size_t total = 0;
    if (templateNode == NULL)
        return total;

    std::string mediaurl;
    if(templateNode->hasAttribute("media"))
        mediaurl = templateNode->getAttributeValue("media");

    SegmentTemplate *mediaTemplate = new (std::nothrow) SegmentTemplate(info);
    if(!mediaTemplate)
        return total;
    mediaTemplate->setSourceUrl(mediaurl);

    parseMultipleSegmentBaseType(mpd, templateNode, mediaTemplate, info);

    parseAvailability<SegmentInformation>(mpd, templateNode, info);

    if(templateNode->hasAttribute("initialization")) /* /!\ != Initialization */
    {
        SegmentTemplateInit *initTemplate;
        std::string initurl = templateNode->getAttributeValue("initialization");
        if(!initurl.empty() && (initTemplate = new (std::nothrow) SegmentTemplateInit(mediaTemplate, info)))
        {
            initTemplate->setSourceUrl(initurl);
            delete mediaTemplate->initialisationSegment.Get();
            mediaTemplate->initialisationSegment.Set(initTemplate);
        }
    }

    info->setSegmentTemplate(mediaTemplate);

    return mediaurl.empty() ? ++total : 0;
}

size_t IsoffMainParser::parseSegmentInformation(MPD *mpd, Node *node,
                                                SegmentInformation *info, uint64_t *nextid)
{
    size_t total = 0;
    total += parseSegmentBase(mpd, DOMHelper::getFirstChildElementByName(node, "SegmentBase"), info);
    total += parseSegmentList(mpd, DOMHelper::getFirstChildElementByName(node, "SegmentList"), info);
    total += parseSegmentTemplate(mpd, DOMHelper::getFirstChildElementByName(node, "SegmentTemplate" ), info);
    if(node->hasAttribute("timescale"))
        info->addAttribute(new TimescaleAttr(Timescale(Integer<uint64_t>(node->getAttributeValue("timescale")))));

    parseAvailability<SegmentInformation>(mpd, node, info);

    if(node->hasAttribute("id"))
        info->setID(ID(node->getAttributeValue("id")));
    else
        info->setID(ID((*nextid)++));

    return total;
}

void    IsoffMainParser::parseAdaptationSets  (MPD *mpd, Node *periodNode, BasePeriod *period)
{
    std::vector<Node *> adaptationSets = DOMHelper::getElementByTagName(periodNode, "AdaptationSet", false);
    std::vector<Node *>::const_iterator it;
    uint64_t nextid = 0;

    for(it = adaptationSets.begin(); it != adaptationSets.end(); ++it)
    {
        AdaptationSet *adaptationSet = new AdaptationSet(period);
        if(!adaptationSet)
            continue;
        if((*it)->hasAttribute("mimeType"))
            adaptationSet->setMimeType((*it)->getAttributeValue("mimeType"));

        if((*it)->hasAttribute("lang"))
            adaptationSet->setLang((*it)->getAttributeValue("lang"));

        if((*it)->hasAttribute("bitstreamSwitching"))
            adaptationSet->setBitswitchAble((*it)->getAttributeValue("bitstreamSwitching") == "true");

        if((*it)->hasAttribute("segmentAlignment"))
            adaptationSet->setSegmentAligned((*it)->getAttributeValue("segmentAlignment") == "true");

        Node *baseUrl = DOMHelper::getFirstChildElementByName((*it), "BaseURL");
        if(baseUrl)
        {
            parseAvailability<AdaptationSet>(mpd, baseUrl, adaptationSet);
            adaptationSet->baseUrl.Set(new Url(baseUrl->getText()));
        }

        Node *role = DOMHelper::getFirstChildElementByName((*it), "Role");
        if(role && role->hasAttribute("schemeIdUri") && role->hasAttribute("value"))
        {
            std::string uri = role->getAttributeValue("schemeIdUri");
            if(uri == "urn:mpeg:dash:role:2011")
            {
                const std::string &rolevalue = role->getAttributeValue("value");
                adaptationSet->description.Set(rolevalue);
                if(rolevalue == "main")
                    adaptationSet->setRole(Role::ROLE_MAIN);
                else if(rolevalue == "alternate")
                    adaptationSet->setRole(Role::ROLE_ALTERNATE);
                else if(rolevalue == "supplementary")
                    adaptationSet->setRole(Role::ROLE_SUPPLEMENTARY);
                else if(rolevalue == "commentary")
                    adaptationSet->setRole(Role::ROLE_COMMENTARY);
                else if(rolevalue == "dub")
                    adaptationSet->setRole(Role::ROLE_DUB);
                else if(rolevalue == "caption")
                    adaptationSet->setRole(Role::ROLE_CAPTION);
                else if(rolevalue == "subtitle")
                    adaptationSet->setRole(Role::ROLE_SUBTITLE);
            }
        }

        parseSegmentInformation(mpd, *it, adaptationSet, &nextid);

        parseRepresentations(mpd, (*it), adaptationSet);

#ifdef ADAPTATIVE_ADVANCED_DEBUG
        if(adaptationSet->description.Get().empty())
            adaptationSet->description.Set(adaptationSet->getID().str());
#endif

        if(!adaptationSet->getRepresentations().empty())
            period->addAdaptationSet(adaptationSet);
        else
            delete adaptationSet;
    }
}
void    IsoffMainParser::parseRepresentations (MPD *mpd, Node *adaptationSetNode, AdaptationSet *adaptationSet)
{
    std::vector<Node *> representations = DOMHelper::getElementByTagName(adaptationSetNode, "Representation", false);
    uint64_t nextid = 0;

    for(size_t i = 0; i < representations.size(); i++)
    {
        Representation *currentRepresentation = new Representation(adaptationSet);
        Node *repNode = representations.at(i);

        std::vector<Node *> baseUrls = DOMHelper::getChildElementByTagName(repNode, "BaseURL");
        if(!baseUrls.empty())
        {
            currentRepresentation->baseUrl.Set(new Url(baseUrls.front()->getText()));
            parseAvailability<Representation>(mpd, baseUrls.front(), currentRepresentation);
        }

        if(repNode->hasAttribute("id"))
            currentRepresentation->setID(ID(repNode->getAttributeValue("id")));

        if(repNode->hasAttribute("width"))
            currentRepresentation->setWidth(atoi(repNode->getAttributeValue("width").c_str()));

        if(repNode->hasAttribute("height"))
            currentRepresentation->setHeight(atoi(repNode->getAttributeValue("height").c_str()));

        if(repNode->hasAttribute("bandwidth"))
            currentRepresentation->setBandwidth(atoi(repNode->getAttributeValue("bandwidth").c_str()));

        if(repNode->hasAttribute("mimeType"))
            currentRepresentation->setMimeType(repNode->getAttributeValue("mimeType"));

        if(repNode->hasAttribute("codecs"))
            currentRepresentation->addCodecs(repNode->getAttributeValue("codecs"));

        size_t i_total = parseSegmentInformation(mpd, repNode, currentRepresentation, &nextid);
        /* Empty Representation with just baseurl (ex: subtitles) */
        if(i_total == 0 &&
           (currentRepresentation->baseUrl.Get() && !currentRepresentation->baseUrl.Get()->empty()) &&
            adaptationSet->getMediaSegment(0) == NULL)
        {
            SegmentBase *base = new (std::nothrow) SegmentBase(currentRepresentation);
            if(base)
                currentRepresentation->addAttribute(base);
        }

        adaptationSet->addRepresentation(currentRepresentation);
    }
}
size_t IsoffMainParser::parseSegmentBase(MPD *mpd, Node * segmentBaseNode, SegmentInformation *info)
{
    SegmentBase *base;

    if(!segmentBaseNode || !(base = new (std::nothrow) SegmentBase(info)))
        return 0;

    parseSegmentBaseType(mpd, segmentBaseNode, base, info);

    parseAvailability<SegmentInformation>(mpd, segmentBaseNode, info);

    if(!base->initialisationSegment.Get() && base->indexSegment.Get() && base->indexSegment.Get()->getOffset())
    {
        InitSegment *initSeg = new InitSegment( info );
        initSeg->setSourceUrl(base->getUrlSegment().toString());
        initSeg->setByteRange(0, base->indexSegment.Get()->getOffset() - 1);
        base->initialisationSegment.Set(initSeg);
    }

    info->addAttribute(base);

    return 1;
}

size_t IsoffMainParser::parseSegmentList(MPD *mpd, Node * segListNode, SegmentInformation *info)
{
    size_t total = 0;
    if(segListNode)
    {
        std::vector<Node *> segments = DOMHelper::getElementByTagName(segListNode, "SegmentURL", false);
        SegmentList *list;
        if((list = new (std::nothrow) SegmentList(info)))
        {
            parseMultipleSegmentBaseType(mpd, segListNode, list, info);

            parseAvailability<SegmentInformation>(mpd, segListNode, info);

            uint64_t nzStartTime = 0;
            std::vector<Node *>::const_iterator it;
            for(it = segments.begin(); it != segments.end(); ++it)
            {
                Node *segmentURL = *it;

                Segment *seg = new (std::nothrow) Segment(info);
                if(!seg)
                    continue;

                std::string mediaUrl = segmentURL->getAttributeValue("media");
                if(!mediaUrl.empty())
                    seg->setSourceUrl(mediaUrl);

                if(segmentURL->hasAttribute("mediaRange"))
                {
                    std::string range = segmentURL->getAttributeValue("mediaRange");
                    size_t pos = range.find("-");
                    seg->setByteRange(atoi(range.substr(0, pos).c_str()), atoi(range.substr(pos + 1, range.size()).c_str()));
                }

                stime_t duration = list->inheritDuration();
                if(duration)
                {
                    seg->startTime.Set(nzStartTime);
                    seg->duration.Set(duration);
                    nzStartTime += duration;
                }

                seg->setSequenceNumber(total);

                list->addSegment(seg);
                total++;
            }

            info->updateSegmentList(list, true);
        }
    }
    return total;
}

void IsoffMainParser::parseInitSegment(Node *initNode, Initializable<InitSegment> *init, SegmentInformation *parent)
{
    if(!initNode)
        return;

    InitSegment *seg = new InitSegment( parent );
    seg->setSourceUrl(initNode->getAttributeValue("sourceURL"));

    if(initNode->hasAttribute("range"))
    {
        std::string range = initNode->getAttributeValue("range");
        size_t pos = range.find("-");
        seg->setByteRange(atoi(range.substr(0, pos).c_str()), atoi(range.substr(pos + 1, range.size()).c_str()));
    }

    init->initialisationSegment.Set(seg);
}

void IsoffMainParser::parseTimeline(Node *node, AbstractMultipleSegmentBaseType *base)
{
    if(!node)
        return;

    uint64_t number = 0;
    if(node->hasAttribute("startNumber"))
        number = Integer<uint64_t>(node->getAttributeValue("startNumber"));
    else if(base->inheritStartNumber())
        number = base->inheritStartNumber();

    SegmentTimeline *timeline = new (std::nothrow) SegmentTimeline(base);
    if(timeline)
    {
        std::vector<Node *> elements = DOMHelper::getElementByTagName(node, "S", false);
        std::vector<Node *>::const_iterator it;
        for(it = elements.begin(); it != elements.end(); ++it)
        {
            const Node *s = *it;
            if(!s->hasAttribute("d")) /* Mandatory */
                continue;
            stime_t d = Integer<stime_t>(s->getAttributeValue("d"));
            int64_t r = 0; // never repeats by default
            if(s->hasAttribute("r"))
            {
                r = Integer<int64_t>(s->getAttributeValue("r"));
                if(r < 0)
                    r = std::numeric_limits<unsigned>::max();
            }

            if(s->hasAttribute("t"))
            {
                stime_t t = Integer<stime_t>(s->getAttributeValue("t"));
                timeline->addElement(number, d, r, t);
            }
            else timeline->addElement(number, d, r);

            number += (1 + r);
        }
        //base->setSegmentTimeline(timeline);
        base->addAttribute(timeline);
    }
}

void IsoffMainParser::parseProgramInformation(Node * node, MPD *mpd)
{
    if(!node)
        return;

    ProgramInformation *info = new (std::nothrow) ProgramInformation();
    if (info)
    {
        Node *child = DOMHelper::getFirstChildElementByName(node, "Title");
        if(child)
            info->setTitle(child->getText());

        child = DOMHelper::getFirstChildElementByName(node, "Source");
        if(child)
            info->setSource(child->getText());

        child = DOMHelper::getFirstChildElementByName(node, "Copyright");
        if(child)
            info->setCopyright(child->getText());

        if(node->hasAttribute("moreInformationURL"))
            info->setMoreInformationUrl(node->getAttributeValue("moreInformationURL"));

        mpd->programInfo.Set(info);
    }
}

Profile IsoffMainParser::getProfile() const
{
    Profile res(Profile::Name::Unknown);
    if(this->root == NULL)
        return res;

    std::string urn = root->getAttributeValue("profiles");
    if ( urn.length() == 0 )
        urn = root->getAttributeValue("profile"); //The standard spells it the both ways...

    size_t pos;
    size_t nextpos = std::string::npos;
    do
    {
        pos = nextpos + 1;
        nextpos = urn.find_first_of(",", pos);
        res = Profile(urn.substr(pos, nextpos - pos));
    }
    while (nextpos != std::string::npos && res == Profile::Name::Unknown);

    return res;
}
