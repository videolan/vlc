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

#include "maininterface/main_interface.hpp" /* Interface integration */
#include "player/player_controller.hpp"  /* Speed control */

#include "dialogs/dialogs_provider.hpp"
#include "dialogs/mediainfo/info_panels.hpp"

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>

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
