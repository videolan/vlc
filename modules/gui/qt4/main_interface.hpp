/*****************************************************************************
 * main_interface.hpp : Main Interface
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _MAIN_INTERFACE_H_
#define _MAIN_INTERFACE_H_

#include <vlc/intf.h>
#include "ui/main_interface.h"
#include "util/qvlcframe.hpp"

class InputManager;
class QCloseEvent;
class InputSlider;
class VideoWidget;

class MainInterface : public QVLCMW
{
    Q_OBJECT;
public:
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();

    virtual QSize sizeHint() const;

    int i_saved_width, i_saved_height;

protected:
    void closeEvent( QCloseEvent *);
private:
    VideoWidget *videoWidget;
    InputManager *main_input_manager;
    InputSlider *slider;
    /// Main input associated to the playlist
    input_thread_t *p_input;
    Ui::MainInterfaceUI ui;
private slots:
    void setDisplay( float, int, int );
    void updateOnTimer();
    void play();
    void stop();
    void prev();
    void next();
};

#endif
