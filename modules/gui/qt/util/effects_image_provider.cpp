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

#include "qt.hpp" // VLC_WEAK

// Qt private exported function
QT_BEGIN_NAMESPACE
extern void VLC_WEAK qt_blurImage(QImage &blurImage, qreal radius, bool quality, int transposed = 0);
QT_END_NAMESPACE


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
        : m_blurRadius(settings["blurRadius"].toReal())
        , m_color(settings["color"].value<QColor>())
        , m_xOffset(settings["xOffset"].toReal())
        , m_yOffset(settings["yOffset"].toReal())
    { }

    QImage generate(const QSize& size) const override
    {
        QImage mask(size, QImage::Format_ARGB32_Premultiplied);
        mask.fill(m_color);
        return generate(mask);
    }

    QImage generate(const QImage& mask) const
    {
        if (Q_UNLIKELY(!&qt_blurImage))
        {
            qWarning("qt_blurImage() is not available! Drop shadow will not work!");
            return {};
        }

        // Create a new image with boundaries containing the mask and effect.
        QImage ret(boundingSize(mask.size()), QImage::Format_ARGB32_Premultiplied);
        ret.fill(0);

        assert(!ret.isNull());
        {
            // Copy the mask
            QPainter painter(&ret);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            const auto radius = m_blurRadius;
            painter.drawImage(radius + m_xOffset, radius + m_yOffset, mask);
        }

        // Blur the mask
        qt_blurImage(ret, m_blurRadius, false);

        return ret;
    }

    constexpr QSize boundingSize(const QSize& size) const
    {
        // Size of bounding rectangle of the effect
        const qreal diameter = m_blurRadius * 2;
        return size + QSize(qAbs(m_xOffset) + diameter, qAbs(m_yOffset) + diameter);
    }

protected:
    qreal m_blurRadius = 1.0;
    QColor m_color {63, 63, 63, 180};
    qreal m_xOffset = 0.0;
    qreal m_yOffset = 0.0;
};

class RoundedRectDropShadowEffect : public RectDropShadowEffect
{
public:
    explicit RoundedRectDropShadowEffect(const QVariantMap& settings)
        : RectDropShadowEffect(settings)
        , m_xRadius(settings["xRadius"].toReal())
        , m_yRadius(settings["yRadius"].toReal())
    { }

    QImage generate(const QSize& size) const override
    {
        assert(!(qFuzzyIsNull(m_xRadius) && qFuzzyIsNull(m_yRadius))); // use RectDropShadowEffect instead

        QImage mask(size, QImage::Format_ARGB32_Premultiplied);
        mask.fill(Qt::transparent);

        assert(!mask.isNull());
        {
            QPainter painter(&mask);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(m_color);

            QPainterPath path;
            path.addRoundedRect(mask.rect(), m_xRadius, m_yRadius);
            painter.fillPath(path, m_color);
            painter.drawPath(path);
        }

        return RectDropShadowEffect::generate(mask);
    }

protected:
    qreal m_xRadius = 0.0;
    qreal m_yRadius = 0.0;
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
        switch (static_cast<EffectsImageProvider::Effect>(effectMetaEnum.keyToValue(url.path().toLatin1())))
        {
        case EffectsImageProvider::RectDropShadow:
            effect = std::make_unique<RectDropShadowEffect>(queryToVariantMap(query));
            break;

        case EffectsImageProvider::RoundedRectDropShadow:
            effect = std::make_unique<RoundedRectDropShadowEffect>(queryToVariantMap(query));
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

QUrl EffectsImageProvider::url(Effect effect, const QVariantMap &properties)
{
    static const auto effectMetaEnum = QMetaEnum::fromType<EffectsImageProvider::Effect>();

    QUrl url;
    // image://
    url.setScheme(QStringLiteral("image"));
    // image://{id} -> image://effects
    url.setAuthority(QLatin1String(providerId), QUrl::ParsingMode::StrictMode);
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
