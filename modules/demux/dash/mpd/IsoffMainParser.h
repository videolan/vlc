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

#include "../adaptative/playlist/SegmentInfoCommon.h"
#include "Profile.hpp"

#include <cstdlib>
#include <sstream>

#include <vlc_common.h>

namespace adaptative
{
    namespace playlist
    {
        class SegmentInformation;
        class MediaSegmentTemplate;
    }
}

namespace dash
{
    namespace xml
    {
        class Node;
    }

    namespace mpd
    {
        class Period;
        class AdaptationSet;
        class MPD;

        using namespace adaptative::playlist;

        class IsoffMainParser
        {
            public:
                IsoffMainParser             (xml::Node *root, stream_t *p_stream, std::string &);
                virtual ~IsoffMainParser    ();

                bool            parse  (Profile profile);
                virtual MPD*    getMPD ();
                virtual void    setMPDBaseUrl(xml::Node *root);

            private:
                void    setMPDAttributes    ();
                void    setAdaptationSets   (xml::Node *periodNode, Period *period);
                void    setRepresentations  (xml::Node *adaptationSetNode, AdaptationSet *adaptationSet);
                void    parseInitSegment    (xml::Node *, Initializable<Segment> *, SegmentInformation *);
                void    parseTimeline       (xml::Node *, MediaSegmentTemplate *);
                void    parsePeriods        (xml::Node *);
                size_t  parseSegmentInformation(xml::Node *, SegmentInformation *);
                size_t  parseSegmentBase    (xml::Node *, SegmentInformation *);
                size_t  parseSegmentList    (xml::Node *, SegmentInformation *);
                size_t  parseSegmentTemplate(xml::Node *, SegmentInformation *);
                void    parseProgramInformation(xml::Node *, MPD *);

                xml::Node       *root;
                MPD             *mpd;
                stream_t        *p_stream;
                std::string      playlisturl;
        };

        class IsoTime
        {
            public:
                IsoTime(const std::string&);
                operator mtime_t() const;

            private:
                mtime_t time;
        };

        class UTCTime
        {
            public:
                UTCTime(const std::string&);
                operator mtime_t() const;

            private:
                mtime_t time;
        };

        template<typename T> class Integer
        {
            public:
                Integer(const std::string &str)
                {
                    try
                    {
                        std::istringstream in(str);
                        in >> value;
                    } catch (int) {
                        value = 0;
                    }
                }

                operator T() const
                {
                    return value;
                }

            private:
                T value;
        };
    }
}

#endif /* ISOFFMAINPARSER_H_ */
