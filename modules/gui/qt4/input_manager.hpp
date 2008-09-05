/*****************************************************************************
 * input_manager.hpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _INPUT_MANAGER_H_
#define _INPUT_MANAGER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>

#include "qt4.hpp"

#include <QObject>
#include <QEvent>

static int const PositionUpdate_Type     = QEvent::User + IMEventType + 1;
static int const ItemChanged_Type        = QEvent::User + IMEventType + 2;
static int const ItemStateChanged_Type   = QEvent::User + IMEventType + 3;
static int const ItemTitleChanged_Type   = QEvent::User + IMEventType + 4;
static int const ItemRateChanged_Type    = QEvent::User + IMEventType + 5;
static int const VolumeChanged_Type      = QEvent::User + IMEventType + 6;
static int const ItemSpuChanged_Type     = QEvent::User + IMEventType + 7;
static int const ItemTeletextChanged_Type= QEvent::User + IMEventType + 8;
static int const InterfaceVoutUpdate_Type= QEvent::User + IMEventType + 9;

static int const FullscreenControlToggle_Type = QEvent::User + IMEventType + 10;
static int const FullscreenControlShow_Type = QEvent::User + IMEventType + 11;
static int const FullscreenControlHide_Type = QEvent::User + IMEventType + 12;
static int const FullscreenControlPlanHide_Type = QEvent::User + IMEventType + 13;

class IMEvent : public QEvent
{
public:
    IMEvent( int type, int id ) : QEvent( (QEvent::Type)(type) )
    { i_id = id ; } ;
    virtual ~IMEvent() {};

    int i_id;
};

class InputManager : public QObject
{
    Q_OBJECT;
public:
    InputManager( QObject *, intf_thread_t * );
    virtual ~InputManager();

    void delInput();
    bool hasInput() { return p_input && !p_input->b_dead && vlc_object_alive (p_input); }
    bool hasAudio();
    bool hasVideo() { return hasInput() && b_video; }

    QString getName() { return old_name; }

private:
    intf_thread_t  *p_intf;
    input_thread_t *p_input;
    int             i_input_id;
    int             i_old_playing_status;
    QString         old_name;
    QString         artUrl;
    int             i_rate;
    bool            b_transparentTelextext;
    bool            b_video;

    void customEvent( QEvent * );
    void addCallbacks();
    void delCallbacks();
    void UpdateRate();
    void UpdateMeta();
    void UpdateStatus();
    void UpdateNavigation();
    void UpdatePosition();
    void UpdateSPU();
    void UpdateTeletext();
    void UpdateArt();
    void UpdateVout();

public slots:
    void setInput( input_thread_t * ); ///< Our controlled input changed
    void sliderUpdate( float ); ///< User dragged the slider. We get new pos
    void togglePlayPause();
    void slower();
    void faster();
    void normalRate();
    void setRate( int );
    void sectionNext();
    void sectionPrev();
    void sectionMenu();
    void telexGotoPage( int ); ///< Goto teletext page
    void telexToggle( bool );  ///< Enable disable teletext buttons
    void telexToggleButtons(); ///< Toggle buttons after click
    void telexSetTransparency(); ///< Set transparency on teletext background

signals:
    /// Send new position, new time and new length
    void positionUpdated( float , int, int );
    void rateChanged( int );
    void nameChanged( QString );
    /// Used to signal whether we should show navigation buttons
    void navigationChanged( int );
    /// Play/pause status
    void statusChanged( int );
    void artChanged( input_item_t* );
    /// Teletext
    void teletextEnabled( bool );
    void toggleTelexButtons();
    void toggleTelexTransparency();
    void setNewTelexPage( int );
    /// Advanced buttons
    void advControlsSetIcon();
    /// Vout
    void voutChanged( bool );
};

class MainInputManager : public QObject
{
    Q_OBJECT;
public:
    static MainInputManager *getInstance( intf_thread_t *_p_intf )
    {
        if( !instance )
            instance = new MainInputManager( _p_intf );
        return instance;
    }
    static void killInstance()
    {
        if( instance ) delete instance;
    }
    virtual ~MainInputManager();

    input_thread_t *getInput() { return p_input; };
    InputManager *getIM() { return im; };

private:
    MainInputManager( intf_thread_t * );
    void customEvent( QEvent * );

    InputManager            *im;
    input_thread_t          *p_input;

    intf_thread_t           *p_intf;
    static MainInputManager *instance;
public slots:
    bool teletextState();
    void togglePlayPause();
    void stop();
    void next();
    void prev();
signals:
    void inputChanged( input_thread_t * );
    void volumeChanged();
};

#endif
