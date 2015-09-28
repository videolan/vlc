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

#include "SegmentTimeline.h"

#include <algorithm>

using namespace adaptative::playlist;

SegmentTimeline::SegmentTimeline(TimescaleAble *parent)
    :TimescaleAble(parent)
{
}

SegmentTimeline::~SegmentTimeline()
{
    std::list<Element *>::iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
        delete *it;
}

void SegmentTimeline::addElement(uint64_t number, stime_t d, uint64_t r, stime_t t)
{
    Element *element = new (std::nothrow) Element(number, d, r, t);
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

uint64_t SegmentTimeline::getElementNumberByScaledPlaybackTime(stime_t scaled) const
{
    uint64_t prevnumber = 0;
    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;
        for(uint64_t repeat = 1 + el->r; repeat; repeat--)
        {
            if(el->d >= scaled)
                return prevnumber;

            scaled -= el->d;
            prevnumber++;
        }

        /* might have been discontinuity */
        prevnumber = el->number;
    }

    return prevnumber;
}

stime_t SegmentTimeline::getScaledPlaybackTimeByElementNumber(uint64_t number) const
{
    stime_t totalscaledtime = 0;

    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;

        /* set start time, or from discontinuity */
        if(it == elements.begin() || el->t)
        {
            totalscaledtime = el->t;
        }

        if(number <= el->number)
            break;

        if(number <= el->number + el->r)
        {
            totalscaledtime += el->d * (number - el->number);
            break;
        }

        totalscaledtime += (el->d * (el->r + 1));
    }

    return totalscaledtime;
}

uint64_t SegmentTimeline::maxElementNumber() const
{
    if(elements.empty())
        return 0;

    const Element *e = elements.back();
    return e->number + e->r;
}

size_t SegmentTimeline::prune(mtime_t time)
{
    stime_t scaled = time * inheritTimescale() / CLOCK_FREQ;
    size_t prunednow = 0;
    while(elements.size())
    {
        Element *el = elements.front();
        if(el->t + (el->d * (stime_t)(el->r + 1)) < scaled)
        {
            prunednow += el->r + 1;
            delete el;
            elements.pop_front();
        }
        else
            break;
    }

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
            if( el->t - last->t >= last->d * (stime_t)(last->r + 1) )
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
    return elements.front()->t * CLOCK_FREQ / inheritTimescale();
}

mtime_t SegmentTimeline::end() const
{
    if(elements.empty())
        return 0;
    const Element *last = elements.back();
    stime_t scaled = last->t + last->d * (last->r + 1);
    return scaled  * CLOCK_FREQ / inheritTimescale();
}

SegmentTimeline::Element::Element(uint64_t number_, stime_t d_, uint64_t r_, stime_t t_)
{
    number = number_;
    d = d_;
    t = t_;
    r = r_;
}
