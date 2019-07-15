/*****************************************************************************
 * DASHCommonAttributesElements.cpp: Defines the common attributes and elements
 *                               for some Dash elements.
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#include "DASHCommonAttributesElements.h"

#include "ContentDescription.h"
#include "../../adaptive/StreamFormat.hpp"

#include <vlc_common.h>
#include <vlc_arrays.h>

using namespace dash::mpd;
using namespace adaptive;

DASHCommonAttributesElements::DASHCommonAttributesElements() :
    parX( 1 ),
    parY( 1 ),
    frameRate( -1 )
{
}

DASHCommonAttributesElements::~DASHCommonAttributesElements()
{
    vlc_delete_all( this->contentProtections );
    vlc_delete_all( this->accessibilities );
    vlc_delete_all( this->ratings );
    vlc_delete_all( this->viewpoints );
}

int     DASHCommonAttributesElements::getParX                 () const
{
    return this->parX;
}

void    DASHCommonAttributesElements::setParX( int parX )
{
    if ( parX > 0 )
        this->parX = parX;
}

int         DASHCommonAttributesElements::getParY                 () const
{
    return this->parY;
}

void        DASHCommonAttributesElements::setParY( int parY )
{
    if ( parY > 0 )
        this->setParY( parY );
}

int                 DASHCommonAttributesElements::getFrameRate            () const
{
    return this->frameRate;
}

void            DASHCommonAttributesElements::setFrameRate( int frameRate )
{
    if ( frameRate > 0 )
        this->frameRate = frameRate;
}

const std::list<std::string>&   DASHCommonAttributesElements::getNumberOfChannels() const
{
    return this->channels;
}

void    DASHCommonAttributesElements::addChannel( const std::string &channel )
{
    if ( channel.empty() == false )
        this->channels.push_back( channel );
}

const std::list<int>&   DASHCommonAttributesElements::getSamplingRates() const
{
    return this->sampleRates;
}

void    DASHCommonAttributesElements::addSampleRate( int sampleRate )
{
    if ( sampleRate > 0 )
        this->sampleRates.push_back( sampleRate );
}

const std::list<ContentDescription*> &DASHCommonAttributesElements::getContentProtections() const
{
    return this->contentProtections;
}

void DASHCommonAttributesElements::addContentProtection(ContentDescription *desc)
{
    if ( desc != NULL )
        this->contentProtections.push_back( desc );
}

const std::list<ContentDescription*> &DASHCommonAttributesElements::getAccessibilities() const
{
    return this->accessibilities;
}

void DASHCommonAttributesElements::addAccessibility(ContentDescription *desc)
{
    if ( desc )
        this->accessibilities.push_back( desc );
}

const std::list<ContentDescription*> &DASHCommonAttributesElements::getRatings() const
{
    return this->ratings;
}

void DASHCommonAttributesElements::addRating(ContentDescription *desc)
{
    if ( desc )
        this->ratings.push_back( desc );
}

const std::list<ContentDescription*> &DASHCommonAttributesElements::getViewpoints() const
{
    return this->viewpoints;
}

void DASHCommonAttributesElements::addViewpoint(ContentDescription *desc)
{
    if ( desc )
        this->viewpoints.push_back( desc );
}
