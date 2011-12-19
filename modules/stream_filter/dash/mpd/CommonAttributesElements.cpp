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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "CommonAttributesElements.h"

using namespace dash::mpd;
using namespace dash::exception;

CommonAttributesElements::CommonAttributesElements() :
    width( -1 ),
    height( -1 ),
    parX( 1 ),
    parY( 1 ),
    frameRate( -1 ),
    contentProtection( NULL )
{
}

CommonAttributesElements::~CommonAttributesElements()
{
    delete this->contentProtection;
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

ContentProtection*  CommonAttributesElements::getContentProtection    () const throw(ElementNotPresentException)
{
    if(this->contentProtection == NULL)
        throw ElementNotPresentException();

    return this->contentProtection;
}
