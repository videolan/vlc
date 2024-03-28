/*****************************************************************************
 * roundimage.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2023 the VideoLAN team
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

#ifndef VLC_ROUNDIMAGE_P_HPP
#define VLC_ROUNDIMAGE_P_HPP

#include <QObject>
#include <QUrl>
#include <QCache>
#include <QQuickImageProvider>

#include <unordered_map>
#include <memory>

#include "roundimage.hpp"

struct ImageCacheKey
{
    ImageCacheKey(QUrl url, QSize size, qreal radius)
        : url(url)
        , size(size)
        , radius(radius)
    {}

    QUrl url;
    QSize size;
    qreal radius;
};

bool operator ==(const ImageCacheKey &lhs, const ImageCacheKey &rhs);
uint qHash(const ImageCacheKey &key, uint seed);

//std hash version is required for unordered_map
template<>
struct std::hash<ImageCacheKey>
{
    std::size_t operator()(const ImageCacheKey& s) const noexcept
    {
        return qHash(s, 0); // or use boost::hash_combine
    }
};


/**
 * @brief The RoundImageRequest class represent a request for a RoundImage generation
 * it will be shared amongst the diffent RoundImage instance that preforms the same request
 * simultaneously, and will notify upon completion or failure through the requestCompleted signal
 */
class RoundImageRequest : public QObject
{
    Q_OBJECT
public:
    RoundImageRequest(const ImageCacheKey& key, qreal dpr, QQmlEngine *engine);

    ~RoundImageRequest();

    inline RoundImage::Status getStatus() const
    {
        return m_status;
    }

    void handleImageResponseFinished();

    void saveInCache();

signals:
    void requestCompleted(RoundImage::Status status, QImage image);

private:
    QQuickImageResponse *getAsyncImageResponse(const QUrl &url, const QSize &requestedSize, const qreal radius, QQmlEngine *engine);

    bool m_cancelOnDelete = true;
    const ImageCacheKey m_key;
    qreal m_dpr;
    QQuickImageResponse* m_imageResponse = nullptr;
    RoundImage::Status m_status = RoundImage::Status::Loading;
    bool m_saveInCache = false;
};

/**
 * @brief The RoundImageCache class contains the cache of generated round images
 *
 * principle is the folowing:
 *
 * @startuml
 * (*) --> if image already in cache then
 *   --> [true] "use image from the cache"
 *   --> "set image in Ready state"
 *   --> (*)
 * else
 *   --> [false] "set image Loading state"
 *   --> if pending requests already exists for the source
 *       --> [true] reference pending request
 *       --> "wait for request completion" as wait
 *   else
 *       --> [false] create a new request
 *       --> wait
 *   endif
 *   --> if request succeed
 *         --> [true] "store image result in cache"
 *         --> "use request image result"
 *         --> "set image in Ready state"
 *         --> (*)
 *       else
 *         --> [false] "set the image in Error state"
 *         --> (*)
 *      endif
 * endif
 * @enduml
 *
 * notes that:
 * - requests are refcounted, if no image reference a request anymore,
 *   the request is canceled and destroyed
 *
 * - failed atempts are not stored, if the same resource is requested latter on,
 *   a new request will be created and executed for this request
 */
class RoundImageCache
{
public:
    RoundImageCache();

    inline QImage* object(const ImageCacheKey& key) const
    {
        return m_imageCache.object(key);
    }

    inline void insert(const ImageCacheKey& key, QImage* image, int size)
    {
        m_imageCache.insert(key, image, size);
    }

    std::shared_ptr<RoundImageRequest> requestImage(const ImageCacheKey& key, qreal dpr, QQmlEngine *engine);

    void removeRequest(const ImageCacheKey& key);

private:
    //images are cached (result of RoundImageGenerator) with the cost calculated from QImage::sizeInBytes
    QCache<ImageCacheKey, QImage> m_imageCache;

    //contains the pending request, we use a weak ptr here as the request may be canceled and destroyed
    //when all RoundImage that requested the image drop the request. user should check for validity before
    //accessing the RoundImageRequest
    std::unordered_map<ImageCacheKey, std::weak_ptr<RoundImageRequest>> m_pendingRequests;
};

#endif // VLC_ROUNDIMAGE_P_HPP
