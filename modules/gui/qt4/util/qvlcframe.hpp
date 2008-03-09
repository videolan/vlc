/*****************************************************************************
 * qvlcframe.hpp : A few helpers
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifndef _QVLCFRAME_H_
#define _QVLCFRAME_H_

#include <QWidget>
#include <QDialog>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QApplication>
#include <QSettings>
#include <QMainWindow>
#include <QPushButton>
#include <QKeyEvent>
#include <QDesktopWidget>

#include "qt4.hpp"
#include <vlc/vlc.h>
#include <vlc_charset.h>

class QVLCTools
{
   public:
       /*
        use this function to save a widgets screen position
        only for windows / dialogs which are floating, if a
        window is docked into an other - don't all this function
        or it may write garbage to position info!
       */
       static void saveWidgetPosition(QSettings *settings, QWidget *widget)
       {
         settings->setValue("geometry", widget->saveGeometry());
       }
       static void saveWidgetPosition(QString configName, QWidget *widget)
       {
         QSettings *settings = new QSettings("vlc", "vlc-qt-interface");
         settings->beginGroup( configName );
         QVLCTools::saveWidgetPosition(settings, widget);
         settings->endGroup();
         delete settings;
       }


       /*
         use this method only for restoring window state of non docked
         windows!
       */
       static vlc_bool_t restoreWidgetPosition(QSettings *settings,
                                           QWidget *widget,
                                           QSize defSize = QSize( 0, 0 ),
                                           QPoint defPos = QPoint( 0, 0 ))
       {
          if(!widget->restoreGeometry(settings->value("geometry")
                                      .toByteArray()))
          {
            widget->move(defPos);
            widget->resize(defSize);

            if(defPos.x() == 0 && defPos.y()==0)
               centerWidgetOnScreen(widget);
            return VLC_TRUE;
          }
          return VLC_FALSE;
       }

       static vlc_bool_t restoreWidgetPosition(QString configName,
                                           QWidget *widget,
                                           QSize defSize = QSize( 0, 0 ),
                                           QPoint defPos = QPoint( 0, 0 ) )
       {
         QSettings *settings = new QSettings( "vlc", "vlc-qt-interface" );
         settings->beginGroup( configName );
         vlc_bool_t defaultUsed = QVLCTools::restoreWidgetPosition(settings,
                                                                   widget,
                                                                   defSize,
                                                                   defPos);
         settings->endGroup();
         delete settings;

         return defaultUsed;
       }

      /*
        call this method for a window or dialog to show it centred on
        current screen
      */
      static void centerWidgetOnScreen(QWidget *widget)
      {
         QDesktopWidget * const desktop = QApplication::desktop();
         QRect screenRect = desktop->screenGeometry(widget);
         QPoint p1 = widget->frameGeometry().center();

         widget->move ( screenRect.center() - p1 );
      }
};

class QVLCFrame : public QWidget
{
public:
    QVLCFrame( intf_thread_t *_p_intf ) : QWidget( NULL ), p_intf( _p_intf )
    {    };
    virtual ~QVLCFrame()   {};

    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    intf_thread_t *p_intf;

    void readSettings( QString name,
                       QSize defSize = QSize( 0, 0 ),
                       QPoint defPos = QPoint( 0, 0 ) )
    {
        QVLCTools::restoreWidgetPosition(name, this, defSize, defPos);
    }

    void writeSettings( QString name )
    {
        QVLCTools::saveWidgetPosition(name, this);
    }

    virtual void cancel()
    {
        hide();
    }
    virtual void close()
    {
        hide();
    }
    virtual void keyPressEvent( QKeyEvent *keyEvent )
    {
        if( keyEvent->key() == Qt::Key_Escape )
        {
            msg_Dbg( p_intf, "Escp Key pressed" );
            cancel();
        }
        else if( keyEvent->key() == Qt::Key_Return )
        {
             msg_Dbg( p_intf, "Enter Key pressed" );
             close();
         }
    }
};

class QVLCDialog : public QDialog
{
public:
    QVLCDialog( QWidget* parent, intf_thread_t *_p_intf ) :
                                    QDialog( parent ), p_intf( _p_intf )
    {}
    virtual ~QVLCDialog() {};
    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }

protected:
    intf_thread_t *p_intf;

    virtual void cancel()
    {
        hide();
    }
    virtual void close()
    {
        hide();
    }
    virtual void keyPressEvent( QKeyEvent *keyEvent )
    {
        if( keyEvent->key() == Qt::Key_Escape )
        {
            msg_Dbg( p_intf, "Escp Key pressed" );
            cancel();
        }
        else if( keyEvent->key() == Qt::Key_Return )
        {
             msg_Dbg( p_intf, "Enter Key pressed" );
             close();
        }
    }
};

class QVLCMW : public QMainWindow
{
public:
    QVLCMW( intf_thread_t *_p_intf ) : QMainWindow( NULL ), p_intf( _p_intf )
    {    }
    virtual ~QVLCMW() {};
    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    intf_thread_t *p_intf;
    QSize mainSize;

    void readSettings( QString name, QSize defSize )
    {
        QVLCTools::restoreWidgetPosition(name, this, defSize);
    }

    void readSettings( QString name )
    {
        QVLCTools::restoreWidgetPosition(name, this);
    }

    void readSettings( QSettings *settings )
    {
        QVLCTools::restoreWidgetPosition(settings, this);
    }

    void readSettings( QSettings *settings, QSize defSize)
    {
        QVLCTools::restoreWidgetPosition(settings, this, defSize);
    }

    void writeSettings(QString name )
    {
        QVLCTools::saveWidgetPosition(name, this);
    }

    void writeSettings(QSettings *settings )
    {
        QVLCTools::saveWidgetPosition(settings, this);
    }

};

#endif
