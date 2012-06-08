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
#include <QHBoxLayout>
#include <QApplication>
#include <QMainWindow>
#include <QKeyEvent>
#include <QDesktopWidget>
#include <QSettings>
#include <QStyle>

#include "qt4.hpp"

class QVLCTools
{
   public:
       /*
        use this function to save a widgets screen position
        only for windows / dialogs which are floating, if a
        window is docked into another - don't all this function
        or it may write garbage to position info!
       */
       static void saveWidgetPosition( QSettings *settings, QWidget *widget)
       {
         settings->setValue("geometry", widget->saveGeometry());
       }
       static void saveWidgetPosition( intf_thread_t *p_intf,
                                       const QString& configName,
                                       QWidget *widget)
       {
         getSettings()->beginGroup( configName );
         QVLCTools::saveWidgetPosition(getSettings(), widget);
         getSettings()->endGroup();
       }


       /*
         use this method only for restoring window state of non docked
         windows!
       */
       static bool restoreWidgetPosition(QSettings *settings,
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
               widget->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, widget->size(), qApp->desktop()->availableGeometry()));
            return true;
          }
          return false;
       }

       static bool restoreWidgetPosition( intf_thread_t *p_intf,
                                           const QString& configName,
                                           QWidget *widget,
                                           QSize defSize = QSize( 0, 0 ),
                                           QPoint defPos = QPoint( 0, 0 ) )
       {
         getSettings()->beginGroup( configName );
         bool defaultUsed = QVLCTools::restoreWidgetPosition( getSettings(),
                                                                   widget,
                                                                   defSize,
                                                                   defPos);
         getSettings()->endGroup();

         return defaultUsed;
       }
};

class QVLCFrame : public QWidget
{
public:
    QVLCFrame( intf_thread_t *_p_intf ) : QWidget( NULL ), p_intf( _p_intf )
    {};
    virtual ~QVLCFrame()   {};

    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    intf_thread_t *p_intf;

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
    virtual void keyPressEvent( QKeyEvent *keyEvent )
    {
        if( keyEvent->key() == Qt::Key_Escape )
        {
            this->cancel();
        }
        else if( keyEvent->key() == Qt::Key_Return
              || keyEvent->key() == Qt::Key_Enter )
        {
             this->close();
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
            this->cancel();
        }
        else if( keyEvent->key() == Qt::Key_Return
              || keyEvent->key() == Qt::Key_Enter )
        {
            this->close();
        }
    }
};

class QVLCMW : public QMainWindow
{
public:
    QVLCMW( intf_thread_t *_p_intf ) : QMainWindow( NULL ), p_intf( _p_intf ){}
    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    intf_thread_t *p_intf;
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
