/*****************************************************************************
 * Inheritables.cpp
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Inheritables.hpp"
#include "SegmentBase.h"
#include "SegmentList.h"
#include "SegmentTemplate.h"
#include "SegmentTimeline.h"

#include <algorithm>

using namespace adaptive::playlist;
using namespace adaptive;

AbstractAttr::AbstractAttr(Type t)
{
    type = t;
    parentNode = NULL;
}

AbstractAttr::~AbstractAttr()
{

}

AbstractAttr::Type AbstractAttr::getType() const
{
    return type;
}

AttrsNode::AttrsNode(Type t, AttrsNode *parent_)
    : AbstractAttr( t )
{
    setParentNode(parent_);
    is_canonical_root = (t == SEGMENTINFORMATION);
}

AttrsNode::~AttrsNode()
{
    while(!props.empty())
    {
        delete props.front();
        props.pop_front();
    }
}

void AttrsNode::addAttribute(AbstractAttr *p)
{
    props.push_front(p);
    p->setParentNode(this);
}

void AttrsNode::replaceAttribute(AbstractAttr *p)
{
    AbstractAttr *old = getAttribute(p->getType());
    if(old)
    {
        props.remove(old);
        delete old;
    }
    props.push_front(p);
    p->setParentNode(this);
}

AbstractAttr * AttrsNode::inheritAttribute(AbstractAttr::Type type)
{
    AbstractAttr *p = getAttribute(type);
    if(p && p->isValid())
        return p;

    /* List our path elements up to pseudo root node */
    AttrsNode *rootNode;
    std::list<AbstractAttr::Type> matchingpath;
    for(rootNode = this; rootNode; rootNode = rootNode->parentNode)
    {
        if(rootNode->is_canonical_root)
            break;
        matchingpath.push_front(rootNode->getType());
    }

    if(rootNode && !matchingpath.empty())
    {
        /* Try matching at each sibling level */
        for(;;)
        {
            /* Try same path on each sibling first */
            for(AttrsNode *node = rootNode->parentNode; node; node = node->parentNode)
            {
                p = node->getAttribute(type, matchingpath);
                if(p && p->isValid())
                    return p;
            }

            matchingpath.pop_back();
            if(matchingpath.empty())
                break;
        }
    }

    {
        /* Just try anything below */
        for(AttrsNode *node = this->parentNode; node ; node = node->parentNode)
        {
            p = node->getAttribute(type);
            if(p && p->isValid())
                return p;
        }
    }

    return p;
}

AbstractAttr * AttrsNode::inheritAttribute(AbstractAttr::Type type) const
{
    return const_cast<AttrsNode *>(this)->inheritAttribute(type);
}

stime_t AttrsNode::inheritDuration() const
{
    const AbstractAttr *p = inheritAttribute(Type::DURATION);
    if(p && p->isValid())
        return (const stime_t &) *(static_cast<const DurationAttr *>(p));
    return 0;
}

uint64_t AttrsNode::inheritStartNumber() const
{
    const AbstractAttr *p = inheritAttribute(Type::STARTNUMBER);
    if(p && p->isValid())
        return (const uint64_t &) *(static_cast<const StartnumberAttr *>(p));
    return std::numeric_limits<uint64_t>::max();
}

Timescale AttrsNode::inheritTimescale() const
{
    const AbstractAttr *p = inheritAttribute(Type::TIMESCALE);
    if(p && p->isValid())
        return (Timescale) *(static_cast<const TimescaleAttr *>(p));
    else
        return Timescale(1);
}

vlc_tick_t AttrsNode::inheritAvailabilityTimeOffset() const
{
    const AbstractAttr *p = inheritAttribute(Type::AVAILABILITYTTIMEOFFSET);
    if(p && p->isValid())
        return (const vlc_tick_t &) *(static_cast<const AvailabilityTimeOffsetAttr *>(p));
    return 0;
}

bool AttrsNode::inheritAvailabilityTimeComplete() const
{
    const AbstractAttr *p = inheritAttribute(Type::AVAILABILITYTTIMECOMPLETE);
    if(p && p->isValid())
        return (const bool &) *(static_cast<const AvailabilityTimeCompleteAttr *>(p));
    return true;
}

SegmentBase * AttrsNode::inheritSegmentBase() const
{
    AbstractAttr *p = inheritAttribute(Type::SEGMENTBASE);
    if(p && p->isValid())
        return static_cast<SegmentBase *>(p);
    return NULL;
}

SegmentList * AttrsNode::inheritSegmentList() const
{
    AbstractAttr *p = inheritAttribute(Type::SEGMENTLIST);
    if(p && p->isValid())
        return static_cast<SegmentList *> (p);
    return NULL;
}

SegmentTemplate * AttrsNode::inheritSegmentTemplate() const
{
    AbstractAttr *p = inheritAttribute(Type::SEGMENTTEMPLATE);
    if(p && p->isValid())
        return static_cast<SegmentTemplate *> (p);
    return NULL;
}

SegmentTimeline * AttrsNode::inheritSegmentTimeline() const
{
    AbstractAttr *p = inheritAttribute(Type::TIMELINE);
    if(p && p->isValid())
        return static_cast<SegmentTimeline *> (p);
    return NULL;
}

AttrsNode * AttrsNode::matchPath(std::list<AbstractAttr::Type>&path)
{
    AttrsNode *pn = this;
    std::list<AbstractAttr::Type>::const_iterator it;
    for(it = path.begin(); it != path.end(); it++)
    {
        AbstractAttr *p = pn->getAttribute(*it);
        if(!p || !p->isValid())
            return NULL;
        pn = dynamic_cast<AttrsNode *>(p);
        if(pn == NULL)
            return NULL;
    }
    return pn;
}

AbstractAttr * AttrsNode::getAttribute(AbstractAttr::Type type)
{
    for(auto it = props.begin(); it != props.end(); ++it)
    {
        if((*it)->getType() == type)
            return *it;
    }
    return NULL;
}

AbstractAttr * AttrsNode::getAttribute(AbstractAttr::Type type) const
{
    return const_cast<AttrsNode *>(this)->getAttribute(type);
}

AbstractAttr * AttrsNode::getAttribute(AbstractAttr::Type type,
                                           std::list<AbstractAttr::Type>&path)
{
    AttrsNode *matched = matchPath(path);
    if(matched)
    {
        AbstractAttr *p = matched->getAttribute(type);
        if(p && p->isValid())
            return p;
    }
    return NULL;
}
