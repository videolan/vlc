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
    mpd = new MPD();
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

    for(size_t i = 0; i < adaptationSets.size(); i++)
    {
        AdaptationSet *adaptationSet = new AdaptationSet();
        this->setRepresentations(adaptationSets.at(i), adaptationSet);
        period->addAdaptationSet(adaptationSet);
    }
}
void    IsoffMainParser::setRepresentations (Node *adaptationSetNode, AdaptationSet *adaptationSet)
{
    std::vector<Node *> representations = DOMHelper::getElementByTagName(adaptationSetNode, "Representation", false);

    for(size_t i = 0; i < representations.size(); i++)
    {
        this->currentRepresentation = new Representation;
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

        this->setSegmentBase(repNode, this->currentRepresentation);
        this->setSegmentList(repNode, this->currentRepresentation);
        adaptationSet->addRepresentation(this->currentRepresentation);
    }
}
void    IsoffMainParser::setSegmentBase     (dash::xml::Node *repNode, Representation *rep)
{
    std::vector<Node *> segmentBase = DOMHelper::getElementByTagName(repNode, "SegmentBase", false);

    if(segmentBase.size() > 0)
    {
        SegmentBase *base = new SegmentBase();
        this->setInitSegment(segmentBase.at(0), base);
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
        Segment *seg = new Segment( this->currentRepresentation );
        seg->setSourceUrl(initSeg.at(0)->getAttributeValue("sourceURL"));

        if(initSeg.at(0)->hasAttribute("range"))
        {
            std::string range = initSeg.at(0)->getAttributeValue("range");
            size_t pos = range.find("-");
            seg->setByteRange(atoi(range.substr(0, pos).c_str()), atoi(range.substr(pos + 1, range.size()).c_str()));
        }

        for(size_t i = 0; i < this->mpd->getBaseUrls().size(); i++)
            seg->addBaseUrl(this->mpd->getBaseUrls().at(i));

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

        for(size_t j = 0; j < this->mpd->getBaseUrls().size(); j++)
            seg->addBaseUrl(this->mpd->getBaseUrls().at(j));

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
        std::vector<BaseUrl *>::const_iterator h;
        for(h = mpd->getBaseUrls().begin(); h != mpd->getBaseUrls().end(); h++)
            msg_Dbg(p_stream, "BaseUrl=%s", (*h)->getUrl().c_str());

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
                    msg_Dbg(p_stream, "   Representation");
                    msg_Dbg(p_stream, "    InitSeg url=%s", (*k)->getSegmentBase()->getInitSegment()->getSourceUrl().c_str());

                    const SegmentList *segmentList = (*k)->getSegmentList();
                    if (segmentList)
                    {
                        std::vector<Segment *>::const_iterator l;
                        for(l = segmentList->getSegments().begin();
                            l < segmentList->getSegments().end(); l++)
                        {
                            msg_Dbg(p_stream, "    Segment url=%s", (*l)->getSourceUrl().c_str());
                        }
                    }
                }
            }
        }
    }
}
