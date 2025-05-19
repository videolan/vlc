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
#ifndef DELAYESTIMATOR
#define DELAYESTIMATOR

#pragma once

#include <QObject>
#include <QEvent>
#include "qt.hpp"
#include "util/vlctick.hpp"

class VLCDuration;

class DelayEstimator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isHeardTimeMarked READ isHeardTimeMarked NOTIFY heardTimeChanged FINAL)
    Q_PROPERTY(bool isSpottedTimeMarked READ isSpottedTimeMarked NOTIFY spottedTimeChanged FINAL)
    Q_PROPERTY(VLCDuration delay READ getDelay NOTIFY delayChanged FINAL)

public:
    explicit DelayEstimator(QObject* parent = nullptr);
    ~DelayEstimator();

    bool isHeardTimeMarked();
    bool isSpottedTimeMarked();
    void calculateDelay();
    VLCDuration getDelay();

    Q_INVOKABLE void markHeardTime();
    Q_INVOKABLE void markSpottedTime();
    Q_INVOKABLE void reset();

signals:
    void heardTimeChanged();
    void spottedTimeChanged();
    void delayChanged();

private:
    VLCTime m_heardTime;
    VLCTime m_spottedTime;
    VLCDuration m_delay;
};

#endif
