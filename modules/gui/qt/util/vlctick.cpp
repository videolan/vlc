/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "vlctick.hpp"
#include "qt.hpp"

namespace  {

int64_t roundNearestMultiple(int64_t number, int64_t multiple)
{
    int64_t result = number + multiple/2;
    result -= result % multiple;
    return result;
}

}

VLCTick::VLCTick(vlc_tick_t ticks)
    : m_ticks(ticks)
{}


bool VLCTick::isSubSecond() const
{
    return valid() && asMilliseconds() < 1000;
}

bool VLCTick::isSubHour() const
{
    return valid() && asSeconds() < 3600;
}

QString VLCTick::formatHMS(int formatFlags) const
{
    if (!valid())
        return "--:--";

    if (!isSubSecond() || !(formatFlags & SubSecondFormattedAsMS))
    {
        //truncate milliseconds toward 0
        int64_t t_sec = asSeconds();
        int sec = t_sec % 60;
        int min = (t_sec / 60) % 60;
        int hour = t_sec / 3600;

        if (hour == 0)
            return QString("%1:%2")
                    .arg(min, 2, 10, QChar('0'))
                    .arg(sec, 2, 10, QChar('0'));
        else
           return QString("%1:%2:%3")
                    .arg(hour, 2, 10, QChar('0'))
                    .arg(min, 2, 10, QChar('0'))
                    .arg(sec, 2, 10, QChar('0'));
    }
    else
        return qtr("%1 ms").arg(asMilliseconds());
}

QString VLCTick::formatLong(int formatFlags) const
{
    if (!valid())
        return "--:--";

    int64_t t_ms = asMilliseconds();
    if (t_ms >= 60*60*1000)
    {
        //round to the nearest minute
        t_ms = roundNearestMultiple(t_ms, 60*1000);
        int64_t t_sec = t_ms / 1000;
        int min = (t_sec / 60) % 60;
        int hour = t_sec / 3600;

        return qtr("%1 h %2 min")
                .arg(hour)
                .arg(min);
    }
    else if (!isSubSecond() || !(formatFlags & SubSecondFormattedAsMS))
    {
        //round to the nearest second
        t_ms = roundNearestMultiple(t_ms, 1000);
        int64_t t_sec = t_ms / 1000;
        int sec = t_sec % 60;
        int min = (t_sec / 60) % 60;
        if (min > 0)
            return qtr("%1 min %2 s")
                    .arg(min)
                    .arg(sec);
        else
            return qtr("%1 sec").arg(sec);

    }
    else
        return qtr("%1 ms").arg(t_ms);
}

QString VLCTick::formatShort(int formatFlags) const
{
    if (!valid())
        return "--:--";

    int64_t t_ms = asMilliseconds();
    if (t_ms >= 60*60*1000)
    {
        //round to the nearest minute
        t_ms = roundNearestMultiple(t_ms, 60*1000);
        int64_t t_sec = t_ms / 1000;
        int min = (t_sec / 60) % 60;
        int hour = t_sec / 3600;

        return qtr("%1h%2")
                .arg(hour)
                .arg(min, 2, 10, QChar('0'));
    }
    else if (!isSubSecond() || !(formatFlags & SubSecondFormattedAsMS))
    {
        //round to the nearest second
        t_ms = roundNearestMultiple(t_ms, 1000);
        int64_t t_sec = t_ms / 1000;
        int sec = t_sec % 60;
        int min = (t_sec / 60) % 60;

        return QString("%1:%2")
                .arg(min, 2, 10, QChar('0'))
                .arg(sec, 2, 10, QChar('0'));

    }
    else
        return qtr("%1ms").arg(t_ms);
}

int VLCTick::toSeconds() const
{
    return valid() ? asSeconds() : 0;
}

int VLCTick::toMinutes() const
{
    return asSeconds() / 60;
}

int VLCTick::toHours() const
{
    return asSeconds() / 3600;
}

int VLCTick::toMilliseconds() const
{
    return valid() ? asMilliseconds() : 0;
}


///// VLCDuration

VLCDuration::VLCDuration()
    : VLCTick(0)
    , m_valid(false)
{

}

VLCDuration::VLCDuration(vlc_tick_t t)
    : VLCTick(t)
    , m_valid(true)
{
}

VLCDuration VLCDuration::operator*(double f) const
{
    if (m_valid)
        return VLCDuration(m_ticks * f);
    else
        return {};
}

bool VLCDuration::operator==(const VLCDuration &rhs) const
{
    return m_valid == rhs.m_valid && m_ticks == rhs.m_ticks;
}

bool VLCDuration::operator>(const VLCDuration &rhs) const
{
    return m_ticks > rhs.m_ticks;
}

VLCDuration VLCDuration::operator+(const VLCDuration &rhs) const
{
    if (m_valid || rhs.m_valid)
        return VLCDuration{m_ticks + rhs.m_ticks};
    return VLCDuration{};
}

VLCDuration& VLCDuration::operator+=(const VLCDuration &rhs)
{
    m_valid |= rhs.m_valid;
    m_ticks += rhs.m_ticks;
    return *this;
}

VLCDuration VLCDuration::operator-(const VLCDuration &rhs) const
{
    if (m_valid || rhs.m_valid)
        return VLCDuration{m_ticks + rhs.m_ticks};
    return VLCDuration{};
}

VLCDuration& VLCDuration::operator-=(const VLCDuration &rhs)
{
    m_valid |= rhs.m_valid;
    m_ticks -= rhs.m_ticks;
    return *this;
}

double VLCDuration::toSecf() const
{
    return secf_from_vlc_tick(m_ticks);
}

VLCDuration VLCDuration::scale(float scalar) const
{
    if (m_valid)
        return VLCDuration(m_ticks * scalar);
    else
        return {};
}

VLCDuration VLCDuration::fromMS(int64_t ms)
{
    return VLCDuration(VLC_TICK_FROM_MS(ms));
}

int64_t VLCDuration::asMilliseconds() const
{
    return MS_FROM_VLC_TICK(m_ticks);
}

int64_t VLCDuration::asSeconds() const
{
    return SEC_FROM_VLC_TICK(m_ticks);
}

bool VLCDuration::valid() const
{
    return m_valid;
}

///// VLCTime

VLCTime::VLCTime()
    : VLCTick(VLC_TICK_INVALID)
{
}

VLCTime::VLCTime(vlc_tick_t t)
    : VLCTick(t)
{
}

VLCTime::VLCTime(VLCDuration d)
    : VLCTick(VLC_TICK_0 + d.toVLCTick())
{
}

VLCDuration VLCTime::operator-(const VLCTime &rhs) const
{
    if(m_ticks >= rhs.m_ticks)
        return VLCDuration(m_ticks - rhs.m_ticks);
    return VLCDuration();
}

bool VLCTime::operator<=(const VLCTime &rhs) const
{
    return m_ticks <= rhs.m_ticks;
}

VLCTime VLCTime::scale(float scalar) const
{
    if(!valid())
        return VLCTime(VLC_TICK_INVALID);
    return VLCTime(VLC_TICK_0 + ((m_ticks - VLC_TICK_0) * scalar));
}

int64_t VLCTime::asMilliseconds() const
{
    return MS_FROM_VLC_TICK(m_ticks - VLC_TICK_0);
}

int64_t VLCTime::asSeconds() const
{
    return SEC_FROM_VLC_TICK(m_ticks - VLC_TICK_0);
}

bool VLCTime::valid() const
{
    return m_ticks != VLC_TICK_INVALID;
}
