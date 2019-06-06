/*****************************************************************************
 * interface_widgets.hpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#ifndef VLC_QT_INTERFACE_WIDGETS_HPP_
#define VLC_QT_INTERFACE_WIDGETS_HPP_

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

class QMenu;
class QSlider;
class QTimer;
class QWidgetAction;
class SpeedControlWidget;
struct vout_window_t;

/******************** Video Widget ****************/
class VideoWidget : public QFrame
{
    Q_OBJECT
public:
    VideoWidget( intf_thread_t *, QWidget* p_parent );
    virtual ~VideoWidget();

    void request( struct vout_window_t * );
    void release( void );
    void sync( void );

protected:
    QPaintEngine *paintEngine() const Q_DECL_OVERRIDE
    {
        return NULL;
    }

    bool nativeEvent(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;
    virtual void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    QSize physicalSize() const;

private:
    int qtMouseButton2VLC( Qt::MouseButton );
    intf_thread_t *p_intf;
    vout_window_t *p_window;

    QWidget *stable;
    QLayout *layout;
    QTimer *cursorTimer;
    int cursorTimeout;

    void reportSize();
    void showCursor();

signals:
    void sizeChanged( int, int );

public slots:
    void setSize( unsigned int, unsigned int );

private slots:
    void hideCursor();
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
    void contextMenuEvent( QContextMenuEvent *event ) Q_DECL_OVERRIDE;
protected:
    void paintEvent( QPaintEvent *e ) Q_DECL_OVERRIDE;
    void showEvent( QShowEvent * e ) Q_DECL_OVERRIDE;
    void updateDefaultArt( const QString& );
    static const int MARGIN = 5;
    QString defaultArt;
public slots:
    void toggle(){ isVisible() ? hide() : show(); }
    void updateArt( const QString& );
    void titleUpdated( const QString& );
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
    void paintEvent( QPaintEvent *e ) Q_DECL_OVERRIDE;
    void showEvent( QShowEvent *e ) Q_DECL_OVERRIDE;
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;

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

class ClickableQLabel : public QLabel
{
    Q_OBJECT
public:
    void mouseDoubleClickEvent( QMouseEvent *event ) Q_DECL_OVERRIDE
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
    void mousePressEvent( QMouseEvent *event ) Q_DECL_OVERRIDE
    {
        if( displayType == TimeLabel::Elapsed ) return;
        toggleTimeDisplay();
        event->accept();
    }
    void mouseDoubleClickEvent( QMouseEvent *event ) Q_DECL_OVERRIDE
    {
        if( displayType != TimeLabel::Both ) return;
        event->accept();
        toggleTimeDisplay();
        ClickableQLabel::mouseDoubleClickEvent( event );
    }
private:
    intf_thread_t *p_intf;
    bool b_remainingTime;
    float cachedPos;
    vlc_tick_t cachedTime;
    int cachedLength;
    TimeLabel::Display displayType;

    char psz_length[MSTRTIME_MAX_SIZE];
    char psz_time[MSTRTIME_MAX_SIZE];
    void toggleTimeDisplay();
    void refresh();
private slots:
    void setRemainingTime( bool );
    void setDisplayPosition( float pos, vlc_tick_t time, int length );
    void setDisplayPosition( float pos );
signals:
    void broadcastRemainingTime( bool );
};

class SpeedLabel : public QLabel
{
    Q_OBJECT
public:
    SpeedLabel( intf_thread_t *, QWidget * );
    virtual ~SpeedLabel();

protected:
    void mousePressEvent ( QMouseEvent * event ) Q_DECL_OVERRIDE
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
    QWidgetAction *widgetAction;
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
    void mouseDoubleClickEvent( QMouseEvent *event ) Q_DECL_OVERRIDE
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
