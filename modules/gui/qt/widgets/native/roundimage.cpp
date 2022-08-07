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
#include "util/qsgroundedrectangularimagenode.hpp"

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

    QImage prepareImage(const QSize &targetSize, const qreal radius, const QImage sourceImage)
    {
        if (qFuzzyIsNull(radius))
        {
            return sourceImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

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

        QImage execute() override
        {
            QImageReader reader;
            reader.setDevice(device);
            const QSize sourceSize = reader.size();

            if (requestedSize.isValid())
                reader.setScaledSize(sourceSize.scaled(requestedSize, Qt::KeepAspectRatioByExpanding));

            auto img = reader.read();
            errorStr = reader.errorString();

            if (!errorStr.isEmpty())
                img = prepareImage(requestedSize.isValid() ? requestedSize : img.size(), radius, img);

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

            QImage execute() override
            {
                return prepareImage(sourceImg.size(), radius, sourceImg);
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

            assert(provider->imageType() == QQmlImageProviderBase::ImageResponse);

            const auto imageId = url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);;

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
    if (Q_LIKELY(qGuiApp))
        setDPR(qGuiApp->devicePixelRatio());

    connect(this, &QQuickItem::heightChanged, this, &RoundImage::regenerateRoundImage);
    connect(this, &QQuickItem::widthChanged, this, &RoundImage::regenerateRoundImage);

    connect(this, &QQuickItem::windowChanged, this, [this](const QQuickWindow* const window) {
        if (window)
            setDPR(window->devicePixelRatio());
        else if (Q_LIKELY(qGuiApp))
            setDPR(qGuiApp->devicePixelRatio());
    });

    connect(this, &QQuickItem::windowChanged, this, &RoundImage::adjustQSGCustomGeometry);
}

RoundImage::~RoundImage()
{
    resetImageResponse(true);
}

QSGNode *RoundImage::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto customImageNode = dynamic_cast<QSGRoundedRectangularImageNode*>(oldNode);
    auto imageNode = dynamic_cast<QSGImageNode*>(oldNode);

    if (Q_UNLIKELY(oldNode && ((m_QSGCustomGeometry && !customImageNode)
                               || (!m_QSGCustomGeometry && !imageNode))))
    {
        // This must be extremely unlikely.
        // Assigned to different window with different renderer?
        delete oldNode;
        oldNode = nullptr;
    }

    if (m_roundImage.isNull())
    {
        delete oldNode;
        m_dirty = false;
        return nullptr;
    }

    if (!oldNode)
    {
        if (m_QSGCustomGeometry)
        {
            customImageNode = new QSGRoundedRectangularImageNode;
        }
        else
        {
            assert(window());
            imageNode = window()->createImageNode();
            assert(imageNode);
            imageNode->setOwnsTexture(true);
        }
    }

    if (m_dirty)
    {
        m_dirty = false;
        assert(window());

        QQuickWindow::CreateTextureOptions flags = QQuickWindow::TextureCanUseAtlas;

        if (!m_roundImage.hasAlphaChannel())
            flags |= QQuickWindow::TextureIsOpaque;

        if (std::unique_ptr<QSGTexture> texture { window()->createTextureFromImage(m_roundImage, flags) })
        {
            if (m_QSGCustomGeometry)
            {
                customImageNode->setTexture(std::move(texture));
            }
            else
            {
                // No need to delete the old texture manually as it is owned by the node.
                imageNode->setTexture(texture.release());
            }
        }
        else
        {
            qmlWarning(this) << "Could not generate texture from " << m_roundImage;
        }
    }

    // Geometry:
    if (m_QSGCustomGeometry)
    {
        customImageNode->setShape({boundingRect(), radius()});
        customImageNode->setSmooth(smooth());
        assert(customImageNode->geometry() && customImageNode->material());
        return customImageNode;
    }
    else
    {
        imageNode->setRect(boundingRect());
        imageNode->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
        return imageNode;
    }
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

RoundImage::Status RoundImage::status() const
{
    return m_status;
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

void RoundImage::handleImageResponseFinished()
{
    const QString error = m_activeImageResponse->errorString();
    QImage image;
    if (auto textureFactory = m_activeImageResponse->textureFactory())
    {
        image = textureFactory->image();
        delete textureFactory;
    }

    resetImageResponse(false);

    if (image.isNull())
    {
        setStatus(Status::Error);
        qDebug() << "failed to get image, error" << error << source();
        return;
    }

    image.setDevicePixelRatio(m_dpr);
    setRoundImage(image);
    setStatus(Status::Ready);

    // FIXME: These should be gathered from the generator:
    const qreal scaledWidth = this->width() * m_dpr;
    const qreal scaledHeight = this->height() * m_dpr;
    const qreal scaledRadius = m_QSGCustomGeometry ? 0.0 : (this->radius() * m_dpr);

    const ImageCacheKey key {source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius};
    imageCache.insert(key, new QImage(image), image.sizeInBytes());
}

void RoundImage::resetImageResponse(bool cancel)
{
    if (!m_activeImageResponse)
        return;

    if (cancel)
        m_activeImageResponse->cancel();

    m_activeImageResponse->disconnect(this);
    m_activeImageResponse = nullptr;
}

void RoundImage::load()
{
    m_enqueuedGeneration = false;

    auto engine = qmlEngine(this);
    if (!engine || m_source.isEmpty() || !size().isValid() || size().isEmpty())
        return;

    const qreal scaledWidth = this->width() * m_dpr;
    const qreal scaledHeight = this->height() * m_dpr;
    const qreal scaledRadius = m_QSGCustomGeometry ? 0.0 : (this->radius() * m_dpr);

    const ImageCacheKey key {source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius};
    if (auto image = imageCache.object(key)) // should only by called in mainthread
    {
        setRoundImage(*image);
        setStatus(Status::Ready);
        return;
    }

    m_activeImageResponse = getAsyncImageResponse(source(), QSizeF {scaledWidth, scaledHeight}.toSize(), scaledRadius, engine);

    connect(m_activeImageResponse, &QQuickImageResponse::finished, this, &RoundImage::handleImageResponseFinished);
    connect(m_activeImageResponse, &QQuickImageResponse::finished, m_activeImageResponse, &QObject::deleteLater);
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

void RoundImage::setStatus(const RoundImage::Status status)
{
    if (status == m_status)
        return;

    m_status = status;
    emit statusChanged();
}

void RoundImage::regenerateRoundImage()
{
    if (!isComponentComplete() || m_enqueuedGeneration)
        return;

    setStatus(source().isEmpty() ? Status::Null : Status::Loading);

    // remove old contents
    setRoundImage({});

    resetImageResponse(true);

    // use Qt::QueuedConnection to delay generation, so that dependent properties
    // subsequent updates can be merged, f.e when VLCStyle.scale changes
    m_enqueuedGeneration = true;

    QMetaObject::invokeMethod(this, &RoundImage::load, Qt::QueuedConnection);
}

void RoundImage::adjustQSGCustomGeometry(const QQuickWindow* const window)
{
    if (!window) return;

    // No need to check if the scene graph is initialized according to docs.

    const auto enableCustomGeometry = [this, window]() {
        if (m_QSGCustomGeometry)
            return;

        // Favor custom geometry instead of clipping the image.
        // This allows making the texture opaque, as long as
        // source image is also opaque, for optimization
        // purposes.
        if (window->format().samples() != -1)
            m_QSGCustomGeometry = true;
        // No need to regenerate as transparent part will not
        // matter. However, in order for the material to not
        // require blending, a regeneration is necessary.
        // We could force the material to not require blending
        // for the outer transparent part which is not within the
        // geometry, but then inherently transparent images would
        // not be rendered correctly due to the alpha channel.

        QMetaObject::invokeMethod(this,
                                  &RoundImage::regenerateRoundImage,
                                  Qt::QueuedConnection);

        // It might be tempting to not regenerate the image on size
        // change. However;
        // If upscaled, we don't know if the image can be provided
        // in a higher resolution.
        // If downscaled, we would like to free some used memory.
        // On the other hand, there is no need to regenerate the
        // image when the radius changes. This behavior does not
        // mean that the radius can be animated. Although possible,
        // the custom geometry node is not designed to handle animations.
        disconnect(this, &RoundImage::radiusChanged, this, &RoundImage::regenerateRoundImage);
        connect(this, &RoundImage::radiusChanged, this, &QQuickItem::update);
    };

    const auto disableCustomGeometry = [this]() {
        if (!m_QSGCustomGeometry)
            return;

        m_QSGCustomGeometry = false;

        QMetaObject::invokeMethod(this,
                                  &RoundImage::regenerateRoundImage,
                                  Qt::QueuedConnection);

        connect(this, &RoundImage::radiusChanged, this, &RoundImage::regenerateRoundImage);
        disconnect(this, &RoundImage::radiusChanged, this, &QQuickItem::update);
    };


    if (window->rendererInterface()->graphicsApi() == QSGRendererInterface::GraphicsApi::OpenGL)
    {
        // Direct OpenGL, enable custom geometry:
        enableCustomGeometry();
    }
    else
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        // QSG(Opaque)TextureMaterial supports Qt RHI,
        // so there is no obstacle using custom geometry
        // if Qt RHI is in use.
        if (QSGRendererInterface::isApiRhiBased(window->rendererInterface()->graphicsApi()))
            enableCustomGeometry();
        else
            disableCustomGeometry();
#else
        // Qt RHI is introduced in Qt 5.14.
        // QSG(Opaque)TextureMaterial does not support any graphics API other than OpenGL
        // without the Qt RHI abstraction layer.
        disableCustomGeometry();
#endif
    }
}
