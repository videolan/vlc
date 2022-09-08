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

VLCTick::VLCTick()
    : m_ticks(VLC_TICK_INVALID)
{

}

VLCTick::VLCTick(vlc_tick_t ticks)
    : m_ticks(ticks)
{
}

VLCTick::operator vlc_tick_t() const
{
    return m_ticks;
}

bool VLCTick::valid() const
{
    return m_ticks != VLC_TICK_INVALID;
}

QString VLCTick::formatHMS() const
{
    if (m_ticks == VLC_TICK_INVALID)
        return "--:--";

    int64_t t_ms = MS_FROM_VLC_TICK(m_ticks);
    if (t_ms >= 1000)
    {
        //round to the nearest second
        t_ms = roundNearestMultiple(t_ms, 1000);
        int64_t t_sec = t_ms / 1000;
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
        return qtr("%1 ms").arg(MS_FROM_VLC_TICK(m_ticks));
}

QString VLCTick::formatLong() const
{
    if (m_ticks == VLC_TICK_INVALID)
        return "--:--";

    int64_t t_ms = MS_FROM_VLC_TICK(m_ticks);
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
    else if (t_ms >= 1000)
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

QString VLCTick::formatShort() const
{
    if (m_ticks == VLC_TICK_INVALID)
        return "--:--";

    int64_t t_ms = MS_FROM_VLC_TICK(m_ticks);
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
    else if (t_ms >= 1000)
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

VLCTick VLCTick::scale(float scalar) const
{
    if (scalar == 0.0f)
        return VLCTick(VLC_TICK_0); // to not decay to VLC_TICK_INVALID

    return VLCTick(m_ticks * scalar);
}

int VLCTick::toSeconds() const
{
    if (m_ticks == VLC_TICK_INVALID)
        return 0;

    int64_t t_sec = SEC_FROM_VLC_TICK(m_ticks);
    return t_sec;
}

int VLCTick::toMinutes() const
{
    if (m_ticks == VLC_TICK_INVALID)
        return 0;

    int64_t t_sec = SEC_FROM_VLC_TICK(m_ticks);
    return (t_sec / 60);
}

int VLCTick::toHours() const
{
    if (m_ticks == VLC_TICK_INVALID)
        return 0;

    int64_t t_sec = SEC_FROM_VLC_TICK(m_ticks);
    return (t_sec / 3600);
}

VLCTick VLCTick::fromMS(int64_t ms)
{
    return VLCTick(VLC_TICK_FROM_MS(ms));
}
