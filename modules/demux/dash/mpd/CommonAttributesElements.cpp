/*****************************************************************************
 * CommonAttributesElements.cpp: Defines the common attributes and elements
 *                               for some Dash elements.
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <beauze.h@gmail.com>
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

#include "CommonAttributesElements.h"

#include "mpd/ContentDescription.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

using namespace dash::mpd;

CommonAttributesElements::CommonAttributesElements() :
    width( -1 ),
    height( -1 ),
    parX( 1 ),
    parY( 1 ),
    frameRate( -1 )
{
}

CommonAttributesElements::~CommonAttributesElements()
{
    vlc_delete_all( this->contentProtections );
    vlc_delete_all( this->accessibilities );
    vlc_delete_all( this->ratings );
    vlc_delete_all( this->viewpoints );
}

const std::string&      CommonAttributesElements::getMimeType() const
{
    return this->mimeType;
}

void                    CommonAttributesElements::setMimeType( const std::string &mimeType )
{
    this->mimeType = mimeType;
}

int     CommonAttributesElements::getWidth                () const
{
    return this->width;
}

void    CommonAttributesElements::setWidth( int width )
{
    if ( width > 0 )
        this->width = width;
}

int     CommonAttributesElements::getHeight               () const
{
    return this->height;
}

void    CommonAttributesElements::setHeight( int height )
{
    if ( height > 0 )
        this->height = height;
}

int     CommonAttributesElements::getParX                 () const
{
    return this->parX;
}

void    CommonAttributesElements::setParX( int parX )
{
    if ( parX > 0 )
        this->parX = parX;
}

int         CommonAttributesElements::getParY                 () const
{
    return this->parY;
}

void        CommonAttributesElements::setParY( int parY )
{
    if ( parY > 0 )
        this->setParY( parY );
}

const std::list<std::string>& CommonAttributesElements::getLang() const
{
    return this->lang;
}

void    CommonAttributesElements::addLang( const std::string &lang )
{
    if ( lang.empty() == false )
        this->lang.push_back( lang );
}

int                 CommonAttributesElements::getFrameRate            () const
{
    return this->frameRate;
}

void            CommonAttributesElements::setFrameRate( int frameRate )
{
    if ( frameRate > 0 )
        this->frameRate = frameRate;
}

const std::list<std::string>&   CommonAttributesElements::getNumberOfChannels() const
{
    return this->channels;
}

void    CommonAttributesElements::addChannel( const std::string &channel )
{
    if ( channel.empty() == false )
        this->channels.push_back( channel );
}

const std::list<int>&   CommonAttributesElements::getSamplingRates() const
{
    return this->sampleRates;
}

void    CommonAttributesElements::addSampleRate( int sampleRate )
{
    if ( sampleRate > 0 )
        this->sampleRates.push_back( sampleRate );
}

const std::list<ContentDescription*> &CommonAttributesElements::getContentProtections() const
{
    return this->contentProtections;
}

void CommonAttributesElements::addContentProtection(ContentDescription *desc)
{
    if ( desc != NULL )
        this->contentProtections.push_back( desc );
}

const std::list<ContentDescription*> &CommonAttributesElements::getAccessibilities() const
{
    return this->accessibilities;
}

void CommonAttributesElements::addAccessibility(ContentDescription *desc)
{
    if ( desc )
        this->accessibilities.push_back( desc );
}

const std::list<ContentDescription*> &CommonAttributesElements::getRatings() const
{
    return this->ratings;
}

void CommonAttributesElements::addRating(ContentDescription *desc)
{
    if ( desc )
        this->ratings.push_back( desc );
}

const std::list<ContentDescription*> &CommonAttributesElements::getViewpoints() const
{
    return this->viewpoints;
}

void CommonAttributesElements::addViewpoint(ContentDescription *desc)
{
    if ( desc )
        this->viewpoints.push_back( desc );
}
