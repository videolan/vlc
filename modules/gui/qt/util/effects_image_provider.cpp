/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "effects_image_provider.hpp"

#include <QPainter>
#include <QUrl>
#include <QUrlQuery>
#include <QPainterPath>

#include <memory>

#include "fast_gaussian_blur_template.h"

namespace {

class IEffect
{
public:
    virtual QImage generate(const QSize& size) const = 0;
    virtual ~IEffect() = default;
};

class RectDropShadowEffect : public IEffect
{

public:
    explicit RectDropShadowEffect(const QVariantMap& settings)
        : m_viewport(settings["viewportWidth"].toReal(), settings["viewportHeight"].toReal())
        , m_color(settings["color"].value<QColor>())
        , m_blurRadius(settings["blurRadius"].toReal())
        , m_xOffset(settings["xOffset"].toReal())
        , m_yOffset(settings["yOffset"].toReal())
        , m_recWidth(settings["rectWidth"].toReal())
        , m_height(settings["rectHeight"].toReal())
    {
    }

    virtual void draw(QPainter& painter, qreal xscale, qreal yscale) const
    {
        QPainterPath path;
        //center in window + offset
        path.addRect(
            ((m_viewport.width() / 2.)  - (m_recWidth / 2.) + m_xOffset) * xscale,
            ((m_viewport.height() / 2.) - (m_height / 2.)   + m_yOffset) * yscale,
            m_recWidth * xscale,
            m_height * yscale);
        painter.fillPath(path, m_color);
        painter.drawPath(path);
    }

    QImage generate(const QSize& size) const override
    {
        //scale image to fit in the requested size
        QSize viewport = m_viewport.scaled(size, Qt::KeepAspectRatio);
        qreal xscale = viewport.width() / (qreal)m_viewport.width();
        qreal yscale = viewport.height() / (qreal)m_viewport.height();

        //we need to generate packed image for the blur implementation
        //default QImage constructor may not for enforce this
        unsigned char* rawSource = (unsigned char*)malloc(viewport.width() * viewport.height() * 4);
        if (!rawSource)
            return {};

        //don't make the QImage hold the rawbuffer, as fast_gaussian_blur may swap input and output buffers
        QImage source(rawSource,
            viewport.width(), viewport.height(), viewport.width() * 4,
            QImage::Format_ARGB32_Premultiplied);

        // Create a new image with boundaries containing the mask and effect.
        source.fill(Qt::transparent);
        {
            // Copy the mask
            QPainter painter(&source);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            //note: can we use painter.scale here?
            draw(painter, xscale, yscale);
        }

        unsigned char* rawDest = (unsigned char*)malloc(viewport.width() * viewport.height() * 4);
        if (!rawDest)
        {
            free(rawSource);
            return {};
        }

        fast_gaussian_blur(
            rawSource, rawDest,
            viewport.width(), viewport.height(), 4, // 4 channels
            m_blurRadius * xscale / 2, // sigma is radius/2, see https://drafts.csswg.org/css-backgrounds/#shadow-blur
            3, Border::kMirror //3 passes
            );

        free(rawSource);
        QImage dest(rawDest,
            viewport.width(), viewport.height(), viewport.width() * 4,
            QImage::Format_ARGB32_Premultiplied,
            free, rawDest);

        return dest;
    }


protected:
    QSize m_viewport;

    QColor m_color {63, 63, 63, 180};
    qreal m_blurRadius = 1.0;
    qreal m_xOffset = 0.0;
    qreal m_yOffset = 0.0;
    qreal m_recWidth = 0.0;
    qreal m_height = 0.0;
};

class RoundedRectDropShadowEffect : public RectDropShadowEffect
{
public:
    explicit RoundedRectDropShadowEffect(const QVariantMap& settings)
        : RectDropShadowEffect(settings)

        , m_xRadius(settings["xRadius"].toReal())
        , m_yRadius(settings["yRadius"].toReal())
    { }

    void draw(QPainter& painter, qreal xscale, qreal yscale) const override
    {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(m_color);

        QPainterPath path;
        path.addRoundedRect(
            ((m_viewport.width() / 2.)  - (m_recWidth / 2.) + m_xOffset) * xscale,
            ((m_viewport.height() / 2.) - (m_height / 2.)   + m_yOffset) * yscale,
            m_recWidth * xscale,
            m_height * yscale,
            m_xRadius * xscale,
            m_yRadius * yscale);
        painter.fillPath(path, m_color);
        painter.drawPath(path);
    }

protected:
    qreal m_xRadius = 0.0;
    qreal m_yRadius = 0.0;
};

class DoubleShadowEffect : public IEffect
{
public:
    explicit DoubleShadowEffect(const QVariantMap& settings)
        : shadow1(adaptSettings(settings, "primary"))
        , shadow2(adaptSettings(settings, "secondary"))
    {
    }

