/*
 * MPDFactory.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2012
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

#include "MPDFactory.h"

using namespace dash::xml;
using namespace dash::mpd;

MPD* MPDFactory::create             (dash::xml::Node *root, stream_t *p_stream, Profile profile)
{
    switch(profile)
    {
        case dash::mpd::Full2011:
        case dash::mpd::Basic:
        case dash::mpd::BasicCM:    return MPDFactory::createBasicCMMPD(root, p_stream);
        case dash::mpd::IsoffMain:  return MPDFactory::createIsoffMainMPD(root, p_stream);

        default: return NULL;
    }
}
MPD* MPDFactory::createBasicCMMPD    (dash::xml::Node *root, stream_t *p_stream)
{
    dash::mpd::BasicCMParser mpdParser(root, p_stream);

    if(mpdParser.parse() == false || mpdParser.getMPD() == NULL)
        return NULL;
    mpdParser.getMPD()->setProfile( dash::mpd::BasicCM );
    return mpdParser.getMPD();
}
MPD* MPDFactory::createIsoffMainMPD  (dash::xml::Node *root, stream_t *p_stream)
{
    dash::mpd::IsoffMainParser mpdParser(root, p_stream);

    if(mpdParser.parse() == false || mpdParser.getMPD() == NULL)
        return NULL;
    mpdParser.getMPD()->setProfile( dash::mpd::IsoffMain );
    return mpdParser.getMPD();
}
