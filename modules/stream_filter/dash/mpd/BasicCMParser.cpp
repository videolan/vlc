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
#include "mpd/ContentDescription.h"

#include <cstdlib>
#include <sstream>

#include <vlc_common.h>
#include <vlc_strings.h>

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
bool    BasicCMParser::setMPD()
{
    const std::map<std::string, std::string>    attr = this->root->getAttributes();
    this->mpd = new MPD;

    std::map<std::string, std::string>::const_iterator  it;
    it = attr.find( "profile" );
    if ( it != attr.end() )
        this->mpd->setProfile( it->second );

    it = attr.find("mediaPresentationDuration");
    /*
        Standard specifies a default of "On-Demand",
        so anything that is not "Live" is "On-Demand"
    */
    this->mpd->setLive( it != attr.end() && it->second == "Live" );
    it = attr.find( "availabilityStartTime" );
    if ( it == attr.end() && this->mpd->isLive() == true )
    {
        std::cerr << "An @availabilityStartTime attribute must be specified when"
                     " the stream @type is Live" << std::endl;
        return false;
    }
    if ( it != attr.end() )
    {
        struct tm   t;
        char        *res = strptime( it->second.c_str(), "%Y-%m-%dT%T", &t );
        if ( res == NULL )
        {
            if ( this->mpd->isLive() == true )
            {
                std::cerr << "An @availabilityStartTime attribute must be specified when"
                             " the stream @type is Live" << std::endl;
                return false;
            }
        }
        else
            this->mpd->setAvailabilityStartTime( mktime( &t ) );
    }
    it = attr.find( "availabilityEndTime" );
    if ( it != attr.end() )
    {
        struct tm   t;
        char        *res = strptime( it->second.c_str(), "%Y-%m-%dT%T", &t );
        if ( res != NULL )
            this->mpd->setAvailabilityEndTime( mktime( &t ) );
    }
    it = attr.find( "mediaPresentationDuration" );
    if ( it != attr.end() )
        this->mpd->setDuration( str_duration( it->second.c_str() ) );
    it = attr.find( "minimumUpdatePeriodMPD" );
    if ( it != attr.end() )
        this->mpd->setMinUpdatePeriod( str_duration( it->second.c_str() ) );
    it = attr.find( "minBufferTime" );
    if ( it != attr.end() )
        this->mpd->setMinBufferTime( str_duration( it->second.c_str() ) );

    if ( this->mpd->isLive() )
    {
        //This value is undefined when using type "On-Demand"
        it = attr.find( "timeshiftBufferDepth" );
        if ( it != attr.end() )
            this->mpd->setTimeShiftBufferDepth( str_duration( it->second.c_str() ) );
    }

    this->setMPDBaseUrl(this->root);
    this->setPeriods(this->root);
    this->mpd->setProgramInformation( this->parseProgramInformation() );
    return true;
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

        it = attributes.find( "dependencyId" );
        if ( it != attributes.end() )
            this->handleDependencyId( rep, group, it->second );

        if ( this->setSegmentInfo( representations.at(i), rep ) == false )
        {
            delete rep;
            continue ;
        }
        group->addRepresentation(rep);
    }
}

void    BasicCMParser::handleDependencyId( Representation *rep, const Group *group, const std::string &dependencyId )
{
    if ( dependencyId.empty() == true )
        return ;
    std::istringstream  s( dependencyId );
    while ( s )
    {
        std::string     id;
        s >> id;
        const Representation    *dep = group->getRepresentationById( id );
        if ( dep )
            rep->addDependency( dep );
    }
}

bool    BasicCMParser::setSegmentInfo       (Node *root, Representation *rep)
{
    Node    *segmentInfo = DOMHelper::getFirstChildElementByName( root, "SegmentInfo");

    if ( segmentInfo )
    {
        const std::map<std::string, std::string> attr = segmentInfo->getAttributes();

        SegmentInfo *info = new SegmentInfo();
        //Init segment is not mandatory.
        this->setInitSegment( segmentInfo, info );
        //If we don't have any segment, there's no point keeping this SegmentInfo.
        if ( this->setSegments(segmentInfo, info ) == false )
        {
            delete info;
            return false;
        }
        std::map<std::string, std::string>::const_iterator  it;
        it = attr.find( "duration" );
        if ( it != attr.end() )
            info->setDuration( str_duration( it->second.c_str() ) );

        rep->setSegmentInfo(info);
        return true;
    }
    return false;
}

