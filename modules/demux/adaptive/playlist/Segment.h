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
#include "ICanonicalUrl.hpp"
#include "../http/Chunk.h"
#include "../encryption/CommonEncryption.hpp"
#include "../tools/Properties.hpp"
#include "Time.hpp"

namespace adaptive
{
    class SharedResources;

    namespace http
    {
        class AbstractConnectionManager;
    }

    namespace playlist
    {
        class BaseRepresentation;
        class SubSegment;
        class SegmentChunk;

        using namespace http;
        using namespace encryption;

        class ISegment : public ICanonicalUrl
        {
            public:
                ISegment(const ICanonicalUrl *parent);
                virtual ~ISegment();
                /**
                 *  @return true if the segment should be dropped after being read.
                 *          That is basically true when using an Url, and false
                 *          when using an UrlTemplate
                 */
                virtual SegmentChunk*                   toChunk         (SharedResources *, AbstractConnectionManager *,
                                                                         size_t, BaseRepresentation *);
                virtual SegmentChunk*                   createChunk     (AbstractChunkSource *, BaseRepresentation *) = 0;
                virtual void                            setByteRange    (size_t start, size_t end);
                virtual void                            setSequenceNumber(uint64_t);
                virtual uint64_t                        getSequenceNumber() const;
                virtual bool                            isTemplate      () const;
                virtual size_t                          getOffset       () const;
                virtual std::vector<ISegment*>          subSegments     () = 0;
                virtual void                            addSubSegment   (SubSegment *) = 0;
                virtual void                            debug           (vlc_object_t *,int = 0) const;
                virtual bool                            contains        (size_t byte) const;
                virtual int                             compare         (ISegment *) const;
                void                                    setEncryption   (CommonEncryption &);
                int                                     getClassId      () const;
                Property<stime_t>       startTime;
                Property<stime_t>       duration;
                bool                    discontinuity;

                static const int CLASSID_ISEGMENT = 0;

            protected:
                virtual bool                            prepareChunk    (SharedResources *,
                                                                         SegmentChunk *,
                                                                         BaseRepresentation *);
                CommonEncryption        encryption;
                size_t                  startByte;
                size_t                  endByte;
                std::string             debugName;
                int                     classId;
                bool                    templated;
                uint64_t                sequence;
                static const int        SEQUENCE_INVALID;
                static const int        SEQUENCE_FIRST;
        };

        class Segment : public ISegment
        {
            public:
                Segment( ICanonicalUrl *parent );
                ~Segment();
                virtual SegmentChunk* createChunk(AbstractChunkSource *, BaseRepresentation *); /* impl */
                virtual void setSourceUrl( const std::string &url );
                virtual Url getUrlSegment() const; /* impl */
                virtual std::vector<ISegment*> subSegments();
                virtual void debug(vlc_object_t *,int = 0) const;
                virtual void addSubSegment(SubSegment *);
                static const int CLASSID_SEGMENT = 1;

            protected:
                std::vector<SubSegment *> subsegments;
                Url sourceUrl;
                int size;
        };

        class InitSegment : public Segment
        {
            public:
                InitSegment( ICanonicalUrl *parent );
                static const int CLASSID_INITSEGMENT = 2;
        };

        class IndexSegment : public Segment
        {
            public:
                IndexSegment( ICanonicalUrl *parent );
                static const int CLASSID_INDEXSEGMENT = 3;
        };

        class SubSegment : public ISegment
        {
            public:
                SubSegment(ISegment *, size_t start, size_t end);
                virtual SegmentChunk* createChunk(AbstractChunkSource *, BaseRepresentation *); /* impl */
                virtual Url getUrlSegment() const; /* impl */
                virtual std::vector<ISegment*> subSegments();
                virtual void addSubSegment(SubSegment *);
                static const int CLASSID_SUBSEGMENT = 4;
        };
    }
}

#endif /* SEGMENT_H_ */
