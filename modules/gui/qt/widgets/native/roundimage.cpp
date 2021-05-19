/*****************************************************************************
 * roundimage.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

#include "roundimage.hpp"

#include <qhashfunctions.h>

#include <QBuffer>
#include <QCache>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QGuiApplication>

#ifdef QT_NETWORK_LIB
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

namespace
{
    struct ImageCacheKey
    {
        QUrl url;
        QSize size;
        qreal radius;
    };

    bool operator ==(const ImageCacheKey &lhs, const ImageCacheKey &rhs)
    {
        return lhs.radius == rhs.radius && lhs.size == rhs.size && lhs.url == rhs.url;
    }

    uint qHash(const ImageCacheKey &key, uint seed)
    {
        QtPrivate::QHashCombine hash;
        seed = hash(seed, key.url);
        seed = hash(seed, key.size.width());
        seed = hash(seed, key.size.height());
        seed = hash(seed, key.radius);
        return seed;
    }

    // images are cached (result of RoundImageGenerator) with the cost calculated from QImage::sizeInBytes
    QCache<ImageCacheKey, QImage> imageCache(2 * 1024 * 1024); // 2 MiB

    QString getPath(const QUrl &url)
    {
        QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (path.startsWith("qrc:///"))
            path.replace(0, strlen("qrc:///"), ":/");
        return path;
    }

    std::unique_ptr<QIODevice> getReadable(const QUrl &url)
    {
#ifdef QT_NETWORK_LIB
        if (url.scheme() == "http" || url.scheme() == "https")
        {
            QNetworkAccessManager networkMgr;
            networkMgr.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
            auto reply = networkMgr.get(QNetworkRequest(url));
            QEventLoop loop;
            QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
            loop.exec();

            class DataOwningBuffer : public QBuffer
            {
            public:
                DataOwningBuffer(const QByteArray &data) : m_data {data}
                {
                    setBuffer(&m_data);
                }

                ~DataOwningBuffer()
                {
                    close();
                    setBuffer(nullptr);
                }

            private:
                QByteArray m_data;
            };

            auto file = std::make_unique<DataOwningBuffer>(reply->readAll());
            file->open(QIODevice::ReadOnly);
            return file;
        }
#endif

        auto file = std::make_unique<QFile>(getPath(url));
        file->open(QIODevice::ReadOnly);
        return file;
    }
}

RoundImage::RoundImage(QQuickItem *parent) : QQuickPaintedItem {parent}
{
    if (window() || qGuiApp)
        setDPR(window() ? window()->devicePixelRatio() : qGuiApp->devicePixelRatio());

    connect(this, &QQuickItem::heightChanged, this, &RoundImage::regenerateRoundImage);
    connect(this, &QQuickItem::widthChanged, this, &RoundImage::regenerateRoundImage);
}

void RoundImage::paint(QPainter *painter)
{
    if (m_roundImage.isNull())
        return;
    painter->drawImage(QPointF {0., 0.}, m_roundImage, m_roundImage.rect());
}

void RoundImage::classBegin()
{
    QQuickPaintedItem::classBegin();

    m_isComponentComplete = false;
}

void RoundImage::componentComplete()
{
    QQuickPaintedItem::componentComplete();

    Q_ASSERT(!m_isComponentComplete); // classBegin is not called?
    m_isComponentComplete = true;
    if (!m_source.isEmpty())
        regenerateRoundImage();
    else
        m_roundImage = {};
}

QUrl RoundImage::source() const
{
    return m_source;
}

qreal RoundImage::radius() const
{
    return m_radius;
}

void RoundImage::setSource(QUrl source)
{
    if (m_source == source)
        return;

    m_source = source;
    emit sourceChanged(m_source);
    regenerateRoundImage();
}

void RoundImage::setRadius(qreal radius)
{
    if (m_radius == radius)
        return;

    m_radius = radius;
    emit radiusChanged(m_radius);
    regenerateRoundImage();
}

void RoundImage::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    if (change == QQuickItem::ItemDevicePixelRatioHasChanged)
        setDPR(value.realValue);

    QQuickPaintedItem::itemChange(change, value);
}

void RoundImage::setDPR(const qreal value)
{
    if (m_dpr == value)
        return;

    m_dpr = value;
    regenerateRoundImage();
}

void RoundImage::regenerateRoundImage()
{
    if (!m_isComponentComplete || m_enqueuedGeneration)
        return;

    // use Qt::QueuedConnection to delay generation, so that dependent properties
    // subsequent updates can be merged, f.e when VLCStyle.scale changes
    m_enqueuedGeneration = true;

    QMetaObject::invokeMethod(this, [this] ()
    {
        m_enqueuedGeneration = false;

        const qreal scaleWidth = this->width() * m_dpr;
        const qreal scaledHeight = this->height() * m_dpr;
        const qreal scaledRadius = this->radius() * m_dpr;

        const ImageCacheKey key {source(), QSizeF {scaleWidth, scaledHeight}.toSize(), scaledRadius};
        if (auto image = imageCache.object(key)) // should only by called in mainthread
        {
            m_roundImage = *image;
            update();
            return;
        }

        // Image is generated in size factor of `m_dpr` to avoid scaling artefacts when
        // generated image is set with device pixel ratio
        m_roundImageGenerator.reset(new RoundImageGenerator(m_source, scaleWidth, scaledHeight, scaledRadius));
        connect(m_roundImageGenerator.get(), &BaseAsyncTask::result, this, [this, key]()
        {
            m_roundImage = m_roundImageGenerator->takeResult();
            m_roundImage.setDevicePixelRatio(m_dpr);
            m_roundImageGenerator.reset();

            if (!m_roundImage.isNull())
                imageCache.insert(key, new QImage(m_roundImage), m_roundImage.sizeInBytes());

            update();
        });

        m_roundImageGenerator->start(*QThreadPool::globalInstance());
    }, Qt::QueuedConnection);
}

RoundImage::RoundImageGenerator::RoundImageGenerator(const QUrl &source, qreal width, qreal height, qreal radius)
    : source(source)
    , width(width)
    , height(height)
    , radius(radius)
{
}

QImage RoundImage::RoundImageGenerator::execute()
{
    if (width <= 0 || height <= 0)
        return {};

    auto file = getReadable(source);
    if (!file || !file->isOpen())
        return {};

    QImageReader sourceReader(file.get());

    // do PreserveAspectCrop
    const QSizeF size {width, height};
    QSizeF defaultSize = sourceReader.size();
    if (!defaultSize.isValid())
        defaultSize = size;

    const qreal ratio = std::max(size.width() / defaultSize.width(), size.height() / defaultSize.height());
    const QSizeF targetSize = defaultSize * ratio;
    const QPointF alignedCenteredTopLeft {(size.width() - targetSize.width()) / 2., (size.height() - targetSize.height()) / 2.};
    sourceReader.setScaledSize(targetSize.toSize());

    QImage target(width, height, QImage::Format_ARGB32);
    if (target.isNull())
        return target;

    target.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(0, 0, width, height, radius, radius);
    painter.setClipPath(path);

    painter.drawImage({alignedCenteredTopLeft, targetSize}, sourceReader.read());
    painter.end();

    return target;
}
