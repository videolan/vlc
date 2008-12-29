/*****************************************************************************
 * interface_widgets.hpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "main_interface.hpp"
#include "input_manager.hpp"

#include "components/controller.hpp"
#include "components/controller_widget.hpp"

//#include <vlc_aout.h> Visualizer

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>

class ResizeEvent;
class QPalette;
class QPixmap;
class QHBoxLayout;

/******************** Video Widget ****************/
class VideoWidget : public QFrame
{
    Q_OBJECT
friend class MainInterface;

public:
    VideoWidget( intf_thread_t * );
    virtual ~VideoWidget();

    void *request( vout_thread_t *, int *, int *,
                   unsigned int *, unsigned int * );
    void  release( void );
    int   control( void *, int, va_list );

    virtual QSize sizeHint() const;

protected:
    virtual QPaintEngine *paintEngine() const
    {
        return NULL;
    }

    virtual void paintEvent(QPaintEvent *);

private:
    intf_thread_t *p_intf;
    vout_thread_t *p_vout;

    QSize videoSize;

signals:
    void askVideoWidgetToShow( unsigned int, unsigned int );

public slots:
    void SetSizing( unsigned int, unsigned int );

};

/******************** Background Widget ****************/
class BackgroundWidget : public QWidget
{
    Q_OBJECT
public:
    BackgroundWidget( intf_thread_t * );
    virtual ~BackgroundWidget();

private:
    QPalette plt;
    QLabel *label;
    virtual void contextMenuEvent( QContextMenuEvent *event );
    intf_thread_t *p_intf;
    virtual void resizeEvent( QResizeEvent * event );

public slots:
    void toggle(){ TOGGLEV( this ); }
    void updateArt( input_item_t* );
};

#if 0
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
#endif

class TimeLabel : public QLabel
{
    Q_OBJECT
public:
    TimeLabel( intf_thread_t *_p_intf );
protected:
    virtual void mousePressEvent( QMouseEvent *event )
    {
        toggleTimeDisplay();
    }
    virtual void mouseDoubleClickEvent( QMouseEvent *event )
    {
        toggleTimeDisplay();
        emit timeLabelDoubleClicked();
    }
private:
    intf_thread_t *p_intf;
    bool b_remainingTime;
    void toggleTimeDisplay();
signals:
    void timeLabelDoubleClicked();
private slots:
    void setDisplayPosition( float pos, int time, int length );
    void setStatus( int i_status );
};

class SpeedLabel : public QLabel
{
    Q_OBJECT
public:
    SpeedLabel( intf_thread_t *_p_intf, const QString text ): QLabel( text )
    { p_intf = _p_intf; }

protected:
    virtual void mouseDoubleClickEvent ( QMouseEvent * event )
    {
        THEMIM->getIM()->setRate( INPUT_RATE_DEFAULT );
    }
private:
    intf_thread_t *p_intf;
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

public slots:
    void setEnable( bool );

private slots:
    void updateRate( int );
    void resetRate();
};

class CoverArtLabel : public QLabel
{
    Q_OBJECT
public:
    CoverArtLabel( QWidget *parent,
                   vlc_object_t *p_this,
                   input_item_t *p_input = NULL );
    virtual ~CoverArtLabel()
    { if( p_input ) vlc_gc_decref( p_input ); }
private:
    input_item_t *p_input;
    vlc_object_t *p_this;

    QString prevArt;

public slots:
    void requestUpdate() { emit updateRequested(); };
    void update( input_item_t* p_item )
    {
        if( p_input ) vlc_gc_decref( p_input );
        if( ( p_input = p_item ) )
            vlc_gc_incref( p_input );
        requestUpdate();
    }

private slots:
    void doUpdate();
    void downloadCover();

signals:
    void updateRequested();
};

#endif
