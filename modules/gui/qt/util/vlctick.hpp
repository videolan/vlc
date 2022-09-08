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
#ifndef VLCTICK_HPP
#define VLCTICK_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <vlc_common.h>
#include <vlc_tick.h>

class VLCTick
{
    Q_GADGET
public:
    VLCTick();
    VLCTick(vlc_tick_t ticks);

    operator vlc_tick_t() const;

    Q_INVOKABLE bool valid() const;

    /**
     * @brief formatHMS
     * @return time as HH:MM:SS
     */
    Q_INVOKABLE QString formatHMS() const;

    /**
     * @brief formatLong
     * @return time in literal form
     * 1h 2min
     * 5 min
     * 10 sec
     * 43 ms
     */
    Q_INVOKABLE QString formatLong() const;

    /**
     * @brief formatShort
     * @return time in literal form
     * 1h02
     * 02:42
     * 43 ms
     */
    Q_INVOKABLE QString formatShort() const;


    Q_INVOKABLE VLCTick scale(float) const;

    Q_INVOKABLE int toMinutes() const;
    Q_INVOKABLE int toSeconds() const;
    Q_INVOKABLE int toHours()   const;


    static VLCTick fromMS(int64_t ms);
private:
    vlc_tick_t m_ticks;
};

Q_DECLARE_METATYPE(VLCTick)

#endif // VLCTICK_HPP
