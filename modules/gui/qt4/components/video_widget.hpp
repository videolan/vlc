/*****************************************************************************
 * video_widget.hpp : Embedded video
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <QWidget>

class VideoWidget : public QWidget
{
public:
    VideoWidget( intf_thread_t *);
    virtual ~VideoWidget();

    virtual QSize sizeHint() const;

    void *Request( vout_thread_t *, int *, int *,
                   unsigned int *, unsigned int * );
    void Release( void * );
    int Control( void *, int, va_list );
    int i_video_height, i_video_width;
private:
    QWidget *frame;
    intf_thread_t *p_intf;
    vout_thread_t *p_vout;
    vlc_mutex_t lock;

};

#endif
