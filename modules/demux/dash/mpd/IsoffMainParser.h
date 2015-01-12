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

#include "xml/Node.h"
#include "mpd/IMPDParser.h"
#include "mpd/AdaptationSet.h"
#include "mpd/BaseUrl.h"
#include "mpd/SegmentBase.h"
#include "mpd/SegmentList.h"
#include "mpd/Segment.h"

#include <cstdlib>
#include <sstream>

namespace dash
{
    namespace mpd
    {
        class IsoffMainParser : public IMPDParser
        {
            public:
                IsoffMainParser             (dash::xml::Node *root, stream_t *p_stream);
                virtual ~IsoffMainParser    ();

                bool    parse  (Profile profile);
                void    print  ();

            private:
                void    setMPDAttributes    ();
                void    setAdaptationSets   (dash::xml::Node *periodNode, Period *period);
                void    setRepresentations  (dash::xml::Node *adaptationSetNode, AdaptationSet *adaptationSet);
                void    parseInitSegment    (dash::xml::Node *, Initializable<Segment> *);
                void    parseTimeline       (dash::xml::Node *, MediaSegmentTemplate *);
                void    parsePeriods        (dash::xml::Node *);
                size_t  parseSegmentInformation(dash::xml::Node *, SegmentInformation *);
                void    parseSegmentBase    (dash::xml::Node *, SegmentInformation *);
                size_t  parseSegmentList    (dash::xml::Node *, SegmentInformation *);
                size_t  parseSegmentTemplate(dash::xml::Node *, SegmentInformation *);
                void    parseProgramInformation(dash::xml::Node *, MPD *);
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
