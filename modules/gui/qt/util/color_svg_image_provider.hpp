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

#ifndef COLOR_SVG_IMAGE_PROVIDER_HPP
#define COLOR_SVG_IMAGE_PROVIDER_HPP

#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QString>
#include <QUrlQuery>

struct qt_intf_t;

class SVGColorImageImageProvider: public QQuickAsyncImageProvider
{
public:
    SVGColorImageImageProvider(qt_intf_t *p_intf);

    QQuickImageResponse* requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    qt_intf_t *m_intf = nullptr;
};


class SVGColorImageBuilder : public QObject
{
    Q_OBJECT
public:
    SVGColorImageBuilder(QString path, QObject* parent = nullptr);

    /**
     * @brief uri will generate an uri usable with images and roud images
     */
    Q_INVOKABLE QString uri() const;

    /**
     * @brief color1 will replace #FF00FF (mangenta) with the color @a c1
     * @param c1 the new color, only RBG (no A) colors are supported
     */
    Q_INVOKABLE SVGColorImageBuilder* color1(QColor c1);

    /**
     * @brief color1 will replace #00FFFF (cyan) with the color @a c2
     * @param c2 the new color, only RBG (no A) colors are supported
     */
    Q_INVOKABLE SVGColorImageBuilder* color2(QColor c2);

    /**
     * @brief color1 will replace #FF8800 (orange) with the color @a c2
     * @param c2 the new color, only RBG (no A) colors are supported
     */
    Q_INVOKABLE SVGColorImageBuilder* accent(QColor c2);

    /**
     * @brief will fill the image background with given color (transparent by default)
     * @param bg the new color, RGBA colors are supported
     */
    Q_INVOKABLE SVGColorImageBuilder* background(QColor bg);

    /**
     * @brief any will replace any string provided in the map
     * @param map a map of string to replace, key is the string to be replaced, value (string or color) is the replaced value
     */
    Q_INVOKABLE SVGColorImageBuilder* any(QVariantMap map);

private:
    QUrlQuery m_query;
};

class SVGColorImage : public QObject {
    Q_OBJECT
public:
    SVGColorImage(QObject* parent = nullptr);

    /**
     * @brief colorize create a path builder for the path @path
     * @param path to the artwork
     *
     * sample usage:
     *
     * @code{qml}
     *   Image {
     *     src: SVGColorImage.colorize("qrc:///path/to/assert.svg")
     *                       .color1("blue")
     *                       .color2(Qt.color(0.2, 0.7, 0.0))
     *                       .uri()
     *   }
     * @code
     *
     * @note
     * At the moment only simple string substitution is performed, the method is really stupid and
     * doesn't take SVG specificities into account. As a result, this is not possible to set transparent
     * colors as SVG only recognize RGB definitions, transparency being handled in a separate field
     *
     * Handling transparent color properly would probably require a proper XML parser and defining colors
     * as defs entries using solidColor node (which Qt supports)
     */
    Q_INVOKABLE SVGColorImageBuilder* colorize(QString path);
};

#endif /* COLOR_SVG_IMAGE_PROVIDER_HPP */
