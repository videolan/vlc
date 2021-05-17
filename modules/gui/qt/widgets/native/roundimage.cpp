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

#include <QBrush>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQuickWindow>
#include <QSvgRenderer>
#include <QGuiApplication>

#ifdef QT_NETWORK_LIB
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

namespace
{
    class Image
    {
    public:
        static std::unique_ptr<Image> getImage(const QUrl &source, const QSizeF &sourceSize);
        virtual ~Image() = default;

        // must be reentrant
        virtual void paint(QPainter *painter, const QSizeF &size) = 0;
    };

    QString getPath(const QUrl &url)
    {
        QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (path.startsWith("qrc:///"))
            path.replace(0, strlen("qrc:///"), ":/");
        return path;
    }

    QByteArray readFile(const QUrl &url)
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
            return reply->readAll();
        }
#endif

        QByteArray data;
        QString path = getPath(url);
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            data = file.readAll();
        return data;
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

        // Image is generated in size factor of `m_dpr` to avoid scaling artefacts when
        // generated image is set with device pixel ratio
        m_roundImageGenerator.reset(new RoundImageGenerator(m_source, width() * m_dpr, height() * m_dpr, radius() * m_dpr));
        connect(m_roundImageGenerator.get(), &BaseAsyncTask::result, this, [this]()
        {
            m_roundImage = m_roundImageGenerator->takeResult();
            m_roundImage.setDevicePixelRatio(m_dpr);
            m_roundImageGenerator.reset();

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

    auto image = Image::getImage(source, {width, height});
    image->paint(&painter, {width, height});

    painter.end();

    return target;
}

std::unique_ptr<Image> Image::getImage(const QUrl &source, const QSizeF &sourceSize)
{
    class QtImage : public Image
    {
    public:
        QtImage(const QByteArray &data, const QSizeF &sourceSize)
        {
            m_image.loadFromData(data);

            if (sourceSize.isValid())
                m_image = m_image.scaled(sourceSize.toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        void paint(QPainter *painter, const QSizeF &size) override
        {
            if (m_image.isNull() || !painter || !size.isValid())
                return;

            auto image = m_image;

            // do PreserveAspectCrop
            const qreal ratio = std::max(qreal(size.width()) / image.width(), qreal(size.height()) / image.height());
            if (ratio != 0.0)
                image = image.scaled(qRound(image.width() * ratio), qRound(image.height() * ratio),
                                     Qt::IgnoreAspectRatio, // aspect ratio handled manually by using `ratio`
                                     Qt::SmoothTransformation);

            const QPointF alignedCenteredTopLeft {(size.width() - image.width()) / 2, (size.height() - image.height()) / 2};
            painter->drawImage(alignedCenteredTopLeft, image);
        }

    private:
        QImage m_image;
    };

    class SVGImage : public Image
    {
    public:
        SVGImage(const QByteArray &data)
        {
            m_svg.load(data);
        }

        void paint(QPainter *painter, const QSizeF &size) override
        {
            if (!m_svg.isValid() || !painter || !size.isValid())
                return;

            // do PreserveAspectCrop
            const QSizeF defaultSize = m_svg.defaultSize();
            const qreal ratio = std::max(size.width() / defaultSize.width(), size.height() / defaultSize.height());
            const QSizeF targetSize = defaultSize * ratio;
            const QPointF alignedCenteredTopLeft {(size.width() - targetSize.width()) / 2., (size.height() - targetSize.height()) / 2.};
            m_svg.render(painter, QRectF {alignedCenteredTopLeft, targetSize});
        }

    private:
        QSvgRenderer m_svg;
    };

    const QByteArray data = readFile(source);
    if (source.toString().endsWith(".svg"))
        return std::make_unique<SVGImage>(data);
    return std::make_unique<QtImage>(data, sourceSize);
}
