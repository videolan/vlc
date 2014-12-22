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
#include "SegmentTemplate.h"
#include "SegmentInfoDefault.h"
#include "xml/DOMHelper.h"
#include <vlc_strings.h>
#include <vlc_stream.h>
#include <cstdio>
#include <sstream>

using namespace dash::mpd;
using namespace dash::xml;

IsoffMainParser::IsoffMainParser    (Node *root, stream_t *p_stream) :
                 IMPDParser(root, NULL, p_stream, NULL)
{
}
IsoffMainParser::~IsoffMainParser   ()
{
}

bool    IsoffMainParser::parse              (Profile profile)
{
    mpd = new MPD(p_stream, profile);
    setMPDAttributes();
    setMPDBaseUrl(root);
    parsePeriods(root);

    print();
    return true;
}

void    IsoffMainParser::setMPDAttributes   ()
{
    const std::map<std::string, std::string> attr = this->root->getAttributes();

    std::map<std::string, std::string>::const_iterator it;

    it = attr.find("mediaPresentationDuration");
    if(it != attr.end())
        this->mpd->setDuration(IsoTime(it->second));

    it = attr.find("minBufferTime");
    if(it != attr.end())
        this->mpd->setMinBufferTime(IsoTime(it->second));

    it = attr.find("type");
    if(it != attr.end())
        mpd->setType(it->second);
}

void IsoffMainParser::parsePeriods(Node *root)
{
    std::vector<Node *> periods = DOMHelper::getElementByTagName(root, "Period", false);
    std::vector<Node *>::const_iterator it;

    for(it = periods.begin(); it != periods.end(); it++)
    {
        Period *period = new (std::nothrow) Period(mpd);
        if (!period)
            continue;
        parseSegmentInformation(*it, period);
        setAdaptationSets(*it, period);
        mpd->addPeriod(period);
    }
}

size_t IsoffMainParser::parseSegmentTemplate(Node *templateNode, SegmentInformation *info)
{
    size_t total = 0;
    if (templateNode == NULL || !templateNode->hasAttribute("media"))
        return total;

    std::string mediaurl = templateNode->getAttributeValue("media");
    SegmentTemplate *mediaTemplate = NULL;
    if(mediaurl.empty() || !(mediaTemplate = new (std::nothrow) SegmentTemplate(info)) )
        return total;
    mediaTemplate->setSourceUrl(mediaurl);

    if(templateNode->hasAttribute("startNumber"))
    {
        std::istringstream in(templateNode->getAttributeValue("startNumber"));
        size_t i;
        in >> i;
        mediaTemplate->setStartIndex(i);
    }

    if(templateNode->hasAttribute("duration"))
    {
        std::istringstream in(templateNode->getAttributeValue("duration"));
        size_t i;
        in >> i;
        mediaTemplate->setDuration(i);
    }

    InitSegmentTemplate *initTemplate = NULL;

    if(templateNode->hasAttribute("initialization"))
    {
        std::string initurl = templateNode->getAttributeValue("initialization");
        if(!initurl.empty() && (initTemplate = new (std::nothrow) InitSegmentTemplate(info)))
            initTemplate->setSourceUrl(initurl);
    }

    info->setSegmentTemplate(mediaTemplate, SegmentInformation::INFOTYPE_MEDIA);
    info->setSegmentTemplate(initTemplate, SegmentInformation::INFOTYPE_INIT);

    total += ( mediaTemplate != NULL );

    return total;
}

size_t IsoffMainParser::parseSegmentInformation(Node *node, SegmentInformation *info)
{
    size_t total = 0;
    parseSegmentBase(DOMHelper::getFirstChildElementByName(node, "SegmentBase"), info);
    total += parseSegmentList(DOMHelper::getFirstChildElementByName(node, "SegmentList"), info);
    total += parseSegmentTemplate(DOMHelper::getFirstChildElementByName(node, "SegmentTemplate" ), info);
    if(node->hasAttribute("bitstreamSwitching"))
        info->setBitstreamSwitching(node->getAttributeValue("bitstreamSwitching") == "true");
    return total;
}

void    IsoffMainParser::setAdaptationSets  (Node *periodNode, Period *period)
{
    std::vector<Node *> adaptationSets = DOMHelper::getElementByTagName(periodNode, "AdaptationSet", false);
    std::vector<Node *>::const_iterator it;

    for(it = adaptationSets.begin(); it != adaptationSets.end(); it++)
    {
        AdaptationSet *adaptationSet = new AdaptationSet(period);
        if(!adaptationSet)
            continue;
        if((*it)->hasAttribute("mimeType"))
            adaptationSet->setMimeType((*it)->getAttributeValue("mimeType"));

        parseSegmentInformation( *it, adaptationSet );

        setRepresentations((*it), adaptationSet);
        period->addAdaptationSet(adaptationSet);
    }
}
void    IsoffMainParser::setRepresentations (Node *adaptationSetNode, AdaptationSet *adaptationSet)
{
    std::vector<Node *> representations = DOMHelper::getElementByTagName(adaptationSetNode, "Representation", false);

    for(size_t i = 0; i < representations.size(); i++)
    {
        this->currentRepresentation = new Representation(adaptationSet, getMPD());
        Node *repNode = representations.at(i);

        std::vector<Node *> baseUrls = DOMHelper::getChildElementByTagName(repNode, "BaseURL");
        if(!baseUrls.empty())
            currentRepresentation->setBaseUrl( new BaseUrl( baseUrls.front()->getText() ) );

        if(repNode->hasAttribute("id"))
            currentRepresentation->setId(repNode->getAttributeValue("id"));

        if(repNode->hasAttribute("width"))
            this->currentRepresentation->setWidth(atoi(repNode->getAttributeValue("width").c_str()));

        if(repNode->hasAttribute("height"))
            this->currentRepresentation->setHeight(atoi(repNode->getAttributeValue("height").c_str()));

        if(repNode->hasAttribute("bandwidth"))
            this->currentRepresentation->setBandwidth(atoi(repNode->getAttributeValue("bandwidth").c_str()));

        if(repNode->hasAttribute("mimeType"))
            currentRepresentation->setMimeType(repNode->getAttributeValue("mimeType"));

        size_t totalmediasegments = parseSegmentInformation(repNode, currentRepresentation);
        if(!totalmediasegments)
        {
            /* unranged & segment less representation, add fake segment */
            SegmentList *list = new SegmentList();
            Segment *seg = new Segment(currentRepresentation);
            if(list && seg)
            {
                list->addSegment(seg);
                currentRepresentation->setSegmentList(list);
            }
            else
            {
                delete seg;
                delete list;
            }
        }

        adaptationSet->addRepresentation(this->currentRepresentation);
    }
}

