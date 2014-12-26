/*
 * IMPDParser.h
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

#ifndef IMPDPARSER_H_
#define IMPDPARSER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mpd/MPD.h"
#include "xml/DOMParser.h"

#include <vlc_common.h>

namespace dash
{
    namespace mpd
    {
        class IMPDParser
        {
            public:
                IMPDParser(dash::xml::Node *, MPD*, stream_t*, Representation*);
                virtual ~IMPDParser(){}
                virtual bool    parse  (Profile profile) = 0;
                virtual MPD*    getMPD ();
                virtual void    setMPDBaseUrl(dash::xml::Node *root);
                virtual void    setAdaptationSets(dash::xml::Node *periodNode, Period *period) = 0;

            protected:
                dash::xml::Node *root;
                MPD             *mpd;
                stream_t        *p_stream;
                Representation  *currentRepresentation;
        };
    }
}

#endif /* IMPDPARSER_H_ */
