/*****************************************************************************
 * qvlcframe.hpp : A few helpers
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifndef VLC_QT_QVLCFRAME_HPP_
#define VLC_QT_QVLCFRAME_HPP_

#include <QWidget>
#include <QDialog>
#include <QMainWindow>
#include <QSettings>

#include "qt.hpp"

class QKeyEvent;

class QVLCTools
{
   public:
       /*
        use this function to save a widgets screen position
        only for windows / dialogs which are floating, if a
        window is docked into another - don't all this function
        or it may write garbage to position info!
       */
       static void saveWidgetPosition( QSettings *settings, QWidget *widget);

       static void saveWidgetPosition( qt_intf_t *p_intf,
                                       const QString& configName,
                                       QWidget *widget);

       /*
         use this method only for restoring window state of non docked
         windows!
       */
       static bool restoreWidgetPosition(QSettings *settings,
                                           QWidget *widget,
                                           QSize defSize = QSize( 0, 0 ),
                                           QPoint defPos = QPoint( 0, 0 ));

       static bool restoreWidgetPosition( qt_intf_t *p_intf,
                                           const QString& configName,
                                           QWidget *widget,
                                           QSize defSize = QSize( 0, 0 ),
                                           QPoint defPos = QPoint( 0, 0 ) );


       static void saveWindowPosition(QSettings *settings, QWindow *window);
       static void restoreWindowPosition(QSettings *settings, QWindow *window,
                                          QSize defSize = QSize( 0, 0 ),
                                          QPoint defPos = QPoint( 0, 0 ) );
};

class QVLCFrame : public QWidget
{
public:
    QVLCFrame( qt_intf_t *_p_intf ) : QWidget( NULL ), p_intf( _p_intf )
    {};
    virtual ~QVLCFrame()   {};

    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    qt_intf_t *p_intf;

    void restoreWidgetPosition( const QString& name,
                       QSize defSize = QSize( 1, 1 ),
                       QPoint defPos = QPoint( 0, 0 ) )
    {
        QVLCTools::restoreWidgetPosition(p_intf, name, this, defSize, defPos);
    }

    void saveWidgetPosition( const QString& name )
    {
        QVLCTools::saveWidgetPosition( p_intf, name, this);
    }

    virtual void cancel()
    {
        hide();
    }
    virtual void close()
    {
        hide();
    }
    void keyPressEvent( QKeyEvent *keyEvent ) override;
};

class QVLCDialog : public QDialog
{
public:
    QVLCDialog(QWindow *parent, qt_intf_t *_p_intf );
    virtual ~QVLCDialog() {};
    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }

    static void setWindowTransientParent(QWidget* widget, QWindow* parent, qt_intf_t* p_intf);

protected:
    qt_intf_t *p_intf;

    virtual void cancel()
    {
        hide();
    }
    virtual void close()
    {
        hide();
    }
    void keyPressEvent( QKeyEvent *keyEvent ) override;
};

class QVLCMW : public QMainWindow
{
public:
    QVLCMW( qt_intf_t *_p_intf,QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags() )
        : QMainWindow( parent, flags )
        , p_intf( _p_intf )
    {}

    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    qt_intf_t *p_intf;
    QSize mainSize;

    void readSettings( const QString& name, QSize defSize )
    {
        QVLCTools::restoreWidgetPosition( p_intf, name, this, defSize);
    }

    void readSettings( const QString& name )
    {
        QVLCTools::restoreWidgetPosition( p_intf, name, this);
    }
    void readSettings( QSettings *settings )
    {
        QVLCTools::restoreWidgetPosition(settings, this);
    }

    void readSettings( QSettings *settings, QSize defSize)
    {
        QVLCTools::restoreWidgetPosition(settings, this, defSize);
    }

    void writeSettings( const QString& name )
    {
        QVLCTools::saveWidgetPosition( p_intf, name, this);
    }
    void writeSettings(QSettings *settings )
    {
        QVLCTools::saveWidgetPosition(settings, this);
    }
};

#endif
