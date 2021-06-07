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
#include <QSGImageNode>
#include <QtQml>

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
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->error() != QNetworkReply::NoError)
            {
                qDebug() << reply->errorString();
                return {};
            }

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

RoundImage::RoundImage(QQuickItem *parent) : QQuickItem {parent}
{
    if (window() || qGuiApp)
        setDPR(window() ? window()->devicePixelRatio() : qGuiApp->devicePixelRatio());

    connect(this, &QQuickItem::heightChanged, this, &RoundImage::regenerateRoundImage);
    connect(this, &QQuickItem::widthChanged, this, &RoundImage::regenerateRoundImage);
}

QSGNode *RoundImage::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto node = static_cast<QSGImageNode *>(oldNode);

    if (!node)
    {
        assert(window());
        node = window()->createImageNode();
        assert(node);
        node->setOwnsTexture(true);
    }

    if (m_dirty)
    {
        if (!m_roundImage.isNull())
        {
            assert(window());

            QSGTexture* texture = window()->createTextureFromImage(m_roundImage,
                static_cast<QQuickWindow::CreateTextureOptions>(QQuickWindow::TextureHasAlphaChannel |
                                                                QQuickWindow::TextureCanUseAtlas));

            if (texture)
            {
                // No need to delete the old texture manually as it is owned by the node.
                node->setTexture(texture);
                node->markDirty(QSGNode::DirtyMaterial);
            }
            else
            {
                qmlWarning(this) << "Could not generate texture from " << m_roundImage;
            }
        }

        m_dirty = false;
    }

    node->setRect(boundingRect());

    return node;
}

void RoundImage::classBegin()
{
    QQuickItem::classBegin();

    m_isComponentComplete = false;
}

void RoundImage::componentComplete()
{
    QQuickItem::componentComplete();

    Q_ASSERT(!m_isComponentComplete); // classBegin is not called?
    m_isComponentComplete = true;
    if (!m_source.isEmpty())
        regenerateRoundImage();
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

    QQuickItem::itemChange(change, value);
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

    m_roundImageGenerator.reset();

    // use Qt::QueuedConnection to delay generation, so that dependent properties
    // subsequent updates can be merged, f.e when VLCStyle.scale changes
    m_enqueuedGeneration = true;

    QMetaObject::invokeMethod(this, [this] ()
    {
        m_enqueuedGeneration = false;
        assert(!m_roundImageGenerator);

        const qreal scaledWidth = this->width() * m_dpr;
        const qreal scaledHeight = this->height() * m_dpr;
        const qreal scaledRadius = this->radius() * m_dpr;

        const ImageCacheKey key {source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius};
        if (auto image = imageCache.object(key)) // should only by called in mainthread
        {
            m_roundImage = *image;
            m_dirty = true;
            setFlag(ItemHasContents, true);
            update();
            return;
        }

        // Image is generated in size factor of `m_dpr` to avoid scaling artefacts when
        // generated image is set with device pixel ratio
        m_roundImageGenerator.reset(new RoundImageGenerator(m_source, scaledWidth, scaledHeight, scaledRadius));
        connect(m_roundImageGenerator.get(), &BaseAsyncTask::result, this, [this, key]()
        {
            const auto image = new QImage(m_roundImageGenerator->takeResult());

            m_roundImageGenerator.reset();

            if (!image->isNull())
            {
                image->setDevicePixelRatio(m_dpr);

                imageCache.insert(key, image, image->sizeInBytes());

                setFlag(ItemHasContents, true);

                m_roundImage = *image;

                m_dirty = true;
            }
            else
            {
                delete image;
                m_dirty = false;
                setFlag(ItemHasContents, false);
            }

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
