/*****************************************************************************
 * Inheritables.hpp Nodes inheritables properties
 *****************************************************************************
 * Copyright (C) 2016-2020 VideoLabs, VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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
#ifndef INHERITABLES_H
#define INHERITABLES_H

#include <list>
#include <limits>
#include <stdint.h>
#include "../Time.hpp"

namespace adaptive
{
    namespace playlist
    {
        class AttrsNode;
        class SegmentTimeline;
        class SegmentTemplate;
        class SegmentList;
        class SegmentBase;

        class AbstractAttr
        {
            public:
                enum Type
                {
                    NONE,
                    PLAYLIST,
                    SEGMENTINFORMATION,
                    SEGMENTLIST,
                    SEGMENTBASE,
                    SEGMENTTEMPLATE,
                    TIMESCALE,
                    TIMELINE,
                    DURATION,
                    STARTNUMBER,
                    AVAILABILITYTTIMEOFFSET,
                    AVAILABILITYTTIMECOMPLETE,
                };
                AbstractAttr(enum Type);
                virtual ~AbstractAttr();
                Type getType() const;
                bool operator ==(const AbstractAttr &t) const { return type == t.getType(); }
                bool operator !=(const AbstractAttr &t) const { return type != t.getType(); }
                virtual bool isValid() const { return true; }
                void setParentNode(AttrsNode *n) { parentNode = n; }

            protected:
                Type type;
                AttrsNode *parentNode;
        };

        class AttrsNode : public AbstractAttr
        {
            public:
                AttrsNode( enum Type, AttrsNode * = NULL );
                ~AttrsNode();
                void addAttribute( AbstractAttr * );
                void replaceAttribute( AbstractAttr * );
                AbstractAttr * inheritAttribute(AbstractAttr::Type);
                AbstractAttr * inheritAttribute(AbstractAttr::Type) const;
                /* helpers */
                uint64_t          inheritStartNumber() const;
                stime_t           inheritDuration() const;
                Timescale         inheritTimescale() const;
                vlc_tick_t        inheritAvailabilityTimeOffset() const;
                bool              inheritAvailabilityTimeComplete() const;
                SegmentTimeline * inheritSegmentTimeline() const;
                SegmentTemplate * inheritSegmentTemplate() const;
                SegmentList *     inheritSegmentList() const;
                SegmentBase *     inheritSegmentBase() const;

            protected:
                AttrsNode * matchPath(std::list<AbstractAttr::Type>&);
                AbstractAttr * getAttribute(AbstractAttr::Type,
                                            std::list<AbstractAttr::Type>&);
                AbstractAttr * getAttribute(AbstractAttr::Type);
                AbstractAttr * getAttribute(AbstractAttr::Type) const;
                std::list<AbstractAttr *> props;
                bool is_canonical_root;
        };

        template<enum AbstractAttr::Type e, typename T>
        class AttrWrapper : public AbstractAttr
        {
            public:
                AttrWrapper(T v) : AbstractAttr(e) { value = v; }
                virtual ~AttrWrapper() {}
                operator const T&() const { return value; }

            protected:
                T value;
        };

        typedef AttrWrapper<AbstractAttr::Type::AVAILABILITYTTIMEOFFSET, vlc_tick_t> AvailabilityTimeOffsetAttr;
        typedef AttrWrapper<AbstractAttr::Type::AVAILABILITYTTIMECOMPLETE, bool>     AvailabilityTimeCompleteAttr;
        typedef AttrWrapper<AbstractAttr::Type::STARTNUMBER, uint64_t>               StartnumberAttr;

        class TimescaleAttr:
                public AttrWrapper<AbstractAttr::Type::TIMESCALE, Timescale>
        {
            public:
                TimescaleAttr(Timescale v) :
                    AttrWrapper<AbstractAttr::Type::TIMESCALE, Timescale>( v ) {}
                virtual bool isValid() const { return value.isValid(); }
        };

        class DurationAttr:
                public AttrWrapper<AbstractAttr::Type::DURATION, stime_t>
        {
            public:
                DurationAttr(stime_t v) :
                    AttrWrapper<AbstractAttr::Type::DURATION, stime_t>( v ) {}
                virtual bool isValid() const { return value > 0; }
        };

        class TimescaleAble
        {
            public:
                TimescaleAble( TimescaleAble * = NULL );
                virtual ~TimescaleAble();
                void setParentTimescaleAble( TimescaleAble * );
                virtual Timescale inheritTimescale() const;
                void setTimescale( const Timescale & );
                void setTimescale( uint64_t );
                const Timescale & getTimescale() const;

            protected:
                TimescaleAble *parentTimescaleAble;

            private:
                Timescale timescale;
        };
    }
}

#endif // INHERITABLES_H
