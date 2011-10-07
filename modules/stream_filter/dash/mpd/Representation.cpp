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

Representation::Representation  (std::map<std::string, std::string>  attributes)
{
    this->attributes        = attributes;
    this->contentProtection = NULL;
    this->trickModeType     = NULL;
    this->segmentInfo       = NULL;
}
Representation::~Representation ()
{
    delete(this->segmentInfo);
    delete(this->contentProtection);
    delete(this->trickModeType);
}

std::string         Representation::getFrameRate            () throw(AttributeNotPresentException)
{
    if(this->attributes.find("frameRate") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["frameRate"];

}
std::string         Representation::getSamplingRate         () throw(AttributeNotPresentException)
{
    if(this->attributes.find("samplingRate") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["samplingRate"];

}
std::string         Representation::getDependencyId         () throw(AttributeNotPresentException)
{
    if(this->attributes.find("dependencyId") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["dependencyId"];

}
std::string         Representation::getId                   () throw(AttributeNotPresentException)
{
    if(this->attributes.find("id") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["id"];

}
std::string         Representation::getLang                 () throw(AttributeNotPresentException)
{
    if(this->attributes.find("lang") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["lang"];

}
std::string         Representation::getParX                 () throw(AttributeNotPresentException)
{
    if(this->attributes.find("parx") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["parx"];

}
std::string         Representation::getParY                 () throw(AttributeNotPresentException)
{
    if(this->attributes.find("pary") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["pary"];

}
std::string         Representation::getHeight               () throw(AttributeNotPresentException)
{
    if(this->attributes.find("height") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["height"];

}
std::string         Representation::getWidth                () throw(AttributeNotPresentException)
{
    if(this->attributes.find("width") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["width"];

}
std::string         Representation::getBandwidth            () throw(AttributeNotPresentException)
{
    if(this->attributes.find("bandwidth") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["bandwidth"];

}
std::string         Representation::getNumberOfChannels     () throw(AttributeNotPresentException)
{
    if(this->attributes.find("numberOfChannels") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["numberOfChannels"];

}
SegmentInfo*        Representation::getSegmentInfo          () throw(ElementNotPresentException)
{
    if(this->segmentInfo == NULL)
        throw ElementNotPresentException();

    return this->segmentInfo;
}
TrickModeType*      Representation::getTrickModeType        () throw(ElementNotPresentException)
{
    if(this->segmentInfo == NULL)
        throw ElementNotPresentException();

    return this->trickModeType;
}
ContentProtection*  Representation::getContentProtection    () throw(ElementNotPresentException)
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
