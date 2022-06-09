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
#include "util/asynctask.hpp"

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
    QCache<ImageCacheKey, QImage> imageCache(32 * 1024 * 1024); // 32 MiB

    QImage applyRadius(const QSize &targetSize, const qreal radius, const QImage sourceImage)
    {
        QImage target(targetSize, QImage::Format_ARGB32_Premultiplied);
        if (target.isNull())
            return target;

        target.fill(Qt::transparent);

        {
            QPainter painter(&target);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

            QPainterPath path;
            path.addRoundedRect(0, 0, targetSize.width(), targetSize.height(), radius, radius);
            painter.setClipPath(path);

            // do PreserveAspectCrop
            const auto imageSize = sourceImage.size();
            const QPointF alignedCenteredTopLeft {(targetSize.width() - imageSize.width()) / 2.
                                                 , (targetSize.height() - imageSize.height()) / 2.};
            painter.drawImage(QRectF {alignedCenteredTopLeft, imageSize}, sourceImage);
        }

        return target;
    }

    class ImageReader : public AsyncTask<QImage>
    {
    public:
        // requestedSize is only taken as hint, the Image is resized with PreserveAspectCrop
        ImageReader(QIODevice *device, QSize requestedSize, const qreal radius)
            : device {device}
            , requestedSize {requestedSize}
            , radius {radius}
        {
        }

        QString errorString() const { return errorStr; }

        QImage execute()
        {
            QImageReader reader;
            reader.setDevice(device);
            const QSize sourceSize = reader.size();

            if (requestedSize.isValid())
                reader.setScaledSize(sourceSize.scaled(requestedSize, Qt::KeepAspectRatioByExpanding));

            auto img = reader.read();
            errorStr = reader.errorString();

            if (!errorStr.isEmpty())
                img = applyRadius(requestedSize.isValid() ? requestedSize : img.size(), radius, img);

            return img;
        }

    private:
        QIODevice *device;
        QSize requestedSize;
        qreal radius;
        QString errorStr;
    };

    class LocalImageResponse : public QQuickImageResponse
    {
    public:
        LocalImageResponse(const QString &fileName, const QSize &requestedSize, const qreal radius)
        {
            auto file = new QFile(fileName);
            reader.reset(new ImageReader(file, requestedSize, radius));
            file->setParent(reader.get());

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

#ifdef QT_NETWORK_LIB
    class NetworkImageResponse : public QQuickImageResponse
    {
    public:
        NetworkImageResponse(QNetworkReply *reply, QSize requestedSize, const qreal radius)
            : reply {reply}
            , requestedSize {requestedSize}
            , radius {radius}
        {
            QObject::connect(reply, &QNetworkReply::finished
                             , this, &NetworkImageResponse::handleNetworkReplyFinished);
        }

        QQuickTextureFactory *textureFactory() const override
        {
            return result.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(result);
        }

        QString errorString() const override
        {
            return error;
        }

        void cancel() override
        {
            if (reply->isRunning())
                reply->abort();

            reader.reset();
        }

    private:
        void handleNetworkReplyFinished()
        {
            if (reply->error() != QNetworkReply::NoError)
            {
                error = reply->errorString();
                emit finished();
                return;
            }

            reader.reset(new ImageReader(reply, requestedSize, radius));
            QObject::connect(reader.get(), &ImageReader::result, this, [this]()
            {
                result = reader->takeResult();
                error = reader->errorString();
                reader.reset();

                emit finished();
            });

            reader->start(*QThreadPool::globalInstance());
        }

        QNetworkReply *reply;
        QSize requestedSize;
        qreal radius;
        TaskHandle<ImageReader> reader;
        QImage result;
        QString error;
    };
#endif

    class ImageProviderAsyncAdaptor : public QQuickImageResponse
    {
    public:
        ImageProviderAsyncAdaptor(QQuickImageProvider *provider, const QString &id, const QSize &requestedSize, const qreal radius)
        {
            task.reset(new ProviderImageGetter(provider, id, requestedSize, radius));
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
            ProviderImageGetter(QQuickImageProvider *provider, const QString &id, const QSize &requestedSize, const qreal radius)
                : provider {provider}
                , id{id}
                , requestedSize{requestedSize}
                , radius {radius}
            {
            }

            QImage execute() override
            {
                auto img = provider->requestImage(id, &sourceSize, requestedSize);
                if (!img.isNull())
                {
                    QSize targetSize = sourceSize;
                    if (requestedSize.isValid())
                        targetSize.scale(requestedSize, Qt::KeepAspectRatioByExpanding);

                    applyRadius(targetSize, radius, img);
                }

                return img;
            }

        private:
            QQuickImageProvider *provider;
            QString id;
            QSize requestedSize;
            qreal radius;
            QSize sourceSize;
        };

        TaskHandle<ProviderImageGetter> task;
        QImage result;
    };


    // adapts a given QQuickImageResponse to produce a image with radius
    class ImageResponseRadiusAdaptor : public QQuickImageResponse
    {
    public:
        ImageResponseRadiusAdaptor(QQuickImageResponse *response, const qreal radius) : response {response}, radius {radius}
        {
            response->setParent(this);
            connect(response, &QQuickImageResponse::finished
                    , this, &ImageResponseRadiusAdaptor::handleResponseFinished);
        }

        QString errorString() const override { return errStr; }

        QQuickTextureFactory *textureFactory() const override
        {
            return !result.isNull() ? QQuickTextureFactory::textureFactoryForImage(result) : nullptr;
        }

    private:
        class RoundImageGenerator : public AsyncTask<QImage>
        {
        public:
            RoundImageGenerator(const QImage &img, const qreal radius) : sourceImg {img}, radius{radius}
            {
            }

            QImage execute()
            {
                return applyRadius(sourceImg.size(), radius, sourceImg);
            }

        private:
            QImage sourceImg;
            qreal radius;
        };

        void handleResponseFinished()
        {
            errStr = response->errorString();
            auto textureFactory = std::unique_ptr<QQuickTextureFactory>(response->textureFactory());
            auto img = !textureFactory ? QImage {} : textureFactory->image();
            if (!textureFactory || img.isNull())
                return;

            response->disconnect(this);
            response->deleteLater();
            response = nullptr;

            generator.reset(new RoundImageGenerator(img, radius));
            connect(generator.get(), &RoundImageGenerator::result
                    , this, &ImageResponseRadiusAdaptor::handleGeneratorFinished);

            generator->start(*QThreadPool::globalInstance());
        }

        void handleGeneratorFinished()
        {
            result = generator->takeResult();

            emit finished();
        }

        QQuickImageResponse *response;
        QString errStr;
        qreal radius;
        QImage result;
        TaskHandle<RoundImageGenerator> generator;
    };

    QQuickImageResponse *getAsyncImageResponse(const QUrl &url, const QSize &requestedSize, const qreal radius, QQmlEngine *engine)
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
                return new ImageProviderAsyncAdaptor(static_cast<QQuickImageProvider *>(provider), imageId, requestedSize, radius);
            if (provider->imageType() == QQmlImageProviderBase::ImageResponse)
            {
                auto rawImageResponse = static_cast<QQuickAsyncImageProvider *>(provider)->requestImageResponse(imageId, requestedSize);
                return new ImageResponseRadiusAdaptor(rawImageResponse, radius);
            }

            return nullptr;
        }
        else if (QQmlFile::isLocalFile(url))
        {
            return new LocalImageResponse(QQmlFile::urlToLocalFileOrQrc(url), requestedSize, radius);
        }
#ifdef QT_NETWORK_LIB
        else
        {
            QNetworkRequest request(url);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            auto reply = engine->networkAccessManager()->get(request);
            return new NetworkImageResponse(reply, requestedSize, radius);
        }
#endif
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

    image.setDevicePixelRatio(m_dpr);
    setRoundImage(image);

    const qreal scaledWidth = this->width() * m_dpr;
    const qreal scaledHeight = this->height() * m_dpr;
    const qreal scaledRadius = this->radius() * m_dpr;

    const ImageCacheKey key {source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius};
    imageCache.insert(key, new QImage(image), image.sizeInBytes());
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

    m_activeImageRequest = getAsyncImageResponse(source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius, engine);
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

    // use Qt::QueuedConnection to delay generation, so that dependent properties
    // subsequent updates can be merged, f.e when VLCStyle.scale changes
    m_enqueuedGeneration = true;

    QMetaObject::invokeMethod(this, &RoundImage::load, Qt::QueuedConnection);
}