    static QVariantMap adaptSettings(const QVariantMap& settings, const QString& prefix)
    {
        QVariantMap ret;
        ret["viewportWidth"]  = settings["viewportWidth"].toReal();
        ret["viewportHeight"] = settings["viewportHeight"].toReal();
        ret["blurRadius"]     = settings[prefix + "BlurRadius"].toReal();
        ret["color"]          = settings[prefix + "Color"].value<QColor>();
        ret["xOffset"]        = settings[prefix + "XOffset"].toReal();
        ret["yOffset"]        = settings[prefix + "YOffset"].toReal();
        ret["rectWidth"]      = settings["rectWidth"].toReal();
        ret["rectHeight"]     = settings["rectHeight"].toReal();
        ret["xRadius"]        = settings["xRadius"].toReal();
        ret["yRadius"]        = settings["yRadius"].toReal();
        return ret;
    }

    QImage generate(const QSize& size) const override
    {
        QImage firstImage = shadow1.generate(size);
        QImage secondImage = shadow2.generate(size);
        QPainter painter(&firstImage);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(firstImage.rect(), secondImage);
        return firstImage;
    }

protected:
    RoundedRectDropShadowEffect shadow1;
    RoundedRectDropShadowEffect shadow2;
};

}

QImage EffectsImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    assert(size);

    const auto generate = [&]() -> QImage {
        // Effect can not be generated if size is not provided.
        // Qt Quick Image may complain about returning null image,
        // but there is not much to do here about it.
        if (requestedSize.isEmpty())
            return {};

        static const auto effectMetaEnum = QMetaEnum::fromType<EffectsImageProvider::Effect>();

        static const auto queryToVariantMap = [](const QUrlQuery& query) {
            QVariantMap map;
            for (auto&& i : query.queryItems())
            {
                map.insert(i.first, QUrl::fromPercentEncoding(i.second.toLatin1()));
            }
            return map;
        };

        QUrl url(id, QUrl::ParsingMode::StrictMode);

        const QUrlQuery query(url);

        std::unique_ptr<IEffect> effect;
        switch (static_cast<EffectsImageProvider::Effect>(effectMetaEnum.keyToValue(url.path().toLatin1().constData())))
        {
        case EffectsImageProvider::RectDropShadow:
            effect = std::make_unique<RectDropShadowEffect>(queryToVariantMap(query));
            break;

        case EffectsImageProvider::RoundedRectDropShadow:
            effect = std::make_unique<RoundedRectDropShadowEffect>(queryToVariantMap(query));
            break;


        case EffectsImageProvider::DoubleRoundedRectDropShadow:
            effect = std::make_unique<DoubleShadowEffect>(queryToVariantMap(query));
            break;

        default:
            return {};
        }

        return effect->generate(requestedSize);
    };

    const auto& effect = generate();
    *size = effect.size();

    return effect;
}

QObject *EffectsImageProvider::instance(QQmlEngine *engine, QJSEngine *)
{
    assert(engine);
    QQmlImageProviderBase* provider = engine->imageProvider(providerId);
    if (!provider)
        return nullptr;
    assert(dynamic_cast<EffectsImageProvider*>(provider));
    engine->setObjectOwnership(provider, QQmlEngine::ObjectOwnership::CppOwnership);
    return provider;
}

QUrl EffectsImageProvider::url(Effect effect, const QVariantMap &properties)
{
    static const auto effectMetaEnum = QMetaEnum::fromType<EffectsImageProvider::Effect>();

    QUrl url;
    // image://
    url.setScheme(QStringLiteral("image"));
    // image://{id} -> image://effects
    url.setAuthority(providerId, QUrl::ParsingMode::StrictMode);
    // image://{id}/{effectType} -> image://effects/DropShadow
    url.setPath(QString("/%1").arg(effectMetaEnum.valueToKey(effect)), QUrl::ParsingMode::StrictMode);

    QUrlQuery query;

    QMapIterator<QString, QVariant> i(properties);
    while (i.hasNext())
    {
        i.next();

        const QVariant& value = i.value();

        if (!value.isValid() || value.isNull()) // if not valid, defaults are used
            continue;

        assert(value.canConvert<QString>());
        assert(!i.key().startsWith('_'));

        // image://{id}/{effectType}?{propertyName}={propertyValue}
        query.addQueryItem(i.key(), QUrl::toPercentEncoding(value.toString()));
    }
    url.setQuery(query);

    assert(url.isValid());
    return url;
}
