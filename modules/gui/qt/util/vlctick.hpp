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
#include <QtQml/qqmlregistration.h>

class VLCTick
{
    Q_GADGET
public:
    enum FormatFlag {
        SubSecondFormattedAsMS = 1
    };
    Q_ENUM(FormatFlag)

    Q_INVOKABLE virtual bool valid() const = 0;

    Q_INVOKABLE bool isSubSecond() const;

    Q_INVOKABLE bool isSubHour() const;

    /**
     * @brief formatHMS
     * @param formatFlags flags to specialize formatting, default is SubSecondFormattedAsMS for legacy reasons
     * @return time as HH:MM:SS
     *
     * this method should be used to present running time or
     * time that will be compared to a running time
     *
     * milliseconds will be truncated towards 0
     */
    Q_INVOKABLE QString formatHMS(int formatFlags = SubSecondFormattedAsMS) const;

    /**
     * @brief formatLong
     * @param formatFlags flags to specialize formatting, default is SubSecondFormattedAsMS for legacy reasons
     * @return time in literal form
     * 1h 2min
     * 5 min
     * 10 sec
     * 43 ms
     */
    Q_INVOKABLE QString formatLong(int formatFlags = SubSecondFormattedAsMS) const;

    /**
     * @brief formatShort
     * @param formatFlags flags to specialize formatting, default is SubSecondFormattedAsMS for legacy reasons
     * @return time in literal form
     * 1h02
     * 02:42
     * 43 ms
     */
    Q_INVOKABLE QString formatShort(int formatFlags = SubSecondFormattedAsMS) const;

    inline vlc_tick_t toVLCTick() const {
        return m_ticks;
    }

    Q_INVOKABLE int toMinutes() const;
    Q_INVOKABLE int toSeconds() const;
    Q_INVOKABLE int toHours()   const;
    int toMilliseconds() const;

protected:
    VLCTick(vlc_tick_t ticks);
    VLCTick() = delete;

    virtual int64_t asMilliseconds() const = 0;
    virtual int64_t asSeconds() const = 0;

    vlc_tick_t m_ticks;
};

class VLCDuration : public VLCTick
{
    Q_GADGET
    QML_VALUE_TYPE(vlcDuration)

public:
    VLCDuration();
    VLCDuration(vlc_tick_t);

    VLCDuration operator*(double) const;
    VLCDuration operator+(const VLCDuration &) const;
    VLCDuration& operator+=(const VLCDuration &);
    VLCDuration operator-(const VLCDuration &) const;
    VLCDuration& operator-=(const VLCDuration &);
    bool operator==(const VLCDuration &) const;
    bool operator>(const VLCDuration &) const;

    double toSecf() const;

    Q_INVOKABLE VLCDuration scale(float) const;

    static VLCDuration fromMS(int64_t ms);

    int64_t asMilliseconds() const override;
    int64_t asSeconds() const override;
    bool valid() const override;

private:
    bool m_valid;
};

class VLCTime : public VLCTick
{
    Q_GADGET
    QML_VALUE_TYPE(vlcTime)
public:
    VLCTime();
    VLCTime(vlc_tick_t);
    VLCTime(VLCDuration);
    VLCDuration operator-(const VLCTime &rhs) const;
    bool operator<=(const VLCTime &) const;

    Q_INVOKABLE VLCTime scale(float) const;

    int64_t asMilliseconds() const override;
    int64_t asSeconds() const override;
    bool valid() const override;
};

//following boilerplate allow registering FormatFlag enum in the VLCTick namespace in QML
//VLCTick is not a QML_VALUE_TYPE so we don't need to define a distinct type to
//register it in the namespace

namespace VLCTickForeign
{
    Q_NAMESPACE
    QML_NAMED_ELEMENT(VLCTick)
    QML_FOREIGN_NAMESPACE(VLCTick)
}

#endif // VLCTICK_HPP
