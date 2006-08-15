/*****************************************************************************
 * main_interface.hpp : Main Interface
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _MAIN_INTERFACE_H_
#define _MAIN_INTERFACE_H_

#include <vlc/intf.h>
#include "ui/main_interface.h"
#include "util/qvlcframe.hpp"

class QCloseEvent;
class QLabel;
class InputManager;
class InputSlider;
class VideoWidget;

class MainInterface : public QVLCMW
{
    Q_OBJECT;
public:
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();

    void resizeEvent( QResizeEvent * );

    QSize videoSize, addSize;

protected:
    void closeEvent( QCloseEvent *);
private:
    VideoWidget *videoWidget;
    InputManager *main_input_manager;
    QLabel *timeLabel;
    QLabel *nameLabel;
    InputSlider *slider;
    /// Main input associated to the playlist
    input_thread_t *p_input;
    Ui::MainInterfaceUI ui;
private slots:
    void setStatus( int );
    void setName( QString );
    void setDisplay( float, int, int );
    void updateOnTimer();
    void play();
    void stop();
    void prev();
    void next();
};

#endif
