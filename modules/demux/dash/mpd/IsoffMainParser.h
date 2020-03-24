/*
 * IsoffMainParser.h
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

#ifndef ISOFFMAINPARSER_H_
#define ISOFFMAINPARSER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../../adaptive/playlist/SegmentInfoCommon.h"
#include "Profile.hpp"

#include <cstdlib>

#include <vlc_common.h>

namespace adaptive
{
    namespace playlist
    {
        class SegmentInformation;
        class MediaSegmentTemplate;
    }
    namespace xml
    {
        class Node;
    }
}

namespace dash
{
    namespace mpd
    {
        class Period;
        class AdaptationSet;
        class MPD;

        using namespace adaptive::playlist;
        using namespace adaptive;

        class IsoffMainParser
        {
            public:
                IsoffMainParser             (xml::Node *root, vlc_object_t *p_object,
                                             stream_t *p_stream, const std::string &);
                virtual ~IsoffMainParser    ();
                MPD *   parse();

            private:
                mpd::Profile getProfile     () const;
                void    parseMPDBaseUrl     (MPD *, xml::Node *);
                void    parseMPDAttributes  (MPD *, xml::Node *);
                void    parseAdaptationSets (MPD *, xml::Node *periodNode, Period *period);
                void    parseRepresentations(MPD *, xml::Node *adaptationSetNode, AdaptationSet *adaptationSet);
                void    parseInitSegment    (xml::Node *, Initializable<Segment> *, SegmentInformation *);
                void    parseTimeline       (xml::Node *, MediaSegmentTemplate *);
                void    parsePeriods        (MPD *, xml::Node *);
                size_t  parseSegmentInformation(MPD *, xml::Node *, SegmentInformation *, uint64_t *);
                size_t  parseSegmentBase    (MPD *, xml::Node *, SegmentInformation *);
                size_t  parseSegmentList    (MPD *, xml::Node *, SegmentInformation *);
                size_t  parseSegmentTemplate(MPD *, xml::Node *, SegmentInformation *);
                void    parseProgramInformation(xml::Node *, MPD *);

                xml::Node       *root;
                vlc_object_t    *p_object;
                stream_t        *p_stream;
                std::string      playlisturl;
        };
    }
}

#endif /* ISOFFMAINPARSER_H_ */
