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
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QGuiApplication>
#include <QSGImageNode>
#include <QtQml>
#include <QQmlFile>

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

    std::unique_ptr<QIODevice> getReadable(const QUrl &url)
    try
    {
        if (!QQmlFile::isLocalFile(url))
        {
#ifdef QT_NETWORK_LIB
            QNetworkAccessManager networkMgr;
            networkMgr.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
            auto reply = networkMgr.get(QNetworkRequest(url));
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->error() != QNetworkReply::NoError)
                throw std::runtime_error(reply->errorString().toStdString());

            class DataOwningBuffer : private QByteArray, public QBuffer
            {
            public:
                explicit DataOwningBuffer(const QByteArray &data)
                    : QByteArray(data), QBuffer(this, nullptr) { }
            };

            auto file = std::make_unique<DataOwningBuffer>(reply->readAll());
            file->open(QIODevice::ReadOnly);
            return file;
#else
            throw std::runtime_error("Qt Network Library is not available!");
#endif
        }
        else
        {
            auto file = std::make_unique<QFile>(QQmlFile::urlToLocalFileOrQrc(url));
            file->open(QIODevice::ReadOnly);
            return file;
        }
    }
    catch (const std::exception& error)
    {
        qWarning() << "Could not load source image:" << url << error.what();
        return {};
    }

    QRectF doPreserveAspectCrop(const QSizeF &sourceSize, const QSizeF &size)
    {
        const qreal ratio = std::max(size.width() / sourceSize.width(), size.height() / sourceSize.height());
        const QSizeF imageSize = sourceSize * ratio;
        const QPointF alignedCenteredTopLeft {(size.width() - imageSize.width()) / 2., (size.height() - imageSize.height()) / 2.};
        return {alignedCenteredTopLeft, imageSize};
    }

    class ImageReader : public AsyncTask<QImage>
    {
    public:
        // requestedSize is only taken as hint, the Image is resized with PreserveAspectCrop
        ImageReader(const QUrl &url, QSize requestedSize)
            : url {url}
            , requestedSize {requestedSize}
        {
        }

        QString errorString() const { return errorStr; }

        QImage execute()
        {
            auto file = getReadable(url);
            if (!file || !file->isOpen())
                return {};

            QImageReader reader;
            reader.setDevice(file.get());
            const QSize sourceSize = reader.size();

            if (requestedSize.isValid())
                reader.setScaledSize(doPreserveAspectCrop(sourceSize, requestedSize).size().toSize());

            auto img = reader.read();
            errorStr = reader.errorString();
            return img;
        }

    private:
        QUrl url;
        QSize requestedSize;
        QString errorStr;
    };

    class LocalImageResponse : public QQuickImageResponse
    {
    public:
        LocalImageResponse(const QUrl &url, const QSize &requestedSize)
        {
            reader.reset(new ImageReader(url, requestedSize));
            connect(reader.get(), &ImageReader::result, this, &LocalImageResponse::handleImageRead);

            reader->start(*QThreadPool::globalInstance());
        }

        QQuickTextureFactory *textureFactory() const override
        {
            return result.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(result);
        }

        QString errorString() const override
        {
            return errorStr;
        }

    private:
        void handleImageRead()
        {
            result = reader->takeResult();
            errorStr = reader->errorString();
            reader.reset();

            emit finished();
        }

        QImage result;
        TaskHandle<ImageReader> reader;
        QString errorStr;
    };

    class ImageProviderAsyncAdaptor : public QQuickImageResponse
    {
    public:
        ImageProviderAsyncAdaptor(QQuickImageProvider *provider, const QString &id, const QSize &requestedSize)
        {
            task.reset(new ProviderImageGetter(provider, id, requestedSize));
            connect(task.get(), &ProviderImageGetter::result, this, [this]()
            {
                result = task->takeResult();
                task.reset();

                emit finished();
            });

            task->start(*QThreadPool::globalInstance());
        }

        QQuickTextureFactory *textureFactory() const override
        {
            return result.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(result);
        }

    private:
        class ProviderImageGetter : public AsyncTask<QImage>
        {
        public:
            ProviderImageGetter(QQuickImageProvider *provider, const QString &id, const QSize &requestedSize)
                : provider {provider}
                , id{id}
                , requestedSize{requestedSize}
            {
            }

            QImage execute() override
            {
                return provider->requestImage(id, &sourceSize, requestedSize);
            }

        private:
            QQuickImageProvider *provider;
            QString id;
            QSize requestedSize;
            QSize sourceSize;
        };

        TaskHandle<ProviderImageGetter> task;
        QImage result;
    };

    QQuickImageResponse *getAsyncImageResponse(const QUrl &url, const QSize &requestedSize, QQmlEngine *engine)
    {
        if (url.scheme() == QStringLiteral("image"))
        {
            auto provider = engine->imageProvider(url.host());
            if (!provider)
                return nullptr;

            assert(provider->imageType() == QQmlImageProviderBase::Image
                   || provider->imageType() == QQmlImageProviderBase::ImageResponse);

            const auto imageId = url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);;

            if (provider->imageType() == QQmlImageProviderBase::Image)
                return new ImageProviderAsyncAdaptor(static_cast<QQuickImageProvider *>(provider), imageId, requestedSize);
            if (provider->imageType() == QQmlImageProviderBase::ImageResponse)
                return static_cast<QQuickAsyncImageProvider *>(provider)->requestImageResponse(imageId, requestedSize);

            return nullptr;
        }
        else
        {
            return new LocalImageResponse(url, requestedSize);
        }
    }
}

