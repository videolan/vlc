/*****************************************************************************
 * extended.cpp : Extended controls - Undocked
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/extended.hpp"

#include "main_interface.hpp" /* Needed for external MI size */
#include "input_manager.hpp"

#include <QTabWidget>
#include <QGridLayout>

ExtendedDialog *ExtendedDialog::instance = NULL;

ExtendedDialog::ExtendedDialog( intf_thread_t *_p_intf ): QVLCFrame( _p_intf )
{
    setWindowFlags( Qt::Tool );
    setWindowOpacity( config_GetFloat( p_intf, "qt-opacity" ) );
    setWindowTitle( qtr( "Adjustments and Effects" ) );
    setWindowRole( "vlc-extended" );

    QGridLayout *layout = new QGridLayout( this );
    layout->setLayoutMargins( 0, 2, 0, 1, 1 );
    layout->setSpacing( 3 );

    mainTabW = new QTabWidget( this );

    /* AUDIO effects */
    QWidget *audioWidget = new QWidget;
    QHBoxLayout *audioLayout = new QHBoxLayout( audioWidget );
    QTabWidget *audioTab = new QTabWidget( audioWidget );

    equal = new Equalizer( p_intf, audioTab );
    audioTab->addTab( equal, qtr( "Graphic Equalizer" ) );

    Spatializer *spatial = new Spatializer( p_intf, audioTab );
    audioTab->addTab( spatial, qtr( "Spatializer" ) );
    audioLayout->addWidget( audioTab );

    mainTabW->addTab( audioWidget, qtr( "Audio Effects" ) );

    /* Video Effects */
    QWidget *videoWidget = new QWidget;
    QHBoxLayout *videoLayout = new QHBoxLayout( videoWidget );
    QTabWidget *videoTab = new QTabWidget( videoWidget );

    videoEffect = new ExtVideo( p_intf, videoTab );
    videoLayout->addWidget( videoTab );
    videoTab->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum );

    mainTabW->addTab( videoWidget, qtr( "Video Effects" ) );

    syncW = new SyncControls( p_intf, videoTab );
    mainTabW->addTab( syncW, qtr( "Synchronization" ) );

    if( module_exists( "v4l2" ) )
    {
        ExtV4l2 *v4l2 = new ExtV4l2( p_intf, mainTabW );
        mainTabW->addTab( v4l2, qtr( "v4l2 controls" ) );
    }

    layout->addWidget( mainTabW, 0, 0, 1, 5 );

    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    layout->addWidget( closeButton, 1, 4, 1, 1 );
    CONNECT( closeButton, clicked(), this, close() );

    /* Restore geometry or move this dialog on the left pane of the MI */
    if( !restoreGeometry(getSettings()->value("EPanel/geometry").toByteArray()))
    {
        resize( QSize( 400, 280 ) );

        MainInterface *p_mi = p_intf->p_sys->p_mi;
        if( p_mi )
            move( ( p_mi->x() - frameGeometry().width() - 10 ), p_mi->y() );
        else
            move ( 450 , 0 );
    }

    CONNECT( THEMIM->getIM(), statusChanged( int ), this, changedItem( int ) );
}

ExtendedDialog::~ExtendedDialog()
{
    writeSettings( "EPanel" );
}

void ExtendedDialog::showTab( int i )
{
    mainTabW->setCurrentIndex( i );
    show();
}

int ExtendedDialog::currentTab()
{
    return mainTabW->currentIndex();
}

void ExtendedDialog::changedItem( int i_status )
{
    if( i_status != END_S ) return;
    syncW->clean();
    videoEffect->clean();
    equal->clean();
}
