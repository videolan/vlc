/*****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef IMAGELUMINANCEXTRACTOR_HPP
#define IMAGELUMINANCEXTRACTOR_HPP

#include <QUrl>

#include "util/asynctask.hpp"

class ImageLuminanceExtractor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int luminance READ luminance NOTIFY luminanceChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setIsEnabled NOTIFY enabledChanged)

public:
    enum Status
    {
        FAILED = -1
    };

    ImageLuminanceExtractor(QObject *parent = nullptr);

    QUrl source() const;
    int luminance() const;
    bool isEnabled() const;

public slots:
    void setSource(const QUrl &source);
    void setIsEnabled(bool enabled);

signals:
    void sourceChanged(QUrl source);
    void luminanceChanged(int luminance);
    void enabledChanged(bool enabled);

private:
    class LuminanceCalculator : public AsyncTask<int>
    {
    public:
        LuminanceCalculator(const QUrl &source);

        int execute() override;

    private:
        QUrl m_source;
    };

    void startTask();
    void setLuminance(int luminance);

    QUrl m_source;
    TaskHandle<LuminanceCalculator> m_task;
    int m_luminance = 0;
    bool m_enabled = false;
    bool m_pendingUpdate = false;
};

#endif // IMAGELUMINANCEXTRACTOR_HPP
