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
#define VOLUME_MAX 200

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

private:
    intf_thread_t *p_intf;
    vlc_mutex_t lock;
signals:
    void askResize();
    void askVideoWidgetToShow();
public slots:
    void SetSizing( unsigned int, unsigned int );
};

/******************** Background Widget ****************/
class BackgroundWidget : public QFrame
{
    Q_OBJECT
public:
    BackgroundWidget( intf_thread_t * );
    virtual ~BackgroundWidget();
    QSize widgetSize;
    QSize sizeHint() const;
    bool b_need_update;
private:
    QPalette plt;
    QLabel *label;
    virtual void resizeEvent( QResizeEvent *e );
    virtual void contextMenuEvent( QContextMenuEvent *event );
    intf_thread_t *p_intf;
    int i_runs;
public slots:
    void toggle(){ TOGGLEV( this ); }
    void update( input_thread_t * );
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

/* Advanced Button Bar */
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
    QPushButton *recordButton, *ABButton;
    QPushButton *snapshotButton, *frameButton;

    mtime_t timeA, timeB;

private slots:
    void snapshot();
    void frame();
    void fromAtoB();
    void record();
    void AtoBLoop( float, int, int );
};

/* Button Bar */
class InputSlider;
class QSlider;
class QGridLayout;
class VolumeClickHandler;
class SoundSlider;
class QAbstractSlider;
class QToolButton;

class ControlsWidget : public QFrame
{
    Q_OBJECT
public:
    /* p_intf, advanced control visible or not, blingbling or not */
    ControlsWidget( intf_thread_t *, MainInterface*, bool, bool );
    virtual ~ControlsWidget();

    QPushButton *playlistButton;
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
    QWidget             *discFrame;
    QWidget             *telexFrame;
    QGridLayout         *controlLayout;
    InputSlider         *slider;
    QPushButton         *prevSectionButton, *nextSectionButton, *menuButton;
    QPushButton         *playButton, *fullscreenButton;
    QToolButton         *slowerButton, *fasterButton;
    AdvControlsWidget   *advControls;
    QLabel              *volMuteLabel;
    QAbstractSlider     *volumeSlider;

    bool                 b_advancedVisible;
private slots:
    void play();
    void stop();
    void prev();
    void next();
    void updateVolume( int );
    void fullscreen();
    void extSettings();
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
        if (e->type() == QEvent::MouseButtonPress  )
        {
            aout_VolumeMute( p_intf, NULL );
            audio_volume_t i_volume;
            aout_VolumeGet( p_intf, &i_volume );
            m->updateVolume( i_volume *  VOLUME_MAX / (AOUT_VOLUME_MAX/2) );
            return true;
        }
        return false;
    }
private:
    ControlsWidget *m;
    intf_thread_t *p_intf;
};

#include <QLabel>
#include <QMouseEvent>
class TimeLabel : public QLabel
{
    Q_OBJECT
    void mousePressEvent( QMouseEvent *event )
    {
        emit timeLabelClicked();
    }
    void mouseDoubleClickEvent( QMouseEvent *event )
    {
        emit timeLabelDoubleClicked();
    }
signals:
    void timeLabelClicked();
    void timeLabelDoubleClicked();
};


/******************** Speed Control Widgets ****************/
class SpeedControlWidget : public QFrame
{
    Q_OBJECT
public:
    SpeedControlWidget( intf_thread_t *);
    virtual ~SpeedControlWidget();
    void updateControls( int );
private:
    intf_thread_t *p_intf;
    QSlider *speedSlider;
private slots:
    void updateRate( int );
    void resetRate();
};

#endif
