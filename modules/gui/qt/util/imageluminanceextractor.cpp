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

#include <QImage>
#include <QColor>

#include "imageluminanceextractor.hpp"

ImageLuminanceExtractor::ImageLuminanceExtractor(QObject *parent) : QObject(parent) {}

QUrl ImageLuminanceExtractor::source() const
{
    return m_source;
}

int ImageLuminanceExtractor::luminance() const
{
    return m_luminance;
}

bool ImageLuminanceExtractor::isEnabled() const
{
    return m_enabled;
}

void ImageLuminanceExtractor::setLuminance(const int luminance)
{
    m_luminance = luminance;
    emit luminanceChanged(m_luminance);
}

void ImageLuminanceExtractor::setSource(const QUrl &source)
{
    if (m_source == source)
        return;

    m_source = source;
    if (m_enabled)
        startTask();
    else
        m_pendingUpdate = true;
    emit sourceChanged(m_source);
}

void ImageLuminanceExtractor::setIsEnabled(const bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    if (m_enabled && m_pendingUpdate)
        startTask();
    emit enabledChanged(m_enabled);
}

void ImageLuminanceExtractor::startTask()
{
    m_pendingUpdate = false;
    m_task.reset(new LuminanceCalculator(m_source));
    connect(m_task.get(), &LuminanceCalculator::result, this, [this]()
    {
        LuminanceCalculator *task = static_cast<LuminanceCalculator *>(sender());
        assert(task == m_task.get());

        int result = task->takeResult();
        if (result != Status::FAILED)
            setLuminance(result);
        else
            qWarning("luminance extraction failed");

        m_task.reset();
    });

    m_task->start(*QThreadPool::globalInstance());
}

ImageLuminanceExtractor::LuminanceCalculator::LuminanceCalculator(const QUrl &source) : m_source {source} {}

int ImageLuminanceExtractor::LuminanceCalculator::execute()
{
    QString path = m_source.isLocalFile() ? m_source.toLocalFile() : m_source.toString();
    if (path.startsWith("qrc:///"))
        path.replace(0, strlen("qrc:///"), ":/");
    else if (!m_source.isLocalFile())
        return Status::FAILED;

    const QImage image = QImage(path).scaled(128, 128, Qt::KeepAspectRatio);
    if (image.isNull())
        return Status::FAILED;

    int lumimance = 0;
    size_t count = 0;
    for (int i = 0; i < image.width(); i++)
    {
        for (int j = 0; j < image.height(); j++)
        {
            const QColor pixelColor = image.pixelColor(i, j);
            if (!pixelColor.isValid())
                continue;

            lumimance += 0.299*pixelColor.red() + 0.587*pixelColor.green() + 0.144*pixelColor.blue();
            count++;
        }
    }

    lumimance = (lumimance / count);
    return lumimance;
}
