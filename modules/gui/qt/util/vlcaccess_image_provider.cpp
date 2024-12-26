/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include <vlc_access.h>
#include <vlc_stream.h>

#include "vlcaccess_image_provider.hpp"

#include "util/asynctask.hpp"
#include "qt.hpp"

#include <QImageReader>
#include <QFile>
#include <QQmlFile>
#include <QUrlQuery>

#define PATH_KEY "uri"

namespace {

class ImageReader : public AsyncTask<QImage>
{
public:
    /**
     * @brief ImageReader
     * @param device i/o source to read from
     *
     * @param requestedSize only taken as hint, the Image is resized with PreserveAspectCrop
     *
     * @param radius
     */
    ImageReader(std::unique_ptr<QIODevice> device, QSize requestedSize, VLCAccessImageProvider::ImagePostProcessCb postProcessCb)
        : device(std::move(device))
        , requestedSize {requestedSize}
        , postProcessCb {postProcessCb}
    {
    }

    QString errorString() const {
        return errorStr;
    }

    QImage execute() override
    {
        QImageReader reader;
        reader.setDevice(device.get());
        const QSize sourceSize = reader.size();

        if (requestedSize.isValid())
            reader.setScaledSize(sourceSize.scaled(requestedSize, Qt::KeepAspectRatioByExpanding));

        auto img = reader.read();

        if (img.isNull()) {
            errorStr = reader.errorString();
        }

        if (!img.isNull() && postProcessCb)
            img = postProcessCb(img, requestedSize);

        return img;
    }

private:
    std::unique_ptr<QIODevice> device;
    QSize requestedSize;
    QString errorStr;
    VLCAccessImageProvider::ImagePostProcessCb postProcessCb;
};


class VLCAccessImageResponse : public QQuickImageResponse
{
public:
    VLCAccessImageResponse(const QUrl& url, const QSize &requestedSize, VLCAccessImageProvider::ImagePostProcessCb postProcessCb = nullptr)
        : imagePostProcessCb(postProcessCb)
    {

        std::unique_ptr<QIODevice> device;
        if (url.scheme().compare(QStringLiteral("qrc"), Qt::CaseInsensitive) == 0)
        {
            QString qrcPath = QQmlFile::urlToLocalFileOrQrc(url);
            device = std::make_unique<QFile>(qrcPath);
        }
        else
        {
            QUrl fileUrl = url;
            if (fileUrl.scheme().isEmpty())
                fileUrl.setScheme("file");
            device = std::make_unique<VLCIODevice>(fileUrl.toString(QUrl::FullyEncoded));
        }
        reader.reset(new ImageReader(std::move(device), requestedSize, postProcessCb));

        connect(reader.get(), &ImageReader::result, this, &VLCAccessImageResponse::handleImageRead);

        reader->start(*QThreadPool::globalInstance());
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return result.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(result);
    }

    QString errorString() const override
    {
        if (result.isNull() && errorStr.isEmpty())
            return QStringLiteral("Unspecified error.");
        return errorStr;
    }

    void cancel() override
    {
        reader.reset();
        emit finished();
    }

private:
    void handleImageRead()
    {
        result = reader->takeResult();
        errorStr = reader->errorString();
        reader.reset();

        emit finished();
    }

    VLCAccessImageProvider::ImagePostProcessCb imagePostProcessCb;
    QImage result;
    TaskHandle<ImageReader> reader;
    QString errorStr;
};

} // anonymous namespace

//// VLCAccessImageProvider

VLCAccessImageProvider::VLCAccessImageProvider(VLCAccessImageProvider::ImagePostProcessCb cb)
    : QQuickAsyncImageProvider()
    , postProcessCb(cb)
{
}

QQuickImageResponse* VLCAccessImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    QUrl url {id};
    QUrlQuery query {url};
    if (!query.hasQueryItem(PATH_KEY))
        return nullptr;

    QString vlcurl = query.queryItemValue(PATH_KEY, QUrl::FullyDecoded);
    return new VLCAccessImageResponse(QUrl::fromEncoded(vlcurl.toUtf8()), requestedSize, postProcessCb);
}

QString VLCAccessImageProvider::wrapUri(QString path)
{
    QUrlQuery query;
    query.addQueryItem(PATH_KEY, path);
    return QStringLiteral("image://vlcaccess/?") + query.toString(QUrl::FullyEncoded);
}

QQuickImageResponse* VLCAccessImageProvider::requestImageResponseUnWrapped(const QUrl url, const QSize &requestedSize, VLCAccessImageProvider::ImagePostProcessCb cb)
{
    return new VLCAccessImageResponse(url, requestedSize, cb);
}

//// VLCImageAccess

VLCAccessImage::VLCAccessImage(QObject* parent)
    : QObject(parent)
{}

QString VLCAccessImage::uri(const QString& path, const bool excludeLocalFileOrUnknownScheme)
{
    if (excludeLocalFileOrUnknownScheme)
    {
        const QUrl url(path);
        if (url.scheme().isEmpty() || url.scheme() == QLatin1String("qrc") || url.scheme() == QLatin1String("file"))
            return path;
    }
    return VLCAccessImageProvider::wrapUri(path);
}

//// VLCIODevice

VLCIODevice::VLCIODevice(const QString& filename, QObject* parent)
    : QIODevice(parent)
    , m_filename(filename)
{
}

VLCIODevice::~VLCIODevice()
{
    close();
}

bool VLCIODevice::open(OpenMode mode)
{
    //we only support reading
    if (mode & QIODevice::OpenModeFlag::WriteOnly)
        return false;

    m_stream = vlc_access_NewMRL(nullptr, qtu(m_filename));
    if (m_stream == nullptr)
        return false;

    return QIODevice::open(mode);
}

bool VLCIODevice::isSequential() const
{
    assert(m_stream);
    //some access (like http) will perform really poorly with the way
    //Qt uses the QIODevice (lots of seeks)
    //return !vlc_stream_CanSeek(m_stream);
    return true;
}

void VLCIODevice::close()
{
    if (!m_stream)
        return;
    vlc_stream_Delete(m_stream);
    m_stream = nullptr;
}

qint64 VLCIODevice::pos() const
{
    assert(m_stream);
    return vlc_stream_Tell(m_stream);
}

qint64 VLCIODevice::size() const
{
    assert(m_stream);
    uint64_t streamSize;
    bool ret = vlc_stream_GetSize(m_stream, &streamSize);
    if (!ret)
        return -1;
    return static_cast<qint64>(streamSize);
}

bool VLCIODevice::seek(qint64 pos)
{
    assert(m_stream);
    QIODevice::seek(pos);
    if (pos < 0)
        return false;
    return vlc_stream_Seek(m_stream, pos) == VLC_SUCCESS;
}

bool VLCIODevice::atEnd() const
{
    assert(m_stream);
    return vlc_stream_Eof(m_stream);
}

bool VLCIODevice::reset()
{
    assert(m_stream);
    return  vlc_stream_Seek(m_stream, 0) == VLC_SUCCESS;
}

qint64 VLCIODevice::readData(char* data, qint64 maxlen)
{
    assert(m_stream);
    return vlc_stream_Read(m_stream, data, maxlen);
}

qint64 VLCIODevice::writeData(const char*, qint64)
{
    assert(m_stream);
    return -1;
}
