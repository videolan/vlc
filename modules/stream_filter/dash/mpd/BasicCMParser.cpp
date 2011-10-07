/*
 * BasicCMParser.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "BasicCMParser.h"

using namespace dash::mpd;
using namespace dash::xml;

BasicCMParser::BasicCMParser    (Node *root)
{
    this->root = root;
}
BasicCMParser::~BasicCMParser   ()
{
}

bool    BasicCMParser::parse                ()
{
    this->setMPD();
    return true;
}
void    BasicCMParser::setMPD               ()
{
    this->mpd = new MPD(this->root->getAttributes());
    this->setMPDBaseUrl(this->root);
    this->setPeriods(this->root);
}
void    BasicCMParser::setMPDBaseUrl        (Node *root)
{
    std::vector<Node *> baseUrls = DOMHelper::getChildElementByTagName(root, "BaseURL");

    for(int i = 0; i < baseUrls.size(); i++)
    {
        BaseUrl *url = new BaseUrl(baseUrls.at(i)->getText());
        this->mpd->addBaseUrl(url);
    }
}
void    BasicCMParser::setPeriods           (Node *root)
{
    std::vector<Node *> periods = DOMHelper::getElementByTagName(root, "Period", false);

    for(int i = 0; i < periods.size(); i++)
    {
        Period *period = new Period(periods.at(i)->getAttributes());
        this->setGroups(periods.at(i), period);
        this->mpd->addPeriod(period);
    }
}
void    BasicCMParser::setGroups            (Node *root, Period *period)
{
    std::vector<Node *> groups = DOMHelper::getElementByTagName(root, "Group", false);

    for(int i = 0; i < groups.size(); i++)
    {
        Group *group = new Group(groups.at(i)->getAttributes());
        this->setRepresentations(groups.at(i), group);
        period->addGroup(group);
    }
}
void    BasicCMParser::setRepresentations   (Node *root, Group *group)
{
    std::vector<Node *> representations = DOMHelper::getElementByTagName(root, "Representation", false);

    for(int i = 0; i < representations.size(); i++)
    {
        Representation *rep = new Representation(representations.at(i)->getAttributes());
        this->setSegmentInfo(representations.at(i), rep);
        group->addRepresentation(rep);
    }
}
void    BasicCMParser::setSegmentInfo       (Node *root, Representation *rep)
{
    std::vector<Node *> segmentInfo = DOMHelper::getChildElementByTagName(root, "SegmentInfo");

    for(int i = 0; i < segmentInfo.size(); i++)
    {
        SegmentInfo *info = new SegmentInfo(segmentInfo.at(i)->getAttributes());
        this->setInitSegment(segmentInfo.at(i), info);
        this->setSegments(segmentInfo.at(i), info);
        rep->setSegmentInfo(info);
        return;
    }
}
void    BasicCMParser::setInitSegment       (Node *root, SegmentInfo *info)
{
    std::vector<Node *> initSeg = DOMHelper::getChildElementByTagName(root, "InitialisationSegmentURL");

    for(int i = 0; i < initSeg.size(); i++)
    {
        InitSegment *seg = new InitSegment(initSeg.at(i)->getAttributes());
        info->setInitSegment(seg);
        return;
    }
}
void    BasicCMParser::setSegments          (Node *root, SegmentInfo *info)
{
    std::vector<Node *> segments = DOMHelper::getElementByTagName(root, "Url", false);

    for(int i = 0; i < segments.size(); i++)
    {
        Segment *seg = new Segment(segments.at(i)->getAttributes());
        info->addSegment(seg);
    }
}
MPD*    BasicCMParser::getMPD               ()
{
    return this->mpd;
}
