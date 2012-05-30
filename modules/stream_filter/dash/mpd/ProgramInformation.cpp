/*
 * ProgramInformation.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#include "ProgramInformation.h"

using namespace dash::mpd;

const std::string &ProgramInformation::getSource() const
{
    return this->source;
}

void ProgramInformation::setSource(const std::string &source)
{
    if ( source.empty() == false )
        this->source = source;
}

const std::string &ProgramInformation::getCopyright() const
{
    return this->copyright;
}

void ProgramInformation::setCopyright(const std::string &copyright)
{
    if ( copyright.empty() == false )
        this->copyright = copyright;
}

void ProgramInformation::setMoreInformationUrl(const std::string &url)
{
    if ( url.empty() == false )
        this->moreInformationUrl = url;
}

const std::string &ProgramInformation::getTitle() const
{
    return this->title;
}

void        ProgramInformation::setTitle                (const std::string &title)
{
    if ( title.empty() == false )
        this->title = title;
}
