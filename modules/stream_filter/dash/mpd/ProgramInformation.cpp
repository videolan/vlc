/*
 * ProgramInformation.cpp
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

#include "ProgramInformation.h"

using namespace dash::mpd;
using namespace dash::exception;

ProgramInformation::ProgramInformation  (std::map<std::string, std::string> attr)
{
    this->attributes    = attr;
}
ProgramInformation::~ProgramInformation ()
{
}

std::string ProgramInformation::getTitle                () throw(ElementNotPresentException)
{
    if(this->title.empty())
        throw ElementNotPresentException();

    return this->title;
}
std::string ProgramInformation::getCopyright            () throw(ElementNotPresentException)
{
    if(this->copyright.empty())
        throw ElementNotPresentException();

    return this->copyright;
}
std::string ProgramInformation::getSource               () throw(ElementNotPresentException)
{
    if(this->source.empty())
        throw ElementNotPresentException();

    return this->source;
}
std::string ProgramInformation::getMoreInformationUrl   () throw(AttributeNotPresentException)
{
    if(this->attributes.find("moreInformationURL") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["moreInformationURL"];

}
void        ProgramInformation::setTitle                (std::string title)
{
    this->title = title;
}
void        ProgramInformation::setCopyright            (std::string copyright)
{
    this->copyright = copyright;
}
void        ProgramInformation::setSource               (std::string source)
{
    this->source = source;
}
