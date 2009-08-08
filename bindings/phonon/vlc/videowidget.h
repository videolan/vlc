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

#ifndef PHONON_VLC_VIDEOWIDGET_H
#define PHONON_VLC_VIDEOWIDGET_H

#include "sinknode.h"

#include <phonon/videowidgetinterface.h>

#include "vlcvideowidget.h"
typedef Phonon::VLC::VLCVideoWidget Widget;


namespace Phonon
{
namespace VLC {


class VideoWidget : public SinkNode, public VideoWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(Phonon::VideoWidgetInterface)
public:

    VideoWidget(QWidget *p_parent);
    ~VideoWidget();

    void connectToMediaObject(PrivateMediaObject * mediaObject);

    Phonon::VideoWidget::AspectRatio aspectRatio() const;
    void setAspectRatio(Phonon::VideoWidget::AspectRatio aspect);

    Phonon::VideoWidget::ScaleMode scaleMode() const;
    void setScaleMode(Phonon::VideoWidget::ScaleMode scale);

    qreal brightness() const;
    void setBrightness(qreal brightness);

    qreal contrast() const;
    void setContrast(qreal contrast);

    qreal hue() const;
    void setHue(qreal hue);

    qreal saturation() const;
    void setSaturation(qreal saturation);

    Widget * widget();

private slots:

    void videoWidgetSizeChanged(int width, int height);

private:

    Widget *p_video_widget;

    Phonon::VideoWidget::AspectRatio aspect_ratio;

    Phonon::VideoWidget::ScaleMode scale_mode;

    bool  b_filter_adjust_activated;
    qreal f_brightness;
    qreal f_contrast;
    qreal f_hue;
    qreal f_saturation;
};

}
} // Namespace Phonon::VLC

#endif // PHONON_VLC_VIDEOWIDGET_H
