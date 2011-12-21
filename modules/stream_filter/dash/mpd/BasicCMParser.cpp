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

#include <cstdlib>
#include <sstream>

using namespace dash::mpd;
using namespace dash::xml;

BasicCMParser::BasicCMParser    (Node *root) : root(root), mpd(NULL)
{
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

    for(size_t i = 0; i < baseUrls.size(); i++)
    {
        BaseUrl *url = new BaseUrl(baseUrls.at(i)->getText());
        this->mpd->addBaseUrl(url);
    }
}
void    BasicCMParser::setPeriods           (Node *root)
{
    std::vector<Node *> periods = DOMHelper::getElementByTagName(root, "Period", false);

    for(size_t i = 0; i < periods.size(); i++)
    {
        Period *period = new Period(periods.at(i)->getAttributes());
        this->setGroups(periods.at(i), period);
        this->mpd->addPeriod(period);
    }
}

void    BasicCMParser::setGroups            (Node *root, Period *period)
{
    std::vector<Node *> groups = DOMHelper::getElementByTagName(root, "Group", false);

    for(size_t i = 0; i < groups.size(); i++)
    {
        Group *group = new Group(groups.at(i)->getAttributes());
        if ( this->parseCommonAttributesElements( groups.at( i ), group ) == false )
        {
            delete group;
            continue ;
        }
        this->setRepresentations(groups.at(i), group);
        period->addGroup(group);
    }
}

void    BasicCMParser::setRepresentations   (Node *root, Group *group)
{
    std::vector<Node *> representations = DOMHelper::getElementByTagName(root, "Representation", false);

    for(size_t i = 0; i < representations.size(); i++)
    {
        const std::map<std::string, std::string>    attributes = representations.at(i)->getAttributes();

        //FIXME: handle @dependencyId afterward
        Representation *rep = new Representation( attributes );
        if ( this->parseCommonAttributesElements( representations.at( i ), rep ) == false )
        {
            delete rep;
            continue ;
        }
        std::map<std::string, std::string>::const_iterator  it;

        it = attributes.find( "id" );
        if ( it == attributes.end() )
        {
            std::cerr << "Missing mandatory attribute for Representation: @id" << std::endl;
            delete rep;
            continue ;
        }
        rep->setId( it->second );

        it = attributes.find( "bandwidth" );
        if ( it == attributes.end() )
        {
            std::cerr << "Missing mandatory attribute for Representation: @bandwidth" << std::endl;
            delete rep;
            continue ;
        }
        rep->setBandwidth( atoi( it->second.c_str() ) );

        it = attributes.find( "qualityRanking" );
        if ( it != attributes.end() )
            rep->setQualityRanking( atoi( it->second.c_str() ) );

        this->setSegmentInfo(representations.at(i), rep);
        if ( rep->getSegmentInfo() && rep->getSegmentInfo()->getSegments().size() > 0 )
            group->addRepresentation(rep);
    }
}
void    BasicCMParser::setSegmentInfo       (Node *root, Representation *rep)
{
    Node    *segmentInfo = DOMHelper::getFirstChildElementByName( root, "SegmentInfo");

    if ( segmentInfo )
    {
        SegmentInfo *info = new SegmentInfo( segmentInfo->getAttributes() );
        this->setInitSegment( segmentInfo, info );
        this->setSegments(segmentInfo, info );
        rep->setSegmentInfo(info);
    }
}

void    BasicCMParser::setInitSegment       (Node *root, SegmentInfo *info)
{
    std::vector<Node *> initSeg = DOMHelper::getChildElementByTagName(root, "InitialisationSegmentURL");

    for(size_t i = 0; i < initSeg.size(); i++)
    {
        InitSegment *seg = new InitSegment(initSeg.at(i)->getAttributes());
        info->setInitSegment(seg);
        return;
    }
}
void    BasicCMParser::setSegments          (Node *root, SegmentInfo *info)
{
    std::vector<Node *> segments = DOMHelper::getElementByTagName(root, "Url", false);

    for(size_t i = 0; i < segments.size(); i++)
    {
        Segment *seg = new Segment(segments.at(i)->getAttributes());
        info->addSegment(seg);
    }
}
MPD*    BasicCMParser::getMPD               ()
{
    return this->mpd;
}

bool    BasicCMParser::parseCommonAttributesElements( Node *node, CommonAttributesElements *common) const
{
    const std::map<std::string, std::string>                &attr = node->getAttributes();
    std::map<std::string, std::string>::const_iterator      it;
    //Parse mandatory elements first.
    it = attr.find( "mimeType" );
    if ( it == attr.end() )
    {
        std::cerr << "Missing mandatory attribute: @mimeType" << std::endl;
        return false;
    }
    common->setMimeType( it->second );
    //Everything else is optionnal.
    it = attr.find( "width" );
    if ( it != attr.end() )
        common->setWidth( atoi( it->second.c_str() ) );
    it = attr.find( "height" );
    if ( it != attr.end() )
        common->setHeight( atoi( it->second.c_str() ) );
    it = attr.find( "parx" );
    if ( it != attr.end() )
        common->setParX( atoi( it->second.c_str() ) );
    it = attr.find( "pary" );
    if ( it != attr.end() )
        common->setParY( atoi( it->second.c_str() ) );
    it = attr.find( "frameRate" );
    if ( it != attr.end() )
        common->setFrameRate( atoi( it->second.c_str() ) );
    it = attr.find( "lang" );

    if ( it != attr.end() && it->second.empty() == false )
    {
        std::istringstream  s( it->second );
        while ( s )
        {
            std::string     lang;
            s >> lang;
            common->addLang( lang );
        }
    }
    it = attr.find( "numberOfChannels" );
    if ( it != attr.end() )
    {
        std::istringstream  s( it->second );
        while ( s )
        {
            std::string     channel;
            s >> channel;
            common->addChannel( channel );
        }
    }
    it = attr.find( "samplingRate" );
    if ( it != attr.end() )
    {
        std::istringstream  s( it->second );
        while ( s )
        {
            int         rate;
            s >> rate;
            common->addSampleRate( rate );
        }
    }
    //FIXME: Handle : group, maximumRAPPeriod startWithRAP attributes
    //FIXME: Handle : ContentProtection Accessibility Rating Viewpoing MultipleViews elements
    return true;
}