void IsoffMainParser::parseSegmentBase(Node * segmentBaseNode, SegmentInformation *info)
{
    if(!segmentBaseNode)
        return;

    else if(segmentBaseNode->hasAttribute("indexRange"))
    {
        SegmentList *list = new SegmentList();
        Segment *seg;

        size_t start = 0, end = 0;
        if (std::sscanf(segmentBaseNode->getAttributeValue("indexRange").c_str(), "%zu-%zu", &start, &end) == 2)
        {
            seg = new IndexSegment(info);
            seg->setByteRange(start, end);
            list->addSegment(seg);
            /* index must be before data, so data starts at index end */
            seg = new Segment(info);
            seg->setByteRange(end + 1, 0);
        }
        else
        {
            seg = new Segment(info);
        }

        list->addSegment(seg);
        info->setSegmentList(list);

        std::vector<Node *> initSeg = DOMHelper::getElementByTagName(segmentBaseNode, "Initialization", false);
        if(!initSeg.empty())
        {
            SegmentBase *base = new SegmentBase();
            setInitSegment(segmentBaseNode, base);
            info->setSegmentBase(base);
        }
    }
    else
    {
        SegmentBase *base = new SegmentBase();
        setInitSegment(segmentBaseNode, base);
        info->setSegmentBase(base);
    }
}

size_t IsoffMainParser::parseSegmentList(Node * segListNode, SegmentInformation *info)
{
    size_t total = 0;
    if(segListNode)
    {
        std::vector<Node *> segments = DOMHelper::getElementByTagName(segListNode, "SegmentURL", false);
        SegmentList *list;
        if(!segments.empty() && (list = new (std::nothrow) SegmentList()))
        {
            std::vector<Node *>::const_iterator it;
            for(it = segments.begin(); it != segments.end(); it++)
            {
                Node *segmentURL = *it;
                std::string mediaUrl = segmentURL->getAttributeValue("media");
                if(mediaUrl.empty())
                    continue;

                Segment *seg = new (std::nothrow) Segment(info);
                if(!seg)
                    continue;

                seg->setSourceUrl(segmentURL->getAttributeValue("media"));

                if(segmentURL->hasAttribute("mediaRange"))
                {
                    std::string range = segmentURL->getAttributeValue("mediaRange");
                    size_t pos = range.find("-");
                    seg->setByteRange(atoi(range.substr(0, pos).c_str()), atoi(range.substr(pos + 1, range.size()).c_str()));
                }

                list->addSegment(seg);
                total++;
            }

            info->setSegmentList(list);
        }
    }
    return total;
}

void    IsoffMainParser::setInitSegment     (dash::xml::Node *segBaseNode, SegmentBase *base)
{
    std::vector<Node *> initSeg = DOMHelper::getElementByTagName(segBaseNode, "Initialisation", false);

    if(initSeg.size() == 0)
        initSeg = DOMHelper::getElementByTagName(segBaseNode, "Initialization", false);

    if(initSeg.size() > 0)
    {
        Segment *seg = new InitSegment( currentRepresentation );
        seg->setSourceUrl(initSeg.at(0)->getAttributeValue("sourceURL"));

        if(initSeg.at(0)->hasAttribute("range"))
        {
            std::string range = initSeg.at(0)->getAttributeValue("range");
            size_t pos = range.find("-");
            seg->setByteRange(atoi(range.substr(0, pos).c_str()), atoi(range.substr(pos + 1, range.size()).c_str()));
        }

        base->addInitSegment(seg);
    }
}

void    IsoffMainParser::print              ()
{
    if(mpd)
    {
        msg_Dbg(p_stream, "MPD profile=%s mediaPresentationDuration=%ld minBufferTime=%ld",
                static_cast<std::string>(mpd->getProfile()).c_str(),
                mpd->getDuration(),
                mpd->getMinBufferTime());
        msg_Dbg(p_stream, "BaseUrl=%s", mpd->getUrlSegment().toString().c_str());

        std::vector<Period *>::const_iterator i;
        for(i = mpd->getPeriods().begin(); i != mpd->getPeriods().end(); i++)
        {
            std::vector<std::string> debug = (*i)->toString();
            std::vector<std::string>::const_iterator l;
            for(l = debug.begin(); l < debug.end(); l++)
            {
                msg_Dbg(p_stream, "%s", (*l).c_str());
            }
        }
    }
}

IsoTime::IsoTime(const std::string &str)
{
    time = str_duration(str.c_str());
}

IsoTime::operator mtime_t () const
{
    return time;
}
