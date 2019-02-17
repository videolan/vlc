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
}

SegmentTimeline::SegmentTimeline(uint64_t scale)
    :TimescaleAble(NULL)
{
    setTimescale(scale);
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

stime_t SegmentTimeline::getMinAheadScaledTime(uint64_t number) const
{
    stime_t totalscaledtime = 0;

    std::list<Element *>::const_reverse_iterator it;
    for(it = elements.rbegin(); it != elements.rend(); ++it)
    {
        const Element *el = *it;

        if(number < el->number)
        {
            totalscaledtime += (el->d * (el->r + 1));
            break;
        }
        else if(number <= el->number + el->r)
        {
            totalscaledtime += el->d * (el->number + el->r - number);
        }
        else break;
    }

    return totalscaledtime;
}

uint64_t SegmentTimeline::getElementNumberByScaledPlaybackTime(stime_t scaled) const
{
    uint64_t prevnumber = 0;
    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;
        if(it == elements.begin())
        {
            scaled -= el->t;
            prevnumber = el->number;
        }

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

bool SegmentTimeline::getScaledPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                                   stime_t *time, stime_t *duration) const
{
    stime_t totalscaledtime = 0;
    stime_t lastduration = 0;

    std::list<Element *>::const_iterator it;
    for(it = elements.begin(); it != elements.end(); ++it)
    {
        const Element *el = *it;

        /* set start time, or from discontinuity */
        if(it == elements.begin() || el->t)
        {
            totalscaledtime = el->t;
        }

        lastduration = el->d;

        if(number <= el->number)
            break;

        if(number <= el->number + el->r)
        {
            totalscaledtime += el->d * (number - el->number);
            break;
        }

        totalscaledtime += (el->d * (el->r + 1));
    }

    *time = totalscaledtime;
    *duration = lastduration;
    return true;
}

stime_t SegmentTimeline::getScaledPlaybackTimeByElementNumber(uint64_t number) const
{
    stime_t time, duration;
    (void) getScaledPlaybackTimeDurationBySegmentNumber(number, &time, &duration);
    return time;
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
            delete el;
        }
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

        if(last->contains(el->t)) /* Same element, but prev could have been middle of repeat */
        {
            const uint64_t count = (el->t - last->t) / last->d;
            last->r = std::max(last->r, el->r + count);
            delete el;
        }
        else if(el->t < last->t)
        {
            delete el;
        }
        else /* Did not exist in previous list */
        {
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
