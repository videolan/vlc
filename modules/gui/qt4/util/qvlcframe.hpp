/*****************************************************************************
 * qvlcframe.hpp : A few helpers
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _QVLCFRAME_H_
#define _QVLCFRAME_H_

#include <QWidget>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QApplication>
#include <QSettings>
#include <QMainWindow>
#include <QPlastiqueStyle>
#include <QPushButton>
#include "qt4.hpp"
#include <vlc/vlc.h>

class QVLCFrame : public QWidget
{
public:
    static void fixStyle( QWidget *w)
    {
         QStyle *style = qApp->style();
#if 0
        // Plastique is too dark.
        /// theming ? getting KDE data ? ?
        if( qobject_cast<QPlastiqueStyle *>(style) )
        {
            QPalette plt( w->palette() );
            plt.setColor( QPalette::Active, QPalette::Highlight, Qt::gray );
            QColor vlg = (Qt::lightGray);
            vlg = vlg.toHsv();
            vlg.setHsv( vlg.hue(), vlg.saturation(), 235  );
            plt.setColor( QPalette::Active, QPalette::Window, vlg );
            plt.setColor( QPalette::Inactive, QPalette::Window, vlg );
            w->setPalette( plt );
        }
#endif
    }
    static void doButtons( QWidget *w, QBoxLayout *l,
                           QPushButton *defaul, char *psz_default,
                           QPushButton *alt, char *psz_alt,
                           QPushButton *other, char *psz_other )
    {
#ifdef QT42
#else
        QHBoxLayout *buttons_layout = new QHBoxLayout;
        QSpacerItem *spacerItem = new QSpacerItem( 40, 20,
                               QSizePolicy::Expanding, QSizePolicy::Minimum);
        buttons_layout->addItem( spacerItem );

        if( psz_default )
        {
            defaul = new QPushButton;
            buttons_layout->addWidget( defaul );
            defaul->setText( qfu( psz_default ) );
        }
        if( psz_alt )
        {
            alt = new QPushButton;
            buttons_layout->addWidget( alt );
            alt->setText( qfu( psz_alt ) );
        }
        if( psz_other )
        {
            other = new QPushButton;
            buttons_layout->addWidget( other );
            other->setText( qfu( psz_other ) );
        }
        l->addLayout( buttons_layout );
#endif
    };

    QVLCFrame( intf_thread_t *_p_intf ) : QWidget( NULL ), p_intf( _p_intf )
    {
        fixStyle( this );
    };
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
        QSettings settings( "VideoLAN", "VLC" );
        settings.beginGroup( name );
        resize( settings.value( "size", defSize ).toSize() );
        move( settings.value( "pos", QPoint( 0,0 ) ).toPoint() );
        settings.endGroup();
    }
    void writeSettings( QString name )
    {
        QSettings settings( "VideoLAN", "VLC" );
        settings.beginGroup( name );
        settings.setValue ("size", size() );
        settings.setValue( "pos", pos() );
        settings.endGroup();
    }
};

class QVLCMW : public QMainWindow
{
public:
    QVLCMW( intf_thread_t *_p_intf ) : QMainWindow( NULL ), p_intf( _p_intf )
    {
        QVLCFrame::fixStyle( this );
    }
    virtual ~QVLCMW() {};
protected:
    intf_thread_t *p_intf;
    QSize mainSize;

    void readSettings( QString name, QSize defSize )
    {
        QSettings settings( "VideoLAN", "VLC" );
        settings.beginGroup( name );
        mainSize = settings.value( "size", defSize ).toSize();
        QPoint npos = settings.value( "pos", QPoint( 0,0 ) ).toPoint();
        if( npos.x() > 0 )
            move( npos );
        settings.endGroup();
    }
    void readSettings( QString name )
    {
        QSettings settings( "VideoLAN", "VLC" );
        settings.beginGroup( name );
        mainSize = settings.value( "size", QSize( 0,0 ) ).toSize();
        settings.endGroup();
    }
    void writeSettings( QString name )
    {
        QSettings settings( "VideoLAN", "VLC" );
        settings.beginGroup( name );
        settings.setValue ("size", size() );
        settings.setValue( "pos", pos() );
        settings.endGroup();
    }
};

#endif
