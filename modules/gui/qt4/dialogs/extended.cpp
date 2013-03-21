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
#include <QDialogButtonBox>
#include <QPushButton>
#include <vlc_modules.h>

ExtendedDialog::ExtendedDialog( intf_thread_t *_p_intf )
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
#ifdef __APPLE__
    setWindowFlags( Qt::Drawer );
#else
    setWindowFlags( Qt::Tool );
#endif

    setWindowOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );
    setWindowTitle( qtr( "Adjustments and Effects" ) );
    setWindowRole( "vlc-extended" );

    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 2, 0, 1 );
    layout->setSpacing( 3 );

    mainTabW = new QTabWidget( this );

    /* AUDIO effects */
    QWidget *audioWidget = new QWidget;
    QHBoxLayout *audioLayout = new QHBoxLayout( audioWidget );
    QTabWidget *audioTab = new QTabWidget( audioWidget );

    equal = new Equalizer( p_intf, audioTab );
    audioTab->addTab( equal, qtr( "Graphic Equalizer" ) );

    Compressor *compres = new Compressor( p_intf, audioTab );
    audioTab->addTab( compres, qtr( "Compressor" ) );

    Spatializer *spatial = new Spatializer( p_intf, audioTab );
    audioTab->addTab( spatial, qtr( "Spatializer" ) );
    audioLayout->addWidget( audioTab );

    mainTabW->insertTab( AUDIO_TAB, audioWidget, qtr( "Audio Effects" ) );

    /* Video Effects */
    QWidget *videoWidget = new QWidget;
    QHBoxLayout *videoLayout = new QHBoxLayout( videoWidget );
    QTabWidget *videoTab = new QTabWidget( videoWidget );

    videoEffect = new ExtVideo( p_intf, videoTab );
    videoLayout->addWidget( videoTab );
    videoTab->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum );

    mainTabW->insertTab( VIDEO_TAB, videoWidget, qtr( "Video Effects" ) );

    syncW = new SyncControls( p_intf, videoTab );
    mainTabW->insertTab( SYNCHRO_TAB, syncW, qtr( "Synchronization" ) );

    if( module_exists( "v4l2" ) )
    {
        ExtV4l2 *v4l2 = new ExtV4l2( p_intf, mainTabW );
        mainTabW->insertTab( V4L2_TAB, v4l2, qtr( "v4l2 controls" ) );
    }

    layout->addWidget( mainTabW );

    /* Bottom buttons / checkbox line */
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    layout->addLayout( buttonsLayout );

    writeChangesBox = new QCheckBox( qtr("&Write changes to config") );
    buttonsLayout->addWidget( writeChangesBox );
    CONNECT( writeChangesBox, toggled(bool), compres, setSaveToConfig(bool) );
    CONNECT( writeChangesBox, toggled(bool), spatial, setSaveToConfig(bool) );
    CONNECT( writeChangesBox, toggled(bool), equal, setSaveToConfig(bool) );
    CONNECT( mainTabW, currentChanged(int), this, currentTabChanged(int) );

    QDialogButtonBox *closeButtonBox = new QDialogButtonBox( Qt::Horizontal, this );
    closeButtonBox->addButton(
        new QPushButton( qtr("&Close"), this ), QDialogButtonBox::RejectRole );
    buttonsLayout->addWidget( closeButtonBox );

    CONNECT( closeButtonBox, rejected(), this, close() );

    /* Restore geometry or move this dialog on the left pane of the MI */
    if( !restoreGeometry( getSettings()->value("EPanel/geometry").toByteArray() ) )
    {
        resize( QSize( 400, 280 ) );

        MainInterface *p_mi = p_intf->p_sys->p_mi;
        if( p_mi && p_mi->x() > 50 )
            move( ( p_mi->x() - frameGeometry().width() - 10 ), p_mi->y() );
        else
            move ( 450 , 0 );
    }

    CONNECT( THEMIM->getIM(), playingStatusChanged( int ), this, changedItem( int ) );
}

ExtendedDialog::~ExtendedDialog()
{
    getSettings()->setValue("Epanel/geometry", saveGeometry());
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
}

void ExtendedDialog::currentTabChanged( int i )
{
    writeChangesBox->setVisible( i == AUDIO_TAB );
}