bool BasicCMParser::parseSegment(Segment *seg, const std::map<std::string, std::string>& attr )
{
    std::map<std::string, std::string>::const_iterator  it;

    it = attr.find( "sourceURL" );
    //FIXME: When not present, the sourceUrl attribute should be computed
    //using BaseURL and the range attribute.
    if ( it != attr.end() )
        seg->setSourceUrl( it->second );
    return true;
}

ProgramInformation* BasicCMParser::parseProgramInformation()
{
    Node*   pInfoNode = DOMHelper::getFirstChildElementByName( this->root, "ProgramInformation" );
    if ( pInfoNode == NULL )
        return NULL;
    ProgramInformation  *pInfo = new ProgramInformation;
    const std::map<std::string, std::string>    attr = pInfoNode->getAttributes();
    std::map<std::string, std::string>::const_iterator  it;
    it = attr.find( "moreInformationURL" );
    if ( it != attr.end() )
        pInfo->setMoreInformationUrl( it->second );
    Node*   title = DOMHelper::getFirstChildElementByName( pInfoNode, "Title" );
    if ( title )
        pInfo->setTitle( title->getText() );
    Node*   source = DOMHelper::getFirstChildElementByName( pInfoNode, "Source" );
    if ( source )
        pInfo->setSource( source->getText() );
    Node*   copyright = DOMHelper::getFirstChildElementByName( pInfoNode, "copyright" );
    if ( copyright )
        pInfo->setCopyright( copyright->getText() );
    return pInfo;
}

void    BasicCMParser::setInitSegment       (Node *root, SegmentInfo *info)
{
    const std::vector<Node *> initSeg = DOMHelper::getChildElementByTagName(root, "InitialisationSegmentURL");

    if (  initSeg.size() > 1 )
        std::cerr << "There could be at most one InitialisationSegmentURL per SegmentInfo"
                     " other InitialisationSegmentURL will be dropped." << std::endl;
    if ( initSeg.size() == 1 )
    {
        Segment     *seg = new Segment();
        parseSegment( seg, initSeg.at(0)->getAttributes() );
        info->setInitSegment( seg );
    }
}

bool    BasicCMParser::setSegments          (Node *root, SegmentInfo *info)
{
    std::vector<Node *> segments = DOMHelper::getElementByTagName(root, "Url", false);

    if ( segments.size() == 0 )
        return false;
    for(size_t i = 0; i < segments.size(); i++)
    {
        Segment *seg = new Segment();
        parseSegment( seg, segments.at(i)->getAttributes() );
        info->addSegment(seg);
    }
    return true;
}

MPD*    BasicCMParser::getMPD()
{
    return this->mpd;
}

void BasicCMParser::parseContentDescriptor(Node *node, const std::string &name, void (CommonAttributesElements::*addPtr)(ContentDescription *), CommonAttributesElements *self) const
{
    std::vector<Node*>  descriptors = DOMHelper::getChildElementByTagName( node, name );
    if ( descriptors.empty() == true )
        return ;
    std::vector<Node*>::const_iterator  it = descriptors.begin();
    std::vector<Node*>::const_iterator  end = descriptors.end();

    while ( it != end )
    {
        const std::map<std::string, std::string>    attr = (*it)->getAttributes();
        std::map<std::string, std::string>::const_iterator  itAttr = attr.find( "schemeIdUri" );
        if  ( itAttr == attr.end() )
        {
            ++it;
            continue ;
        }
        ContentDescription  *desc = new ContentDescription;
        desc->setSchemeIdUri( itAttr->second );
        Node    *schemeInfo = DOMHelper::getFirstChildElementByName( node, "SchemeInformation" );
        if ( schemeInfo != NULL )
            desc->setSchemeInformation( schemeInfo->getText() );
        (self->*addPtr)( desc );
        ++it;
    }
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
    this->parseContentDescriptor( node, "ContentProtection",
                                  &CommonAttributesElements::addContentProtection,
                                  common );
    this->parseContentDescriptor( node, "Accessibility",
                                  &CommonAttributesElements::addAccessibility,
                                  common );
    this->parseContentDescriptor( node, "Rating",
                                  &CommonAttributesElements::addRating, common );
    this->parseContentDescriptor( node, "Viewpoint",
                                  &CommonAttributesElements::addViewpoint, common );
    //FIXME: Handle : group, maximumRAPPeriod startWithRAP attributes
    //FIXME: Handle : ContentProtection Accessibility Rating Viewpoing MultipleViews elements
    return true;
}
