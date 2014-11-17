/*
 * Profile.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2014 VideoLAN Authors
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

#include "Profile.hpp"

using namespace dash::mpd;

Profile::Name Profile::getNameByURN(std::string urn)
{
    struct
    {
        const Name name;
        const char * urn;
    }
    urnmap[] =
    {
        { Full,         "urn:mpeg:dash:profile:full:2011" },
        { ISOOnDemand,  "urn:mpeg:dash:profile:isoff-on-demand:2011" },
        { ISOOnDemand,  "urn:mpeg:mpegB:profile:dash:isoff-basic-on-demand:cm" },
        { ISOOnDemand,  "urn:mpeg:dash:profile:isoff-ondemand:2011" },
        { ISOMain,      "urn:mpeg:dash:profile:isoff-main:2011" },
        { ISOLive,      "urn:mpeg:dash:profile:isoff-live:2011" },
        { MPEG2TSMain,  "urn:mpeg:dash:profile:mp2t-main:2011" },
        { MPEG2TSSimple,"urn:mpeg:dash:profile:mp2t-simple:2011" },
        { Unknown,      "" },
    };

    for( int i=0; urnmap[i].name != Unknown; i++ )
    {
        if ( urn == urnmap[i].urn )
            return urnmap[i].name;
    }
    return Unknown;
}
