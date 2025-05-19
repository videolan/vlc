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
    return m_heardTime.valid();
}

bool DelayEstimator::isSpottedTimeMarked() {
    return m_spottedTime.valid();
}

/*Q_INVOKABLE*/ VLCDuration DelayEstimator::getDelay()
{
    return m_delay;
}

/*Q_INVOKABLE*/ void DelayEstimator::markHeardTime()
{
    if (isHeardTimeMarked())
        m_heardTime = VLCTime();
    else
        m_heardTime = vlc_tick_now();

    emit heardTimeChanged();

    if (isSpottedTimeMarked())
        calculateDelay();
}

/*Q_INVOKABLE*/ void DelayEstimator::markSpottedTime()
{
    if (isSpottedTimeMarked())
        m_spottedTime = VLCTime();
    else
        m_spottedTime = vlc_tick_now();

    emit spottedTimeChanged();

    if (isHeardTimeMarked())
        calculateDelay();
}

void DelayEstimator::calculateDelay()
{
    if (m_heardTime.valid() &&
        m_spottedTime.valid())
    {
        m_delay = m_heardTime - m_spottedTime;
        emit delayChanged();
        m_heardTime = VLCTime();
        m_spottedTime = VLCTime();
        emit spottedTimeChanged();
        emit heardTimeChanged();
    }
}

/*Q_INVOKABLE*/ void DelayEstimator::reset()
{
    m_heardTime = VLCTime();
    m_spottedTime = VLCTime();
    m_delay = VLCDuration();
    emit delayChanged();
    emit spottedTimeChanged();
    emit heardTimeChanged();
}
