/*
 * MPD.cpp
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

#include "MPD.h"

using namespace dash::mpd;
using namespace dash::exception;

MPD::MPD    (std::map<std::string, std::string> attributes)
{
    this->attributes    = attributes;
    this->programInfo   = NULL;
}
MPD::MPD    ()
{

}
MPD::~MPD   ()
{
    for(size_t i = 0; i < this->periods.size(); i++)
        delete(this->periods.at(i));

    for(size_t i = 0; i < this->baseUrls.size(); i++)
        delete(this->baseUrls.at(i));

    delete(this->programInfo);
}

std::vector<Period*>    MPD::getPeriods             ()
{
    return this->periods;
}
std::vector<BaseUrl*>   MPD::getBaseUrls            ()
{
    return this->baseUrls;
}
std::string             MPD::getMinBufferTime       () throw(AttributeNotPresentException)
{
    if(this->attributes.find("minBufferTime") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["minBufferTime"];
}
std::string             MPD::getType                () throw(AttributeNotPresentException)
{
    if(this->attributes.find("type") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["type"];
}
std::string             MPD::getDuration            () throw(AttributeNotPresentException)
{
    if(this->attributes.find("mediaPresentationDuration") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["mediaPresentationDuration"];
}
ProgramInformation*     MPD::getProgramInformation  () throw(ElementNotPresentException)
{
    if(this->programInfo == NULL)
        throw ElementNotPresentException();

    return this->programInfo;
}
void                    MPD::addBaseUrl             (BaseUrl *url)
{
    this->baseUrls.push_back(url);
}
void                    MPD::addPeriod              (Period *period)
{
    this->periods.push_back(period);
}
void                    MPD::setProgramInformation  (ProgramInformation *progInfo)
{
    this->programInfo = progInfo;
}
