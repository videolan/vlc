/*****************************************************************************
 * VLC backend for the Phonon library                                        *
 * Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>               *
 * Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>                *
 * Copyright (C) 2009 Fathi Boudra <fabo@kde.org>                            *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 3 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with this package; if not, write to the Free Software       *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#include "vlcvideowidget.h"

#include <QtGui/QResizeEvent>
#include <QtCore/QDebug>

namespace Phonon
{
namespace VLC {

VLCVideoWidget::VLCVideoWidget(QWidget *p_parent)
        : WidgetNoPaintEvent(p_parent)
{
    // Set background color
    setBackgroundColor(Qt::black);
}

VLCVideoWidget::~VLCVideoWidget()
{
}

void VLCVideoWidget::resizeEvent(QResizeEvent *p_event)
{
    qDebug() << "resizeEvent" << p_event->size();
}

void VLCVideoWidget::setAspectRatio(double f_aspect_ratio)
{
}

void VLCVideoWidget::setScaleAndCropMode(bool b_scale_and_crop)
{
}

void VLCVideoWidget::setVideoSize(const QSize & size)
{
    videoSize = size;
}

QSize VLCVideoWidget::sizeHint() const
{
    return videoSize;
}

}
} // Namespace Phonon::VLC
