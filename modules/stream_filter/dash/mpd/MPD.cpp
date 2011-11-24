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

MPD::MPD    (const AttributesMap& attributes) : attributes( attributes ),
    programInfo( NULL )
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

const std::vector<Period*>&    MPD::getPeriods             () const
{
    return this->periods;
}

const std::vector<BaseUrl*>&   MPD::getBaseUrls            () const
{
    return this->baseUrls;
}

const std::string&             MPD::getMinBufferTime       () const throw(AttributeNotPresentException)
{
    AttributesMap::const_iterator     it = this->attributes.find("minBufferTime");
    if( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;
}

const std::string&             MPD::getType                () const throw(AttributeNotPresentException)
{
    AttributesMap::const_iterator     it = this->attributes.find( "type" );
    if( it == this->attributes.end() )
        throw AttributeNotPresentException();

    return it->second;
}
const std::string&             MPD::getDuration            () const throw(AttributeNotPresentException)
{
    AttributesMap::const_iterator     it = this->attributes.find("mediaPresentationDuration");

    if( it == this->attributes.end())
        throw AttributeNotPresentException();

    return it->second;
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
