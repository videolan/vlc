/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#include "util/vlctick.hpp"
#include "delay_estimator.hpp"

DelayEstimator::DelayEstimator(QObject *parent)
    : QObject(parent)
{
}

DelayEstimator::~DelayEstimator()
{
}

bool DelayEstimator::isHeardTimeMarked() {
    return (m_heardTime != VLC_TICK_INVALID);
}

bool DelayEstimator::isSpottedTimeMarked() {
    return (m_spottedTime != VLC_TICK_INVALID);
}

/*Q_INVOKABLE*/ VLCTick DelayEstimator::getDelay()
{
    return m_delay;
}

/*Q_INVOKABLE*/ void DelayEstimator::markHeardTime()
{
    if (isHeardTimeMarked())
        m_heardTime = VLC_TICK_INVALID;
    else
        m_heardTime = vlc_tick_now();

    emit heardTimeChanged();

    if (isSpottedTimeMarked())
        calculateDelay();
}

/*Q_INVOKABLE*/ void DelayEstimator::markSpottedTime()
{
    if (isSpottedTimeMarked())
        m_spottedTime = VLC_TICK_INVALID;
    else
        m_spottedTime = vlc_tick_now();

    emit spottedTimeChanged();

    if (isHeardTimeMarked())
        calculateDelay();
}

void DelayEstimator::calculateDelay()
{
    if (m_heardTime != VLC_TICK_INVALID &&
        m_spottedTime != VLC_TICK_INVALID)
    {
        m_delay = m_heardTime - m_spottedTime;
        emit delayChanged();
        m_heardTime = VLC_TICK_INVALID;
        m_spottedTime = VLC_TICK_INVALID;
        emit spottedTimeChanged();
        emit heardTimeChanged();
    }
}

/*Q_INVOKABLE*/ void DelayEstimator::reset()
{
    m_heardTime = VLC_TICK_INVALID;
    m_spottedTime = VLC_TICK_INVALID;
    m_delay = VLC_TICK_INVALID;
    emit delayChanged();
    emit spottedTimeChanged();
    emit heardTimeChanged();
}
