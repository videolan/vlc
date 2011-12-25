/*
 * BasicCMParser.h
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

#ifndef BASICCMPARSER_H_
#define BASICCMPARSER_H_

#include "xml/Node.h"
#include "xml/DOMHelper.h"
#include "mpd/IMPDParser.h"
#include "mpd/MPD.h"
#include "mpd/Period.h"
#include "mpd/Group.h"
#include "mpd/Representation.h"
#include "mpd/BaseUrl.h"
#include "mpd/SegmentInfo.h"
#include "mpd/Segment.h"

namespace dash
{
    namespace mpd
    {
        class BasicCMParser : public IMPDParser
        {
            public:
                BasicCMParser           (dash::xml::Node *root);
                virtual ~BasicCMParser  ();

                bool    parse  ();
                MPD*    getMPD ();

            private:
                void    handleDependencyId( Representation* rep, const Group* group, const std::string& dependencyId );

            private:
                dash::xml::Node *root;
                MPD             *mpd;

                bool    setMPD              ();
                void    setPeriods          (dash::xml::Node *root);
                void    setGroups           (dash::xml::Node *root, Period *period);
                void    setRepresentations  (dash::xml::Node *root, Group *group);
                bool    setSegmentInfo      (dash::xml::Node *root, Representation *rep);
                void    setInitSegment      (dash::xml::Node *root, SegmentInfo *info);
                bool    setSegments         (dash::xml::Node *root, SegmentInfo *info);
                void    setMPDBaseUrl       (dash::xml::Node *root);
                void    parseContentDescriptor( xml::Node *node, const std::string &name,
                                                void (CommonAttributesElements::*addPtr)(ContentDescription*),
                                                CommonAttributesElements *self ) const;
                bool    parseCommonAttributesElements( dash::xml::Node *node, CommonAttributesElements *common ) const;
                bool    parseSegment( Segment *seg, const std::map<std::string, std::string> &attr );
                ProgramInformation*     parseProgramInformation();
        };
    }
}

#endif /* BASICCMPARSER_H_ */
