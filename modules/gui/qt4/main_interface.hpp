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
#include <vlc/aout.h>
#include "ui/main_interface.h"
#include "util/qvlcframe.hpp"

#include <QSize>

class QSettings;
class QCloseEvent;
class QKeyEvent;
class QLabel;

class InputManager;
class InputSlider;
class VideoWidget;
class BackgroundWidget;
class PlaylistWidget;
class VolumeClickHandler;
class VisualSelector;

class MainInterface : public QVLCMW
{
    Q_OBJECT;
public:
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();


    void *requestVideo( vout_thread_t *p_nvout, int *pi_x,
                        int *pi_y, unsigned int *pi_width,
                        unsigned int *pi_height );
    void releaseVideo( void *);
    int controlVideo( void *p_window, int i_query, va_list args );
protected:
    void resizeEvent( QResizeEvent * );
    void closeEvent( QCloseEvent *);
    Ui::MainInterfaceUI ui;
    friend class VolumeClickHandler;
private:
    QSettings *settings;
    QSize mainSize, addSize;

    bool need_components_update;
    void calculateInterfaceSize();
    void handleMainUi( QSettings* );
    void doComponentsUpdate();

    /* Video */
    VideoWidget         *videoWidget;
    virtual void keyPressEvent( QKeyEvent *);

    BackgroundWidget    *bgWidget;
    VisualSelector      *visualSelector;
    PlaylistWidget      *playlistWidget;

    bool                 playlistEmbeddedFlag;
    bool                 videoEmbeddedFlag;

    InputManager        *main_input_manager;
    InputSlider         *slider;
    input_thread_t      *p_input;    ///< Main input associated to the playlist

    QLabel              *timeLabel;
    QLabel              *nameLabel;
private slots:
    void setStatus( int );
    void setName( QString );
    void setDisplay( float, int, int );
    void updateOnTimer();
    void play();
    void stop();
    void prev();
    void next();
    void playlist();
    void updateVolume( int sliderVolume );
};


class VolumeClickHandler : public QObject
{
public:
    VolumeClickHandler( MainInterface *_m ) :QObject(_m) {m = _m; }
    virtual ~VolumeClickHandler() {};
    bool eventFilter( QObject *obj, QEvent *e )
    {
        if (e->type() == QEvent::MouseButtonPress )
        {
            if( obj->objectName() == "volLowLabel" )
            {
                m->ui.volumeSlider->setValue( 0 );
            }
            else
                m->ui.volumeSlider->setValue( 100 );
            return true;
        }
        return false;
    }
private:
    MainInterface *m;
};

#endif
