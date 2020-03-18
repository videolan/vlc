/*****************************************************************************
 * SegmentTimeline.cpp: Implement the SegmentTimeline tag.
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#include "SegmentTimeline.h"

#include <algorithm>

using namespace adaptive::playlist;

SegmentTimeline::SegmentTimeline(TimescaleAble *parent)
    :TimescaleAble(parent)
{
    totalLength = 0;
}

SegmentTimeline::SegmentTimeline(uint64_t scale)
    :TimescaleAble(NULL)
{
    setTimescale(scale);
    totalLength = 0;
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
        totalLength += (d * (r + 1));
    }
}

stime_t SegmentTimeline::getMinAheadScaledTime(uint64_t number) const
{
    stime_t totalscaledtime = 0;

    if(!elements.size() ||
       minElementNumber() > number ||
       maxElementNumber() < number)
        return 0;

    std::list<Element *>::const_reverse_iterator it;
    for(it = elements.rbegin(); it != elements.rend(); ++it)
    {
        const Element *el = *it;
        if(number > el->number + el->r)
            break;
        else if(number < el->number + el->r)
            totalscaledtime += (el->d * (el->r + 1));
        else /* within repeat range */
            totalscaledtime += el->d * (el->number + el->r - number);
    }

    return totalscaledtime;
}

uint64_t SegmentTimeline::getElementNumberByScaledPlaybackTime(stime_t scaled) const
{
    const Element *prevel = NULL;
    std::list<Element *>::const_iterator it;

    if(!elements.size())
        return 0;

    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;
        if(scaled >= el->t)
        {
            if((uint64_t)scaled < el->t + (el->d * el->r))
                return el->number + (scaled - el->t) / el->d;
        }
        /* might have been discontinuity */
        else
        {
            if(prevel) /* > prev but < current */
                return prevel->number + prevel->r;
            else /* << first of the list */
                return el->number;
        }
        prevel = el;
    }

    /* time is >> any of the list */
    return prevel->number + prevel->r;
}

bool SegmentTimeline::getScaledPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                                   stime_t *time, stime_t *duration) const
{
    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;
        if(number >= el->number)
        {
            if(number <= el->number + el->r)
            {
                *time = el->t + el->d * (number - el->number);
                *duration = el->d;
                return true;
            }
        }
    }
    return false;
}

stime_t SegmentTimeline::getScaledPlaybackTimeByElementNumber(uint64_t number) const
{
    stime_t time = 0, duration = 0;
    (void) getScaledPlaybackTimeDurationBySegmentNumber(number, &time, &duration);
    return time;
}

stime_t SegmentTimeline::getTotalLength() const
{
    return totalLength;
}

uint64_t SegmentTimeline::maxElementNumber() const
{
    if(elements.empty())
        return 0;

    const Element *e = elements.back();
    return e->number + e->r;
}

uint64_t SegmentTimeline::minElementNumber() const
{
    if(elements.empty())
        return 0;
    return elements.front()->number;
}

void SegmentTimeline::pruneByPlaybackTime(vlc_tick_t time)
{
    const Timescale timescale = inheritTimescale();
    uint64_t num = getElementNumberByScaledPlaybackTime(timescale.ToScaled(time));
    pruneBySequenceNumber(num);
}

size_t SegmentTimeline::pruneBySequenceNumber(uint64_t number)
{
    size_t prunednow = 0;
    while(elements.size())
    {
        Element *el = elements.front();
        if(el->number >= number)
        {
            break;
        }
        else if(el->number + el->r >= number)
        {
            uint64_t count = number - el->number;
            el->number += count;
            el->t += count * el->d;
            el->r -= count;
            prunednow += count;
            break;
        }
        else
        {
            prunednow += el->r + 1;
            elements.pop_front();
            totalLength -= (el->d * (el->r + 1));
            delete el;
        }
    }

    return prunednow;
}

void SegmentTimeline::updateWith(SegmentTimeline &other)
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

        if(last->contains(el->t)) /* Same element, but prev could have been middle of repeat */
        {
            const uint64_t count = (el->t - last->t) / last->d;
            totalLength -= (last->d * (last->r + 1));
            last->r = std::max(last->r, el->r + count);
            totalLength += (last->d * (last->r + 1));
            delete el;
        }
        else if(el->t < last->t)
        {
            delete el;
        }
        else /* Did not exist in previous list */
        {
            totalLength += (el->d * (el->r + 1));
            elements.push_back(el);
            el->number = last->number + last->r + 1;
            last = el;
        }
    }
}

void SegmentTimeline::debug(vlc_object_t *obj, int indent) const
{
    std::stringstream ss;
    ss << std::string(indent, ' ') << "Timeline";
    msg_Dbg(obj, "%s", ss.str().c_str());

    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
        (*it)->debug(obj, indent + 1);
}

SegmentTimeline::Element::Element(uint64_t number_, stime_t d_, uint64_t r_, stime_t t_)
{
    number = number_;
    d = d_;
    t = t_;
    r = r_;
}

bool SegmentTimeline::Element::contains(stime_t time) const
{
    if(time >= t && time < t + (stime_t)(r + 1) * d)
        return true;
    return false;
}

void SegmentTimeline::Element::debug(vlc_object_t *obj, int indent) const
{
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    ss << std::string(indent + 1, ' ') << "Element #" << number
       << " d=" << d << " r=" << r << " @t=" << t;
    msg_Dbg(obj, "%s", ss.str().c_str());
}
