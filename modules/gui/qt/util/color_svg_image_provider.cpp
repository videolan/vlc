/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#include "color_svg_image_provider.hpp"
#include "qt.hpp"
#include "util/asynctask.hpp"
#include "util/imagehelper.hpp"

#include <QUrl>
#include <QUrlQuery>
#include <QPainter>
#include <QFile>
#include <QQmlFile>
#include <QDebug>
#include <QBuffer>
#include <QImageIOHandler>

static const QMap<QString, QString> predefinedSubst = {
    {COLOR1_KEY, "#FF00FF"},
    {COLOR2_KEY, "#00FFFF"},
    {COLOR_ACCENT_KEY, "#FF8800"},
};

QPair<QByteArray, QColor> colorizeSvg(const QString &filename, const QList<QPair<QString, QString> > &replacements)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "SVGColorizer: can not open file " << filename << " for read.";
        return {};
    }

    QByteArray data = file.readAll();
    //we pass by QString because we want to perform case incensite replacements
    QString dataStr = QString::fromUtf8(data);

    QColor backgroundColor;

    for (const auto& replacePair: replacements)
    {
        if (replacePair.first == PATH_KEY)
            continue;
        else if (replacePair.first == BACKGROUND_KEY)
        {
            backgroundColor = QColor(replacePair.second);
        }
        else if (predefinedSubst.contains(replacePair.first))
        {
            dataStr = dataStr.replace(predefinedSubst[replacePair.first],
                                      replacePair.second, Qt::CaseInsensitive);
        }
        else
        {
            dataStr = dataStr.replace(replacePair.first, replacePair.second, Qt::CaseInsensitive);
        }
    }

    return {dataStr.toUtf8(), backgroundColor};
}

namespace {

class SVGColorImageReader: public AsyncTask<QImage>
{
public:
    SVGColorImageReader(const QString& filename, const QList<QPair<QString, QString>>& replacements, QSize requestedSize)
        : AsyncTask<QImage>()
        , m_fileName(filename)
        , m_requestedSize(requestedSize)
        , m_replacements(replacements)
    {
        if (!m_requestedSize.isValid())
            m_requestedSize = QSize{256, 256};
    }

    QImage execute() override
    {
        if (m_requestedSize.width() < 0 || m_requestedSize.height() < 0)
        {
            m_error = "invalid size requested";
            return {};
        }

        const auto data = colorizeSvg(m_fileName, m_replacements);

        if (data.first.isEmpty())
        {
            m_error = "file can't be opened";
            return {};
        }

        const auto svgHandler = QScopedPointer<QImageIOHandler>(ImageHelper::createSvgImageIOHandler());

        QImage image;

        if (svgHandler)
        {
            QBuffer buffer;
            buffer.setData(data.first);

            if (Q_LIKELY(buffer.open(QFile::ReadOnly)))
            {
                svgHandler->setDevice(&buffer);

                if (Q_LIKELY(svgHandler->canRead()))
                {
                    QSize scaledSize;

                    if ((m_requestedSize.width() == 0 || m_requestedSize.height() == 0))
                    {
                        // QImageReader standard behavior, if width or height is 0,
                        // it is calculated to preserve the aspect ratio:

                        const QSize naturalSize = svgHandler->option(QImageIOHandler::Size).toSize();

                        if (m_requestedSize.width() == 0)
                        {
                            scaledSize.setWidth(m_requestedSize.height() * ((qreal)naturalSize.width() / naturalSize.height()));
                            scaledSize.setHeight(m_requestedSize.height());
                        }
                        else if (m_requestedSize.height() == 0)
                        {
                            scaledSize.setHeight(m_requestedSize.width() * ((qreal)naturalSize.height() / naturalSize.width()));
                            scaledSize.setWidth(m_requestedSize.width());
                        }
                    }
                    else
                    {
                        scaledSize = m_requestedSize;
                    }

                    if (!scaledSize.isEmpty())
                        svgHandler->setOption(QImageIOHandler::ScaledSize, scaledSize);

                    if (data.second.isValid())
                        svgHandler->setOption(QImageIOHandler::BackgroundColor, data.second);

                    svgHandler->read(&image);
                }
                else
                {
                    m_error = QStringLiteral("Svg Image Provider: svg image handler can not read the svg contents, is the file svg specification compliant?");
                }
            }
            else
            {
                m_error = QStringLiteral("Svg Image Provider: can not open colorized svg buffer for read.");
            }
        }
        else
        {
            m_error = QStringLiteral("Svg Image Provider: can not found QSvgPlugin, is it installed?");
        }

        if (Q_UNLIKELY(image.isNull()))
        {
            if (m_error.isEmpty())
                m_error = QStringLiteral("Svg Image Provider: unspecified error.");
            image = QImage(m_requestedSize, QImage::Format_RGB32);
            image.fill(QColor("purple"));
        }

        return image;
    }

