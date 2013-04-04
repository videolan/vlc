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

#include "main_interface.hpp" /* Interface integration */
#include "input_manager.hpp"  /* Speed control */

#include "components/controller.hpp"
#include "components/controller_widget.hpp"
#include "dialogs_provider.hpp"
#include "components/info_panels.hpp"

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QLinkedList>

class ResizeEvent;
class QPixmap;
class QHBoxLayout;
class QMenu;
class QSlider;

/******************** Video Widget ****************/
class VideoWidget : public QFrame
{
    Q_OBJECT
public:
    VideoWidget( intf_thread_t * );
    virtual ~VideoWidget();

    WId request( int *, int *, unsigned int *, unsigned int *, bool );
    void  release( void );
    void  sync( void );

protected:
    virtual QPaintEngine *paintEngine() const
    {
        return NULL;
    }

private:
    intf_thread_t *p_intf;

    QWidget *stable;
    QLayout *layout;
signals:
    void sizeChanged( int, int );

public slots:
    void SetSizing( unsigned int, unsigned int );
};

/******************** Background Widget ****************/
class BackgroundWidget : public QWidget
{
    Q_OBJECT
public:
    BackgroundWidget( intf_thread_t * );
    void setExpandstoHeight( bool b_expand ) { b_expandPixmap = b_expand; }
    void setWithArt( bool b_withart_ ) { b_withart = b_withart_; };
private:
    intf_thread_t *p_intf;
    QString pixmapUrl;
    bool b_expandPixmap;
    bool b_withart;
    QPropertyAnimation *fadeAnimation;
    virtual void contextMenuEvent( QContextMenuEvent *event ); 
protected:
    void paintEvent( QPaintEvent *e );
    virtual void showEvent( QShowEvent * e );
    static const int MARGIN = 5;
    QString defaultArt;
public slots:
    void toggle(){ TOGGLEV( this ); }
    void updateArt( const QString& );
};

class EasterEggBackgroundWidget : public BackgroundWidget
{
    Q_OBJECT

public:
    EasterEggBackgroundWidget( intf_thread_t * );
    virtual ~EasterEggBackgroundWidget();

public slots:
    void animate();

protected:
    void paintEvent( QPaintEvent *e );
    void showEvent( QShowEvent *e );
    void hideEvent( QHideEvent * );
    void resizeEvent( QResizeEvent * );

private slots:
    void spawnFlakes();
    void reset();

private:
    struct flake
    {
        QPoint point;
        bool b_fat;
    };
    QTimer *timer;
    QLinkedList<flake *> *flakes;
    int i_rate;
    int i_speed;
    bool b_enabled;
    static const int MAX_FLAKES = 1000;
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

class ClickableQLabel : public QLabel
{
    Q_OBJECT
public:
    virtual void mouseDoubleClickEvent( QMouseEvent *event )
    {
        Q_UNUSED( event );
        emit doubleClicked();
    }
signals:
    void doubleClicked();
};

class TimeLabel : public ClickableQLabel
{
    Q_OBJECT
public:
    enum Display
    {
        Elapsed,
        Remaining,
        Both
    };

    TimeLabel( intf_thread_t *_p_intf, TimeLabel::Display _displayType = TimeLabel::Both );
protected:
    virtual void mousePressEvent( QMouseEvent *event )
    {
        if( displayType == TimeLabel::Elapsed ) return;
        toggleTimeDisplay();
        event->accept();
    }
    virtual void mouseDoubleClickEvent( QMouseEvent *event )
    {
        if( displayType != TimeLabel::Both ) return;
        event->accept();
        toggleTimeDisplay();
        ClickableQLabel::mouseDoubleClickEvent( event );
    }
private:
    intf_thread_t *p_intf;
    bool b_remainingTime;
    int cachedLength;
    QTimer *bufTimer;

    bool buffering;
    bool showBuffering;
    float bufVal;
    TimeLabel::Display displayType;

    char psz_length[MSTRTIME_MAX_SIZE];
    char psz_time[MSTRTIME_MAX_SIZE];
    void toggleTimeDisplay();
    void paintEvent( QPaintEvent* );
private slots:
    void setDisplayPosition( float pos, int64_t time, int length );
    void setDisplayPosition( float pos );
    void updateBuffering( float );
    void updateBuffering();
};

class SpeedLabel : public QLabel
{
    Q_OBJECT
public:
    SpeedLabel( intf_thread_t *, QWidget * );
    virtual ~SpeedLabel();

protected:
    virtual void mousePressEvent ( QMouseEvent * event )
    {
        showSpeedMenu( event->pos() );
    }
private slots:
    void showSpeedMenu( QPoint );
    void setRate( float );
private:
    intf_thread_t *p_intf;
    QMenu *speedControlMenu;
    QString tooltipStringPattern;
    SpeedControlWidget *speedControl;
};

/******************** Speed Control Widgets ****************/
class SpeedControlWidget : public QFrame
{
    Q_OBJECT
public:
    SpeedControlWidget( intf_thread_t *, QWidget * );
    void updateControls( float );
private:
    intf_thread_t* p_intf;
    QSlider* speedSlider;
    QDoubleSpinBox* spinBox;
    int lastValue;

public slots:
    void activateOnState();

private slots:
    void updateRate( int );
    void updateSpinBoxRate( double );
    void resetRate();
};

class CoverArtLabel : public QLabel
{
    Q_OBJECT
public:
    CoverArtLabel( QWidget *parent, intf_thread_t * );
    void setItem( input_item_t * );
    virtual ~CoverArtLabel();

protected:
    virtual void mouseDoubleClickEvent( QMouseEvent *event )
    {
        if( ! p_item && qobject_cast<MetaPanel *>(this->window()) == NULL )
        {
            THEDP->mediaInfoDialog();
        }
        event->accept();
    }
private:
    intf_thread_t *p_intf;
    input_item_t *p_item;

public slots:
    void showArtUpdate( const QString& );
    void showArtUpdate( input_item_t * );
    void askForUpdate();
    void setArtFromFile();
    void clear();
};

#endif
