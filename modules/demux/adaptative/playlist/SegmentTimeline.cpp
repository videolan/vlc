/*****************************************************************************
 * SegmentTimeline.cpp: Implement the SegmentTimeline tag.
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Hugo Beauzée-Luyssen <beauze.h@gmail.com>
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
#define __STDC_CONSTANT_MACROS

#include "SegmentTimeline.h"

#include <algorithm>

using namespace adaptative::playlist;

SegmentTimeline::SegmentTimeline(TimescaleAble *parent)
    :TimescaleAble(parent)
{
    pruned = 0;
}

SegmentTimeline::~SegmentTimeline()
{
    std::list<Element *>::iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
        delete *it;
}

void SegmentTimeline::addElement(mtime_t d, uint64_t r, mtime_t t)
{
    Element *element = new (std::nothrow) Element(d, r, t);
    if(element)
    {
        if(!elements.empty() && !t)
        {
            const Element *el = elements.back();
            element->t = el->t + (el->d * (el->r + 1));
        }
        elements.push_back(element);
    }
}

uint64_t SegmentTimeline::getElementNumberByScaledPlaybackTime(time_t scaled) const
{
    uint64_t count = 0;
    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;
        for(uint64_t repeat = 1 + el->r; repeat; repeat--)
        {
            if(el->d >= scaled)
                return count;

            scaled -= el->d;
            count++;
        }
    }
    count += pruned;
    return count;
}

mtime_t SegmentTimeline::getScaledPlaybackTimeByElementNumber(uint64_t number) const
{
    mtime_t totalscaledtime = 0;

    if(number < pruned)
        return 0;

    number -= pruned;

    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;

        if(number == 0)
        {
            totalscaledtime = el->t;
            break;
        }
        else if(number <= el->r)
        {
            totalscaledtime = el->t + (number * el->d);
            break;
        }
        else
        {
            number -= el->r + 1;
        }
    }

    return totalscaledtime;
}

size_t SegmentTimeline::maxElementNumber() const
{
    size_t count = 0;

    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
        count += (*it)->r + 1;

    return pruned + count - 1;
}

size_t SegmentTimeline::prune(mtime_t time)
{
    mtime_t scaled = time * inheritTimescale() / CLOCK_FREQ;
    size_t prunednow = 0;
    while(elements.size())
    {
        Element *el = elements.front();
        if(el->t + (el->d * (mtime_t)(el->r + 1)) < scaled)
        {
            prunednow += el->r + 1;
            delete el;
            elements.pop_front();
        }
        else
            break;
    }

    pruned += prunednow;
    return prunednow;
}

void SegmentTimeline::mergeWith(SegmentTimeline &other)
{
    if(elements.empty())
    {
        while(other.elements.size())
        {
            elements.push_back(other.elements.front());
            other.elements.pop_front();
        }
        return;
    }

    Element *last = elements.back();
    while(other.elements.size())
    {
        Element *el = other.elements.front();
        other.elements.pop_front();
        if(el->t == last->t) /* Same element, but prev could have been middle of repeat */
        {
            last->r = std::max(last->r, el->r);
            delete el;
        }
        else if(el->t > last->t) /* Did not exist in previous list */
        {
            if( el->t - last->t >= last->d * (mtime_t)(last->r + 1) )
            {
                elements.push_back(el);
                last = el;
            }
            else if(last->d == el->d) /* should always be in that case */
            {
                last->r = ((el->t - last->t) / last->d) - 1;
                elements.push_back(el);
                last = el;
            }
            else
            {
                /* borked: skip */
                delete el;
            }
        }
        else
        {
            delete el;
        }
    }
}

mtime_t SegmentTimeline::start() const
{
    if(elements.empty())
        return 0;
    return CLOCK_FREQ * elements.front()->t / inheritTimescale();
}

mtime_t SegmentTimeline::end() const
{
    if(elements.empty())
        return 0;
    const Element *last = elements.back();
    mtime_t scaled = last->t + last->d * (last->r + 1);
    return CLOCK_FREQ * scaled / inheritTimescale();
}

SegmentTimeline::Element::Element(mtime_t d_, uint64_t r_, mtime_t t_)
{
    d = d_;
    t = t_;
    r = r_;
}
