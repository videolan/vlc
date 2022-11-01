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

#include <QSvgRenderer>
#include <QUrl>
#include <QUrlQuery>
#include <QPainter>
#include <QFile>
#include <QQmlFile>
#include <QDebug>

#define PATH_KEY "_res_PATH"
#define BACKGROUND_KEY "_res_BG"
#define COLOR1_KEY "_res_C1"
#define COLOR2_KEY "_res_C2"
#define COLOR_ACCENT_KEY "_res_ACCENT"

namespace {

static const QMap<QString, QString> predefinedSubst = {
    {COLOR1_KEY, "#FF00FF"},
    {COLOR2_KEY, "#00FFFF"},
    {COLOR_ACCENT_KEY, "#FF8800"},
};

class SVGColorImageReader: public AsyncTask<QImage>
{
public:
    SVGColorImageReader(const QString& filename, const QList<QPair<QString, QString>>& replacements, QSize requestedSize)
        : AsyncTask<QImage>()
        , m_requestedSize(requestedSize)
        , m_replacements(replacements)
    {
        m_file = new QFile(filename, this);
        if (!m_requestedSize.isValid())
            m_requestedSize = QSize{256, 256};
    }

    QImage execute() override
    {
        if (m_requestedSize.width() == 0 || m_requestedSize.height() == 0)
        {
            m_error = "invalid size requested";
            return {};
        }

        if (!m_file->open(QIODevice::ReadOnly))
        {
            m_error = "file can't be opened";
            return {};
        }

        QColor backgroundColor{Qt::transparent};

        QByteArray data = m_file->readAll();
        //we pass by QString because we want to perform case incensite replacements
        QString dataStr = QString::fromUtf8(data);
        for (const auto& replacePair: m_replacements)
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

        QSvgRenderer renderer(dataStr.toUtf8());
        if (!renderer.isValid())
        {
            m_error = "can't parse SVG content";
            return {};
        }

        //FIXME QT < 5.15 doesn't support QSvgRenderer::setAspectRatioMode
        //scale to fit manually
        QRect bounds;
        QSize bbox = renderer.defaultSize();
        float sourceAR = bbox.width() / (float)bbox.height();
        float destAR = m_requestedSize.width() / (float)m_requestedSize.height();
        if (qFuzzyCompare(sourceAR, destAR))
            bounds = QRect({0,0}, m_requestedSize);
        else if (sourceAR < destAR) {
            float scaledWidth = m_requestedSize.height() * sourceAR;
            bounds = QRect((m_requestedSize.width() - scaledWidth) / 2, 0,
                           scaledWidth, m_requestedSize.height());
        } else {
            float scaledHeight = m_requestedSize.width() / sourceAR;
            bounds = QRect(0, (m_requestedSize.height() - scaledHeight) / 2,
                           m_requestedSize.width(), scaledHeight);
        }

        QImage image;
        if (backgroundColor.alpha() == 255)
            image = QImage(m_requestedSize, QImage::Format_RGB32);
        else
            image = QImage(m_requestedSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(backgroundColor);

        QPainter painter;
        painter.begin(&image);
        renderer.render(&painter, bounds);
        painter.end();

        return image;
    }

    QString errorString() const
    {
        return m_error;
    }

private:
    QFile* m_file = nullptr;
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

SVGColorImageImageProvider::SVGColorImageImageProvider(qt_intf_t* p_intf)
    : QQuickAsyncImageProvider()
    , m_intf(p_intf)
{
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


SVGColorImageBuilder::SVGColorImageBuilder(QString path, QObject* parent)
    : QObject(parent)
{
    m_query.addQueryItem(PATH_KEY, path);
}

QString SVGColorImageBuilder::uri() const
{

    auto ret = QString{"image://svgcolor/?"} + m_query.toString(QUrl::FullyEncoded);
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

SVGColorImageBuilder* SVGColorImage::colorize(QString path)
{
    SVGColorImageBuilder* builder = new SVGColorImageBuilder(path);
    QQmlEngine::setObjectOwnership(builder, QQmlEngine::JavaScriptOwnership);
    return builder;
}
