/*****************************************************************************
 * input_slider.cpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2017 the VideoLAN team
 *
 * Authors: Pierre Lamot <pierre@videolabs.io>
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

#include "qt.hpp"
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>
#include "imagehelper.hpp"


QPixmap ImageHelper::loadSvgToPixmap( const QString &path, qint32 i_width, qint32 i_height )
{
#if HAS_QT56
    qreal ratio = QApplication::primaryScreen()->devicePixelRatio();

    QPixmap pixmap( QSize( i_width, i_height ) * ratio );
#else
    QPixmap pixmap( QSize( i_width, i_height ) );
#endif

    pixmap.fill( Qt::transparent );

    QSvgRenderer renderer( path );
    QPainter painter;

    painter.begin( &pixmap );
    renderer.render( &painter );
    painter.end();

#if HAS_QT56
    pixmap.setDevicePixelRatio( ratio );
#endif

    return pixmap;
}
