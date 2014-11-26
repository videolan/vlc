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
#include "xml/DOMHelper.h"
#include <vlc_strings.h>
#include <vlc_stream.h>
#include <cstdio>

using namespace dash::mpd;
using namespace dash::xml;

IsoffMainParser::IsoffMainParser    (Node *root, stream_t *p_stream) :
                 IMPDParser(root, NULL, p_stream, NULL)
{
}
IsoffMainParser::~IsoffMainParser   ()
{
}

bool    IsoffMainParser::parse              ()
{
    mpd = new MPD(p_stream);
    setMPDAttributes();
    setMPDBaseUrl(root);
    setPeriods(root);
    print();
    return true;
}

void    IsoffMainParser::setMPDAttributes   ()
{
    const std::map<std::string, std::string> attr = this->root->getAttributes();

    std::map<std::string, std::string>::const_iterator it;

    it = attr.find("mediaPresentationDuration");
    if(it != attr.end())
        this->mpd->setDuration(str_duration(it->second.c_str()));

    it = attr.find("minBufferTime");
    if(it != attr.end())
        this->mpd->setMinBufferTime(str_duration( it->second.c_str()));

}

void    IsoffMainParser::setAdaptationSets  (Node *periodNode, Period *period)
{
    std::vector<Node *> adaptationSets = DOMHelper::getElementByTagName(periodNode, "AdaptationSet", false);
    std::vector<Node *>::const_iterator it;

    for(it = adaptationSets.begin(); it != adaptationSets.end(); it++)
    {
        AdaptationSet *adaptationSet = new AdaptationSet();
        if(!adaptationSet)
            continue;
        if((*it)->hasAttribute("mimeType"))
            adaptationSet->setMimeType((*it)->getAttributeValue("mimeType"));
        setRepresentations((*it), adaptationSet);
        period->addAdaptationSet(adaptationSet);
    }
}
void    IsoffMainParser::setRepresentations (Node *adaptationSetNode, AdaptationSet *adaptationSet)
{
    std::vector<Node *> representations = DOMHelper::getElementByTagName(adaptationSetNode, "Representation", false);

    for(size_t i = 0; i < representations.size(); i++)
    {
        this->currentRepresentation = new Representation(getMPD());
        Node *repNode = representations.at(i);

        std::vector<Node *> baseUrls = DOMHelper::getChildElementByTagName(repNode, "BaseURL");
        if(!baseUrls.empty())
            currentRepresentation->setBaseUrl( new BaseUrl( baseUrls.front()->getText() ) );

        if(repNode->hasAttribute("width"))
            this->currentRepresentation->setWidth(atoi(repNode->getAttributeValue("width").c_str()));

        if(repNode->hasAttribute("height"))
            this->currentRepresentation->setHeight(atoi(repNode->getAttributeValue("height").c_str()));

        if(repNode->hasAttribute("bandwidth"))
            this->currentRepresentation->setBandwidth(atoi(repNode->getAttributeValue("bandwidth").c_str()));

        if(repNode->hasAttribute("mimeType"))
            currentRepresentation->setMimeType(repNode->getAttributeValue("mimeType"));

        this->setSegmentBase(repNode, this->currentRepresentation);
        this->setSegmentList(repNode, this->currentRepresentation);
        adaptationSet->addRepresentation(this->currentRepresentation);
    }
}

void    IsoffMainParser::setSegmentBase     (dash::xml::Node *repNode, Representation *rep)
{
    std::vector<Node *> segmentBase = DOMHelper::getElementByTagName(repNode, "SegmentBase", false);

    if(segmentBase.front()->hasAttribute("indexRange"))
    {
        SegmentList *list = new SegmentList();
        Segment *seg;

        size_t start = 0, end = 0;
        if (std::sscanf(segmentBase.front()->getAttributeValue("indexRange").c_str(), "%"PRIu64"-%"PRIu64, &start, &end) == 2)
        {
            seg = new IndexSegment(rep);
            seg->setByteRange(start, end);
            list->addSegment(seg);
            /* index must be before data, so data starts at index end */
            seg = new Segment(rep);
            seg->setByteRange(end + 1, 0);
        }
        else
        {
            seg = new Segment(rep);
        }

        list->addSegment(seg);
        rep->setSegmentList(list);

        std::vector<Node *> initSeg = DOMHelper::getElementByTagName(segmentBase.front(), "Initialization", false);
        if(!initSeg.empty())
        {
            SegmentBase *base = new SegmentBase();
            setInitSegment(segmentBase.front(), base);
            rep->setSegmentBase(base);
        }
    }
    else if(!segmentBase.empty())
    {
        SegmentBase *base = new SegmentBase();
        setInitSegment(segmentBase.front(), base);
        rep->setSegmentBase(base);
    }
}
void    IsoffMainParser::setSegmentList     (dash::xml::Node *repNode, Representation *rep)
{
    std::vector<Node *> segmentList = DOMHelper::getElementByTagName(repNode, "SegmentList", false);

    if(segmentList.size() > 0)
    {
        SegmentList *list = new SegmentList();
        this->setSegments(segmentList.at(0), list);
        rep->setSegmentList(list);
    }

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
void    IsoffMainParser::setSegments        (dash::xml::Node *segListNode, SegmentList *list)
{
    std::vector<Node *> segments = DOMHelper::getElementByTagName(segListNode, "SegmentURL", false);

    for(size_t i = 0; i < segments.size(); i++)
    {
        Segment *seg = new Segment( this->currentRepresentation );
        seg->setSourceUrl(segments.at(i)->getAttributeValue("media"));

        if(segments.at(i)->hasAttribute("mediaRange"))
        {
            std::string range = segments.at(i)->getAttributeValue("mediaRange");
            size_t pos = range.find("-");
            seg->setByteRange(atoi(range.substr(0, pos).c_str()), atoi(range.substr(pos + 1, range.size()).c_str()));
        }

        list->addSegment(seg);
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
        msg_Dbg(p_stream, "BaseUrl=%s", mpd->getUrlSegment().c_str());

        std::vector<Period *>::const_iterator i;
        for(i = mpd->getPeriods().begin(); i != mpd->getPeriods().end(); i++)
        {
            msg_Dbg(p_stream, " Period");
            std::vector<AdaptationSet *>::const_iterator j;
            for(j = (*i)->getAdaptationSets().begin(); j != (*i)->getAdaptationSets().end(); j++)
            {
                msg_Dbg(p_stream, "  AdaptationSet");
                std::vector<Representation *>::const_iterator k;
                for(k = (*j)->getRepresentations().begin(); k != (*j)->getRepresentations().end(); k++)
                {
                    std::vector<std::string> debug = (*k)->toString();
                    std::vector<std::string>::const_iterator l;
                    for(l = debug.begin(); l < debug.end(); l++)
                    {
                        msg_Dbg(p_stream, "%s", (*l).c_str());
                    }
                }
            }
        }
    }
}
