/*****************************************************************************
 * input_slider.cpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2017 the VideoLAN team
 *
 * Authors: Pierre Lamot <pierre@videolabs.io>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QImageIOHandler>
#include <QPluginLoader>
#include <QFile>
#include <QThread>
#include "imagehelper.hpp"


QPixmap ImageHelper::loadSvgToPixmap( const QString &path, qint32 i_width, qint32 i_height )
{
    qreal ratio = QApplication::primaryScreen()->devicePixelRatio();

    const auto svgHandler = QScopedPointer<QImageIOHandler>(createSvgImageIOHandler());

    QImage image;
    const auto size = QSize( i_width, i_height ) * ratio;

    if (svgHandler)
    {
        QFile file(path);

        if (file.open(QFile::ReadOnly))
        {
            svgHandler->setDevice(&file);
            svgHandler->setOption(QImageIOHandler::ScaledSize, size);

            if (svgHandler->canRead())
                svgHandler->read(&image);
            else
                qWarning() << "ImageHelper: svg image handler can not read file " << path << ".";
        }
        else
        {
            qWarning() << "ImageHelper: can not open file " << path << ".";
        }
    }

    if (Q_UNLIKELY(image.isNull()))
    {
        image = QImage(size, QImage::Format_RGB32);
        image.fill(QColor("purple"));
    }

    image.setDevicePixelRatio(ratio);
    return QPixmap::fromImage(image);
}

QImageIOHandler *ImageHelper::createSvgImageIOHandler()
{
    static const auto plugin = []() {
        QPointer<QImageIOPlugin> plugin;

        const auto retrieve = [&plugin]() {
#ifdef QT_STATIC
            const auto& staticPlugins = QPluginLoader::staticInstances();
            const auto it = std::find_if(staticPlugins.begin(), staticPlugins.end(), [](QObject *obj) -> bool {
                return obj->inherits("QSvgPlugin");
            });

            if (it != staticPlugins.end())
                plugin = qobject_cast<QImageIOPlugin*>(*it);
#else
            QPluginLoader loader(QStringLiteral("imageformats/qsvg")); // Official Qt plugin
            // No need to check the metadata (or inherits `QSvgPlugin`), a plugin named "qsvg" should already support svg.
            plugin = qobject_cast<QImageIOPlugin*>(loader.instance());
#endif
        };

        assert(qApp); // do not call before QApplication is constructed.
        if (QThread::currentThread() == qApp->thread())
            retrieve();
        else
            QMetaObject::invokeMethod(qApp, retrieve, Qt::BlockingQueuedConnection);

        return plugin;
    }();

    if (!plugin)
    {
        qWarning() << "ImageHelper: svg image plugin is not found.";
        return nullptr;
    }

    return plugin->create(nullptr);
}