    QString errorString() const
    {
        return m_error;
    }

private:
    QString m_fileName;
    QString m_error;
    QSize m_requestedSize;
    QList<QPair<QString, QString>> m_replacements;
};

class SVGColorImageResponse : public QQuickImageResponse
{
public:
    SVGColorImageResponse(const QString& fileName, const QList<QPair<QString, QString>>& arguments, const QSize& requestedSize)
    {
        m_reader.reset(new SVGColorImageReader(fileName, arguments, requestedSize));

        connect(m_reader.get(), &SVGColorImageReader::result, this, &SVGColorImageResponse::handleImageRead);

        m_reader->start(*QThreadPool::globalInstance());
    }

    QQuickTextureFactory* textureFactory() const override
    {
        return m_result.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(m_result);
    }

    QString errorString() const override
    {
        if (m_result.isNull() && m_error.isEmpty())
            return QStringLiteral("Unspecified error.");
        return m_error;
    }

    void cancel() override {
        m_reader.reset();
        emit finished();
    }

private:
    void handleImageRead()
    {
        m_result = m_reader->takeResult();
        if (m_result.isNull())
            m_error = m_reader->errorString();
        m_reader.reset();

        emit finished();
    }

    TaskHandle<SVGColorImageReader> m_reader;
    QImage m_result;
    QString m_error;
};
}

QQuickImageResponse* SVGColorImageImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    QUrl url {id};
    QUrlQuery query {url};
    if (!query.hasQueryItem(PATH_KEY))
        return nullptr;

    QString fileurl  = QQmlFile::urlToLocalFileOrQrc(query.queryItemValue(PATH_KEY));

    QList<QPair<QString, QString>> queryItems = query.queryItems();

    return new SVGColorImageResponse(fileurl, queryItems, requestedSize);
}


SVGColorImageBuilder::SVGColorImageBuilder(const QString &path, QObject* parent)
    : QObject(parent)
{
    m_query.addQueryItem(PATH_KEY, path);
}

QString SVGColorImageBuilder::uri() const
{

    const QString ret = QStringLiteral("image://svgcolor/?") + m_query.toString(QUrl::FullyEncoded);
    return ret;
}

SVGColorImageBuilder* SVGColorImageBuilder::color1(QColor c1)
{
    m_query.addQueryItem(COLOR1_KEY, c1.name(QColor::HexRgb));
    return this;
}

SVGColorImageBuilder* SVGColorImageBuilder::color2(QColor c2)
{
    m_query.addQueryItem(COLOR2_KEY, c2.name(QColor::HexRgb));
    return this;
}

SVGColorImageBuilder* SVGColorImageBuilder::accent(QColor c2)
{
    m_query.addQueryItem(COLOR_ACCENT_KEY, c2.name(QColor::HexRgb));
    return this;
}

SVGColorImageBuilder* SVGColorImageBuilder::any(QVariantMap map)
{
    for (auto kv = map.constBegin(); kv != map.constEnd(); ++kv )
    {
        const QVariant& val = kv.value();
        if (val.canConvert<QColor>())
            m_query.addQueryItem(kv.key(), val.value<QColor>().name(QColor::HexRgb));
        else if (val.canConvert<QString>())
            m_query.addQueryItem(kv.key(), val.toString());
        else
            qWarning() << "can't serialize value" << val;
    }
    return this;
}

SVGColorImageBuilder* SVGColorImageBuilder::background(QColor bg)
{
    m_query.addQueryItem(BACKGROUND_KEY, bg.name(QColor::HexArgb));
    return this;
}


SVGColorImage::SVGColorImage(QObject* parent)
    : QObject(parent)
{}

SVGColorImageBuilder* SVGColorImage::colorize(const QString &path)
{
    SVGColorImageBuilder* builder = new SVGColorImageBuilder(path);
    QQmlEngine::setObjectOwnership(builder, QQmlEngine::JavaScriptOwnership);
    return builder;
}
