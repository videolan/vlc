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

#ifndef IMAGEHELPER_HPP
#define IMAGEHELPER_HPP

#include <QString>
#include <QPixmap>

class ImageHelper
{
public:
    /* render a Svg to a pixmap with current device pixel ratio */
    static QPixmap loadSvgToPixmap(const QString& path, qint32 i_width, qint32 i_height);
};

#endif // IMAGEHELPER_HPP