RoundImage::RoundImage(QQuickItem *parent) : QQuickItem {parent}
{
    if (window() || qGuiApp)
        setDPR(window() ? window()->devicePixelRatio() : qGuiApp->devicePixelRatio());

    connect(this, &QQuickItem::heightChanged, this, &RoundImage::regenerateRoundImage);
    connect(this, &QQuickItem::widthChanged, this, &RoundImage::regenerateRoundImage);
}

RoundImage::~RoundImage()
{
    resetImageRequest();
}

QSGNode *RoundImage::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto node = static_cast<QSGImageNode *>(oldNode);

    if (m_roundImage.isNull())
    {
        delete oldNode;
        m_dirty = false;
        return nullptr;
    }

    if (!node)
    {
        assert(window());
        node = window()->createImageNode();
        assert(node);
        node->setOwnsTexture(true);
    }

    if (m_dirty)
    {
        m_dirty = false;
        assert(window());

        QQuickWindow::CreateTextureOptions flags = QQuickWindow::TextureCanUseAtlas;
        if (Q_LIKELY(m_roundImage.hasAlphaChannel()))
            flags |= QQuickWindow::TextureHasAlphaChannel;

        QSGTexture* texture = window()->createTextureFromImage(m_roundImage, flags);

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

    node->setRect(boundingRect());

    return node;
}

void RoundImage::componentComplete()
{
    QQuickItem::componentComplete();

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

void RoundImage::setSource(const QUrl& source)
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

void RoundImage::handleImageRequestFinished()
{
    const QString error = m_activeImageRequest->errorString();
    QImage image;
    if (auto textureFactory = m_activeImageRequest->textureFactory())
    {
        image = textureFactory->image();
        delete textureFactory;
    }

    resetImageRequest();

    if (image.isNull())
    {
        qDebug() << "failed to get image, error" << error << source();
        return;
    }

    const qreal scaledWidth = this->width() * m_dpr;
    const qreal scaledHeight = this->height() * m_dpr;
    const qreal scaledRadius = this->radius() * m_dpr;

    const ImageCacheKey key {source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius};

    // Image is generated in size factor of `m_dpr` to avoid scaling artefacts when
    // generated image is set with device pixel ratio
    m_roundImageGenerator.reset(new RoundImageGenerator(image, scaledWidth, scaledHeight, scaledRadius));
    connect(m_roundImageGenerator.get(), &BaseAsyncTask::result, this, [this, key]()
    {
        const auto image = new QImage(m_roundImageGenerator->takeResult());

        m_roundImageGenerator.reset();

        if (image->isNull())
        {
            delete image;
            setRoundImage({});
            return;
        }

        image->setDevicePixelRatio(m_dpr);
        setRoundImage(*image);

        imageCache.insert(key, image, image->sizeInBytes());
    });

    m_roundImageGenerator->start(*QThreadPool::globalInstance());
}

void RoundImage::resetImageRequest()
{
    if (!m_activeImageRequest)
        return;

    m_activeImageRequest->disconnect(this);
    m_activeImageRequest->deleteLater();
    m_activeImageRequest = nullptr;
}

void RoundImage::load()
{
    m_enqueuedGeneration = false;
    assert(!m_roundImageGenerator);

    auto engine = qmlEngine(this);
    if (!engine || m_source.isEmpty() || !size().isValid() || size().isEmpty())
        return;

    const qreal scaledWidth = this->width() * m_dpr;
    const qreal scaledHeight = this->height() * m_dpr;
    const qreal scaledRadius = this->radius() * m_dpr;

    const ImageCacheKey key {source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius};
    if (auto image = imageCache.object(key)) // should only by called in mainthread
    {
        setRoundImage(*image);
        return;
    }

    m_activeImageRequest = getAsyncImageResponse(source(), QSizeF {scaledWidth, scaledHeight}.toSize(), engine);
    connect(m_activeImageRequest, &QQuickImageResponse::finished, this, &RoundImage::handleImageRequestFinished);
}

void RoundImage::setRoundImage(QImage image)
{
    m_dirty = true;
    m_roundImage = image;

    // remove old contents, setting ItemHasContent to false will
    // inhibit updatePaintNode() call and old content will remain
    if (image.isNull())
        update();

    setFlag(ItemHasContents, not image.isNull());
    update();
}

void RoundImage::regenerateRoundImage()
{
    if (!isComponentComplete() || m_enqueuedGeneration)
        return;

    // remove old contents
    setRoundImage({});

    resetImageRequest();

    m_roundImageGenerator.reset();

    // use Qt::QueuedConnection to delay generation, so that dependent properties
    // subsequent updates can be merged, f.e when VLCStyle.scale changes
    m_enqueuedGeneration = true;

    QMetaObject::invokeMethod(this, &RoundImage::load, Qt::QueuedConnection);
}

RoundImage::RoundImageGenerator::RoundImageGenerator(const QImage &sourceImage, qreal width, qreal height, qreal radius)
    : sourceImage(sourceImage)
    , width(width)
    , height(height)
    , radius(radius)
{
}

QImage RoundImage::RoundImageGenerator::execute()
{
    if (width <= 0 || height <= 0 || sourceImage.isNull())
        return {};

    QImage target(width, height, QImage::Format_ARGB32_Premultiplied);
    if (target.isNull())
        return target;

    target.fill(Qt::transparent);

    {
        QPainter painter(&target);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        QPainterPath path;
        path.addRoundedRect(0, 0, width, height, radius, radius);
        painter.setClipPath(path);

        // do PreserveAspectCrop
        const auto imageSize = sourceImage.size();
        const QPointF alignedCenteredTopLeft {(width - imageSize.width()) / 2., (height - imageSize.height()) / 2.};
        painter.drawImage(QRectF {alignedCenteredTopLeft, imageSize}, sourceImage);
    }

    return target;
}
