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

CommonAttributesElements::CommonAttributesElements( const std::map<std::string, std::string>& attributes ) :
    attributes( attributes ),
    contentProtection( NULL )
{
}

CommonAttributesElements::~CommonAttributesElements()
{
    delete this->contentProtection;
}

std::string         CommonAttributesElements::getWidth                () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("width");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}

std::string         CommonAttributesElements::getHeight               () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("height");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}

std::string         CommonAttributesElements::getParX                 () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("parx");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}

std::string         CommonAttributesElements::getParY                 () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("pary");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}

std::string         CommonAttributesElements::getLang                 () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("lang");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}

std::string         CommonAttributesElements::getFrameRate            () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("frameRate");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}

std::string         CommonAttributesElements::getNumberOfChannels     () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("numberOfChannels");
    if( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}

std::string         CommonAttributesElements::getSamplingRate         () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("samplingRate");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}

ContentProtection*  CommonAttributesElements::getContentProtection    () const throw(ElementNotPresentException)
{
    if(this->contentProtection == NULL)
        throw ElementNotPresentException();

    return this->contentProtection;
}
