/*
 * Segment.h
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

#ifndef SEGMENT_H_
#define SEGMENT_H_

#include <string>
#include <sstream>
#include <vector>
#include "mpd/BaseUrl.h"
#include "http/Chunk.h"

namespace dash
{
    namespace mpd
    {
        class Representation;
        class Segment
        {
            public:
                Segment( const Representation *parent );
                virtual ~Segment(){}
                virtual std::string getSourceUrl() const;
                virtual void        setSourceUrl( const std::string &url );
                /**
                 *  @return true if the segment should be dropped after being read.
                 *          That is basically true when using an Url, and false
                 *          when using an UrlTemplate
                 */
                virtual bool                            isSingleShot    () const;
                virtual void                            done            ();
                virtual void                            addBaseUrl      (BaseUrl *url);
                virtual const std::vector<BaseUrl *>&   getBaseUrls     () const;
                virtual void                            setByteRange    (int start, int end);
                virtual int                             getStartByte    () const;
                virtual int                             getEndByte      () const;
                virtual dash::http::Chunk*              toChunk         ();
                const Representation*                   getParentRepresentation() const;
                virtual int                             getSize() const;

            protected:
                std::string             sourceUrl;
                std::vector<BaseUrl *>  baseUrls;
                int                     startByte;
                int                     endByte;
                const Representation*   parentRepresentation;
                int                     size;
        };
    }
}

#endif /* SEGMENT_H_ */
