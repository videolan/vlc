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
#include <QPlastiqueStyle>
#include <QPushButton>
#include <QKeyEvent>
#include "qt4.hpp"
#include <vlc/vlc.h>
#include <vlc_charset.h>

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

    void readSettings( QString name, QSize defSize )
    {
        QSettings settings( "vlc", "vlc-qt-interface" );
        settings.beginGroup( name );
        resize( settings.value( "size", defSize ).toSize() );
        move( settings.value( "pos", QPoint( 0,0 ) ).toPoint() );
        settings.endGroup();
    }
    void writeSettings( QString name )
    {
        QSettings settings( "vlc", "vlc-qt-interface" );
        settings.beginGroup( name );
        settings.setValue ("size", size() );
        settings.setValue( "pos", pos() );
        settings.endGroup();
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
        QSettings settings( "vlc", "vlc-qt-interface" );
        settings.beginGroup( name );
        QSize s =  settings.value( "size", defSize ).toSize() ;
        fprintf( stderr, "%i %i ", s.width(), s.height() );
        move( settings.value( "pos", QPoint( 0,0 ) ).toPoint() );
        settings.endGroup();
    }
    void readSettings( QString name )
    {
        QSettings settings( "vlc", "vlc-qt-interface" );
        settings.beginGroup( name );
        mainSize = settings.value( "size", QSize( 0,0 ) ).toSize();
        settings.endGroup();
    }
    void writeSettings( QString name )
    {
        QSettings settings( "vlc", "vlc-qt-interface" );
        settings.beginGroup( name );
        settings.setValue ("size", size() );
        settings.setValue( "pos", pos() );
        settings.endGroup();
    }
};

#endif
