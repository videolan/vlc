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

QString VLCTick::toString() const
{
    if (m_ticks == VLC_TICK_INVALID)
        return "--:--";

    int64_t t_sec = SEC_FROM_VLC_TICK(m_ticks);
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

VLCTick VLCTick::scale(float scalar) const
{
    return VLCTick(m_ticks*scalar);
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
