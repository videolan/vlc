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
#include "mpd/ICanonicalUrl.hpp"
#include "http/Chunk.h"

namespace dash
{
    namespace mpd
    {
        class Representation;
        class SubSegment;

        class ISegment : public ICanonicalUrl
        {
            public:
                ISegment(const ICanonicalUrl *parent);
                virtual ~ISegment(){}
                /**
                 *  @return true if the segment should be dropped after being read.
                 *          That is basically true when using an Url, and false
                 *          when using an UrlTemplate
                 */
                virtual bool                            isSingleShot    () const;
                virtual void                            done            ();
                virtual dash::http::Chunk*              toChunk         ();
                virtual void                            setByteRange    (size_t start, size_t end);
                virtual void                            setStartTime    (mtime_t ztime);
                virtual mtime_t                         getStartTime    () const;
                virtual size_t                          getOffset       () const;
                virtual std::vector<ISegment*>          subSegments     () = 0;
                virtual std::string                     toString        () const;
                virtual Representation*                 getRepresentation() const = 0;
                virtual bool                            contains        (size_t byte) const;
                int                                     getClassId      () const;

                static const int CLASSID_ISEGMENT = 0;

            protected:
                size_t                  startByte;
                size_t                  endByte;
                mtime_t                 startTime;
                std::string             debugName;
                int                     classId;

                class SegmentChunk : public dash::http::Chunk
                {
                    public:
                        SegmentChunk(ISegment *segment, const std::string &url);
                        virtual void onDownload(void *, size_t);

                    protected:
                        ISegment *segment;
                };

                virtual dash::http::Chunk * getChunk(const std::string &);
        };

        class Segment : public ISegment
        {
            public:
                Segment( Representation *parent );
                explicit Segment( ICanonicalUrl *parent );
                ~Segment();
                virtual void setSourceUrl( const std::string &url );
                virtual Url getUrlSegment() const; /* impl */
                virtual dash::http::Chunk* toChunk();
                virtual std::vector<ISegment*> subSegments();
                virtual Representation* getRepresentation() const;
                virtual std::string toString() const;
                virtual void addSubSegment(SubSegment *);
                static const int CLASSID_SEGMENT = 1;

            protected:
                Representation* parentRepresentation;
                std::vector<SubSegment *> subsegments;
                std::string sourceUrl;
                int size;
        };

        class InitSegment : public Segment
        {
            public:
                InitSegment( Representation *parent );
                static const int CLASSID_INITSEGMENT = 2;
        };

        class IndexSegment : public Segment
        {
            public:
                IndexSegment( Representation *parent );
                static const int CLASSID_INDEXSEGMENT = 3;

            protected:
                class IndexSegmentChunk : public SegmentChunk
                {
                    public:
                        IndexSegmentChunk(ISegment *segment, const std::string &);
                        virtual void onDownload(void *, size_t);
                };

                virtual dash::http::Chunk * getChunk(const std::string &);
        };

        class SubSegment : public ISegment
        {
            public:
                SubSegment(Segment *, size_t start, size_t end);
                virtual Url getUrlSegment() const; /* impl */
                virtual std::vector<ISegment*> subSegments();
                virtual Representation* getRepresentation() const;
                static const int CLASSID_SUBSEGMENT = 4;
            private:
                Segment *parent;
        };
    }
}

#endif /* SEGMENT_H_ */
