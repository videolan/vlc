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

#ifndef EFFECTS_IMAGE_PROVIDER_HPP
#define EFFECTS_IMAGE_PROVIDER_HPP

#include <QObject>
#include <QUrl>
#include <QSize>
#include <QQuickImageProvider>

class EffectsImageProvider : public QQuickImageProvider
{
    Q_OBJECT

    static constexpr const char * providerId = "effects";

public:
    enum Effect
    {
        RectDropShadow = 1,
        RoundedRectDropShadow,
        DoubleRoundedRectDropShadow
    };
    Q_ENUM(Effect)

    explicit EffectsImageProvider(QQmlEngine *engine)
        : QQuickImageProvider(QQuickImageProvider::ImageType::Image,
                              QQmlImageProviderBase::ForceAsynchronousImageLoading)
    {
        assert(engine);

        // Engine will take the ownership; no need to set parent in constructor
        engine->addImageProvider(QLatin1String(providerId), this);
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    Q_INVOKABLE static QUrl url(Effect effect, const QVariantMap& properties);
};

#endif // EFFECTS_IMAGE_PROVIDER_HPP
