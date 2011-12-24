/*
 * MPDManagerFactory.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Apr 20, 2011
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

#include "mpd/MPDManagerFactory.h"

using namespace dash::mpd;
using namespace dash::xml;

IMPDManager* MPDManagerFactory::create( Node *root )
{
    BasicCMParser parser(root);

    if ( parser.parse() == false )
        return new NullManager();

    Profile profile = parser.getMPD()->getProfile();
    switch( profile )
    {
        case mpd::Basic:    return new NullManager();
        case mpd::BasicCM:  return new BasicCMManager( parser.getMPD() );
        case mpd::Full2011: return new NullManager();
        case mpd::NotValid: return new NullManager();

        default:            return new NullManager();
    }
}
