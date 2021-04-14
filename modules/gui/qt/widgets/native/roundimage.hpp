/*****************************************************************************
 * roundimage.hpp: Custom widgets
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

#ifndef VLC_QT_ROUNDIMAGE_HPP
#define VLC_QT_ROUNDIMAGE_HPP

#include "qt.hpp"

#include "util/asynctask.hpp"

#include <QPixmap>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QUrl>

class RoundImage : public QQuickPaintedItem
{
    Q_OBJECT

    // url of the image
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)

    // sets the maximum number of pixels stored for the loaded image so that large images do not use more memory than necessary
    Q_PROPERTY(QSizeF sourceSize READ sourceSize WRITE setSourceSize NOTIFY sourceSizeChanged)

    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)

public:
    RoundImage(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    void classBegin() override;
    void componentComplete() override;

    QUrl source() const;
    qreal radius() const;
    QSizeF sourceSize() const;

public slots:
    void setSource(QUrl source);
    void setRadius(qreal radius);
    void setSourceSize(QSizeF sourceSize);

signals:
    void sourceChanged(QUrl source);
    void radiusChanged(int radius);
    void sourceSizeChanged(QSizeF sourceSize);

protected:
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value) override;

private:
    class Image
    {
    public:
        static std::shared_ptr<Image> getImage(const QUrl &source, const QSizeF &sourceSize);
        virtual ~Image() = default;

        // must be reentrant
        virtual void paint(QPainter *painter, const QSizeF &size) = 0;
    };

    using ImagePtr = std::shared_ptr<Image>;

    class Loader : public AsyncTask<ImagePtr>
    {
    public:
        struct Params
        {
            QUrl source;
            QSizeF sourceSize;
        };

        Loader(const Params &params);

        ImagePtr execute();

    private:
        const Params params;
    };

    class RoundImageGenerator : public AsyncTask<QImage>
    {
    public:
        struct Params
        {
            qreal width;
            qreal height;
            qreal radius;
            ImagePtr image;
        };

        RoundImageGenerator(const Params &params);

        QImage execute();

    private:
        const Params params;
    };

    void setDPR(qreal value);
    void updateSource();
    void regenerateRoundImage();

    QUrl m_source;
    ImagePtr m_sourceImage;
    qreal m_radius = 0.0;
    QSizeF m_sourceSize;
    qreal m_dpr = 1.0; // device pixel ratio
    QImage m_roundImage;
    TaskHandle<Loader> m_loader {};
    TaskHandle<RoundImageGenerator> m_roundImageGenerator {};

    bool m_enqueuedGeneration = false;
    bool m_isComponentComplete = true;
};

#endif

