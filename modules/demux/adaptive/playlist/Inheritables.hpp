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
                enum class Type
                {
                    None,
                    Playlist,
                    SegmentInformation,
                    SegmentList,
                    SegmentBase,
                    SegmentTemplate,
                    Timescale,
                    Timeline,
                    Duration,
                    StartNumber,
                    AvailabilityTimeOffset,
                    AvailabilityTimeComplete,
                };
                AbstractAttr(enum Type);
                virtual ~AbstractAttr();
                AbstractAttr(const AbstractAttr &) = delete;
                AbstractAttr & operator=(const AbstractAttr &) = delete;
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
                AttrsNode( Type, AttrsNode * = nullptr );
                ~AttrsNode();
                AttrsNode(const AttrsNode &) = delete;
                AttrsNode & operator=(const AttrsNode &) = delete;
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
                virtual ~AttrWrapper() { condDeleteValue(value); }
                AttrWrapper(const AttrWrapper &) = delete;
                AttrWrapper<e, T> & operator=(const AttrWrapper<e, T> &) = delete;
                operator const T&() const { return value; }

            protected:
                void condDeleteValue(const T &) {}
                void condDeleteValue(T* &v) { delete v; }
                T value;
        };

        using AvailabilityTimeOffsetAttr   = AttrWrapper<AbstractAttr::Type::AvailabilityTimeOffset, vlc_tick_t>;
        using AvailabilityTimeCompleteAttr = AttrWrapper<AbstractAttr::Type::AvailabilityTimeComplete, bool>;
        using StartnumberAttr              = AttrWrapper<AbstractAttr::Type::StartNumber, uint64_t>;

        class TimescaleAttr:
                public AttrWrapper<AbstractAttr::Type::Timescale, Timescale>
        {
            public:
                TimescaleAttr(Timescale v) :
                    AttrWrapper<AbstractAttr::Type::Timescale, Timescale>( v ) {}
                virtual bool isValid() const { return value.isValid(); }
        };

        class DurationAttr:
                public AttrWrapper<AbstractAttr::Type::Duration, stime_t>
        {
            public:
                DurationAttr(stime_t v) :
                    AttrWrapper<AbstractAttr::Type::Duration, stime_t>( v ) {}
                virtual bool isValid() const { return value > 0; }
        };
    }
}

#endif // INHERITABLES_H
