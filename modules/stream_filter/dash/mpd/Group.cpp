/*
 * Group.cpp
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

#include "Group.h"

using namespace dash::mpd;
using namespace dash::exception;

Group::Group    ( const std::map<std::string, std::string>&  attributes) :
    attributes( attributes ),
    contentProtection( NULL ),
    accessibility( NULL ),
    viewpoint( NULL ),
    rating( NULL )
{
}

Group::~Group   ()
{
    for(size_t i = 1; i < this->representations.size(); i++)
        delete(this->representations.at(i));

    delete(this->contentProtection);
    delete(this->rating);
    delete(this->viewpoint);
    delete(this->accessibility);
}

std::string                     Group::getSubSegmentAlignment   () throw(AttributeNotPresentException)
{
    if(this->attributes.find("subsegmentAlignmentFlag") == this->attributes.end())
        throw AttributeNotPresentException();

    return this->attributes["subsegmentAlignmentFlag"];
}

Viewpoint*                      Group::getViewpoint             () throw(ElementNotPresentException)
{
    if(this->viewpoint == NULL)
        throw ElementNotPresentException();

    return this->viewpoint;
}

Rating*                         Group::getRating                () throw(ElementNotPresentException)
{
    if(this->rating == NULL)
        throw ElementNotPresentException();

    return this->rating;
}

Accessibility*                  Group::getAccessibility         () throw(ElementNotPresentException)
{
    if(this->accessibility == NULL)
        throw ElementNotPresentException();

    return this->accessibility;
}

std::vector<Representation*>    Group::getRepresentations       ()
{
    return this->representations;
}

const Representation *Group::getRepresentationById(const std::string &id) const
{
    std::vector<Representation*>::const_iterator    it = this->representations.begin();
    std::vector<Representation*>::const_iterator    end = this->representations.end();

    while ( it != end )
    {
        if ( (*it)->getId() == id )
            return *it;
        ++it;
    }
    return NULL;
}

void                            Group::addRepresentation        (Representation *rep)
{
    this->representations.push_back(rep);
}

void                            Group::setRating                (Rating *rating)
{
    this->rating = rating;
}

void                            Group::setContentProtection     (ContentProtection *protection)
{
    this->contentProtection = protection;
}

void                            Group::setAccessibility         (Accessibility *accessibility)
{
    this->accessibility = accessibility;
}

void                            Group::setViewpoint             (Viewpoint *viewpoint)
{
    this->viewpoint = viewpoint;
}
