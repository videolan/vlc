/*****************************************************************************
 * simple_preferences.cpp : "Simple preferences"
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: preferences.cpp 16348 2006-08-25 21:10:10Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea@videolan.org>
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

#include "components/simple_preferences.hpp"
#include "components/preferences_widgets.hpp"
#include "qt4.hpp"
#include <vlc_config_cat.h>
#include <assert.h>
#include <QListWidget>
#include <QListWidgetItem>
#include <QString>
#include <QFont>

#include "pixmaps/audio.xpm"
#include "pixmaps/video.xpm"
#include "ui/sprefs_trivial.h"

#define ITEM_HEIGHT 25

/*********************************************************************
 * The List of categories
 *********************************************************************/
SPrefsCatList::SPrefsCatList( intf_thread_t *_p_intf, QWidget *_parent ) :
                                  QListWidget( _parent ), p_intf( _p_intf )
{
    setIconSize( QSize( ITEM_HEIGHT,ITEM_HEIGHT ) );
    setAlternatingRowColors( true );

#ifndef WIN32
    // Fixme - A bit UGLY
    QFont f = font();
    int pSize = f.pointSize();
    if( pSize > 0 )
        f.setPointSize( pSize + 1 );
    else
        f.setPixelSize( f.pixelSize() + 1 );
    setFont( f );
#endif

    addItem( "Very trivial" );
    item(0)->setIcon( QIcon( QPixmap( audio_xpm ) ) );
    item(0)->setData( Qt::UserRole, qVariantFromValue( 0 ) );
    addItem( "Video" );
    item(1)->setIcon( QIcon( QPixmap( video_xpm ) ) );
    item(1)->setData( Qt::UserRole, qVariantFromValue( 1 ) );
}

void SPrefsCatList::ApplyAll()
{
    DoAll( false );
}

void SPrefsCatList::CleanAll()
{
    DoAll( true );
}

/// \todo When cleaning, we should remove the panel ?
void SPrefsCatList::DoAll( bool doclean )
{
    /* Todo */
}

/*********************************************************************
 * The Panels
 *********************************************************************/
SPrefsPanel::SPrefsPanel( intf_thread_t *_p_intf, QWidget *_parent,
                          int number ) : QWidget( _parent ), p_intf( _p_intf )
{
    if( number == 0 )
    {
        Ui::SPrefsTrivial ui;
        ui.setupUi( this );
        module_config_t *p_config = config_FindConfig( VLC_OBJECT(p_intf),
                                                        "memcpy" );
        ConfigControl *control = new ModuleConfigControl( VLC_OBJECT(p_intf),
                        p_config, ui.memcpyLabel, ui.memcpyCombo, false );
        controls.append( control );
    }
    else
    {
        int *p = NULL;
        fprintf( stderr, "Ha ha ca te fait bien la bite\n" );
        *p=42;
    }
}

void SPrefsPanel::Apply()
{
    /* todo: factorize with PrefsPanel  */
    QList<ConfigControl *>::Iterator i;
    for( i = controls.begin() ; i != controls.end() ; i++ )
    {
        VIntConfigControl *vicc = qobject_cast<VIntConfigControl *>(*i);
        if( !vicc )
        {
            VFloatConfigControl *vfcc = qobject_cast<VFloatConfigControl *>(*i);
            if( !vfcc)
            {
                VStringConfigControl *vscc =
                               qobject_cast<VStringConfigControl *>(*i);
                assert( vscc );
                config_PutPsz( p_intf, vscc->getName().toAscii().data(),
                                       vscc->getValue().toAscii().data() );
                continue;
            }
            config_PutFloat( p_intf, vfcc->getName().toAscii().data(),
                                     vfcc->getValue() );
            continue;
        }
        config_PutInt( p_intf, vicc->getName().toAscii().data(),
                               vicc->getValue() );
    }
}

void SPrefsPanel::Clean()
{}
