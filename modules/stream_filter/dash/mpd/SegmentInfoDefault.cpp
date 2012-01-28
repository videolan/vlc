/*****************************************************************************
 * SegmentInfoDefault.cpp: Implement the SegmentInfoDefault element.
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 * $Id$
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

#include "SegmentInfoDefault.h"

using namespace dash::mpd;

SegmentInfoDefault::SegmentInfoDefault()
{
}

const std::string&      SegmentInfoDefault::getSourceURLTemplatePeriod() const
{
    return this->sourceURLTemplatePeriod;
}

void    SegmentInfoDefault::setSourceURLTemplatePediod( const std::string &url )
{
    if ( url.empty() == false )
        this->sourceURLTemplatePeriod = url;
}

int     SegmentInfoDefault::getIndexTemplate() const
{
    return this->indexTemplate;
}

void    SegmentInfoDefault::setIndexTemplate( int indexTpl )
{
    if ( indexTpl >= 0 )
        this->indexTemplate = indexTpl;
}
