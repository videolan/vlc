/*****************************************************************************
 * interface_widgets.hpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
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

#ifndef _INTFWIDGETS_H_
#define _INTFWIDGETS_H_

#include <vlc/vlc.h>
#include <vlc_interface.h>

#include <vlc_aout.h>
#include "qt4.hpp"

#include <QWidget>
#include <QFrame>

class ResizeEvent;
class QPalette;
class QPixmap;
class QLabel;
class QHBoxLayout;

/******************** Video Widget ****************/
class VideoWidget : public QFrame
{
    Q_OBJECT
public:
    VideoWidget( intf_thread_t * );
    virtual ~VideoWidget();

    void *request( vout_thread_t *, int *, int *,
                   unsigned int *, unsigned int * );
    void release( void * );
    int control( void *, int, va_list );

    int i_video_height, i_video_width;
    vout_thread_t *p_vout;

    QSize widgetSize;
    virtual QSize sizeHint() const;
private:
    QWidget *frame;
    intf_thread_t *p_intf;
    vlc_mutex_t lock;
signals:
    void askResize();
private slots:
    void SetMinSize();
};

/******************** Background Widget ****************/
class BackgroundWidget : public QFrame
{
    Q_OBJECT
public:
    BackgroundWidget( intf_thread_t * );
    virtual ~BackgroundWidget();
    QSize widgetSize;
    virtual QSize sizeHint() const;
private:
    QPalette plt;
    QLabel *label;
    QHBoxLayout *backgroundLayout;
    virtual void resizeEvent( QResizeEvent *e );
    int DrawBackground();
    int CleanBackground();
    intf_thread_t *p_intf;
private slots:
    void setArt( QString );
};

class VisualSelector : public QFrame
{
    Q_OBJECT
public:
    VisualSelector( intf_thread_t *);
    virtual ~VisualSelector();
private:
    intf_thread_t *p_intf;
    QLabel *current;
private slots:
    void prev();
    void next();
};

class QPushButton;
class AdvControlsWidget : public QFrame
{
    Q_OBJECT
public:
    AdvControlsWidget( intf_thread_t *);
    virtual ~AdvControlsWidget();
    void enableInput( bool );
    void enableVideo( bool );
private:
    intf_thread_t *p_intf;
    QPushButton *normalButton, *recordButton, *ABButton;
    QPushButton *snapshotButton, *frameButton;
private slots:
    void normal();
    void snapshot();
    void frame();
    void fromAtoB();
    void record();
};

class InputSlider;
class QSlider;
class QGridLayout;
class VolumeClickHandler;
class ControlsWidget : public QFrame
{
    Q_OBJECT
public:
    ControlsWidget( intf_thread_t *, bool );
    virtual ~ControlsWidget();

    QPushButton *playlistButton;
    QSlider *volumeSlider;
    void setStatus( int );
    void enableInput( bool );
    void enableVideo( bool );
public slots:
    void setNavigation( int );
    void updateOnTimer();
protected:
    friend class MainInterface;
    friend class VolumeClickHandler;
private:
    intf_thread_t       *p_intf;
    QFrame              *discFrame;
    QGridLayout         *controlLayout;
    InputSlider         *slider;
    QPushButton         *prevSectionButton, *nextSectionButton, *menuButton;
    QPushButton         *playButton, *fullscreenButton;
    QPushButton         *slowerButton, *fasterButton;
    AdvControlsWidget   *advControls;
    QLabel              *volMuteLabel;

    bool                 b_advancedVisible;
private slots:
    void play();
    void stop();
    void prev();
    void next();
    void updateVolume( int );
    void fullscreen();
    void extSettings();
    void prefs();
    void faster();
    void slower();
    void toggleAdvanced();
signals:
    void advancedControlsToggled( bool );
};

class VolumeClickHandler : public QObject
{
public:
    VolumeClickHandler( intf_thread_t *_p_intf, ControlsWidget *_m ) :QObject(_m)
    {m = _m; p_intf = _p_intf; }
    virtual ~VolumeClickHandler() {};
    bool eventFilter( QObject *obj, QEvent *e )
    {
        if (e->type() == QEvent::MouseButtonPress )
        {
            aout_VolumeMute( p_intf, NULL );
            return true;
        }
        return false;
    }
private:
    ControlsWidget *m;
    intf_thread_t *p_intf;
};


/******************** Playlist Widgets ****************/
#include <QModelIndex>
#include <QSplitter>
class QSignalMapper;
class PLSelector;
class PLPanel;
class QPushButton;

class PlaylistWidget : public QSplitter
{
    Q_OBJECT;
public:
    PlaylistWidget( intf_thread_t *_p_i ) ;
    virtual ~PlaylistWidget();
    QSize widgetSize;
    virtual QSize sizeHint() const;
private:
    PLSelector *selector;
    PLPanel *rightPanel;
    QPushButton *addButton;
    QLabel *art;
    QString prevArt;
protected:
     intf_thread_t *p_intf;
private slots:
    void setArt( QString );
signals:
    void rootChanged( int );
    void artSet( QString );
};

#endif
