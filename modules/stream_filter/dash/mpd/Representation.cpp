/*
 * Representation.cpp
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

#include "Representation.h"

using namespace dash::mpd;
using namespace dash::exception;

Representation::Representation  (const std::map<std::string, std::string>&  attributes) :
    attributes( attributes ),
    segmentInfo( NULL ),
    trickModeType( NULL ),
    contentProtection( NULL )
{
}

Representation::~Representation ()
{
    delete(this->segmentInfo);
    delete(this->contentProtection);
    delete(this->trickModeType);
}

std::string         Representation::getFrameRate            () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("frameRate");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getSamplingRate         () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("samplingRate");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getDependencyId         () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("dependencyId");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getId                   () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("id");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getLang                 () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("lang");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getParX                 () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("parx");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getParY                 () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("pary");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getHeight               () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("height");
    if ( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getWidth                () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("width");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getBandwidth            () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("bandwidth");
    if ( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;

}
std::string         Representation::getNumberOfChannels     () const throw(AttributeNotPresentException)
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find("numberOfChannels");
    if( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;

}
SegmentInfo*        Representation::getSegmentInfo          () const throw(ElementNotPresentException)
{
    if(this->segmentInfo == NULL)
        throw ElementNotPresentException();

    return this->segmentInfo;
}
TrickModeType*      Representation::getTrickModeType        () const throw(ElementNotPresentException)
{
    if(this->segmentInfo == NULL)
        throw ElementNotPresentException();

    return this->trickModeType;
}
ContentProtection*  Representation::getContentProtection    () const throw(ElementNotPresentException)
{
    if(this->contentProtection == NULL)
        throw ElementNotPresentException();

    return this->contentProtection;
}
void                Representation::setTrickModeType        (TrickModeType *trickModeType)
{
    this->trickModeType = trickModeType;
}
void                Representation::setContentProtection    (ContentProtection *protection)
{
    this->contentProtection = protection;
}
void                Representation::setSegmentInfo          (SegmentInfo *info)
{
    this->segmentInfo = info;
}
