/*****************************************************************************
 * mediainfo.cpp : Information about an item
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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
 ******************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/mediainfo.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"

#include <QTabWidget>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>

MediaInfoDialog *MediaInfoDialog::instance = NULL;

/* This Dialog has two main modes:
    - General Mode that shows the current Played item, and the stats
    - Single mode that shows the info on ONE SINGLE Item on the playlist
   Please be Careful of not breaking one the modes behaviour... */

MediaInfoDialog::MediaInfoDialog( intf_thread_t *_p_intf,
                                  input_item_t *_p_item,
                                  bool _mainInput,
                                  bool _stats ) :
                                  QVLCFrame( _p_intf ), mainInput(_mainInput),
                                  stats( _stats )
{
    p_item = _p_item;
    b_cleaned = true;
    i_runs = 0;

    setWindowTitle( qtr( "Media Information" ) );

    /* TabWidgets and Tabs creation */
    IT = new QTabWidget;
    MP = new MetaPanel( IT, p_intf );
    IT->addTab( MP, qtr( "&General" ) );
    EMP = new ExtraMetaPanel( IT, p_intf );
    IT->addTab( EMP, qtr( "&Extra Metadata" ) );
    IP = new InfoPanel( IT, p_intf );
    IT->addTab( IP, qtr( "&Codec Details" ) );
    if( stats )
    {
        ISP = new InputStatsPanel( IT, p_intf );
        IT->addTab( ISP, qtr( "&Statistics" ) );
    }

    QGridLayout *layout = new QGridLayout( this );

    /* No need to use a QDialogButtonBox here */
    saveMetaButton = new QPushButton( qtr( "&Save Metadata" ) );
    saveMetaButton->hide();
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    closeButton->setDefault( true );

    uriLine = new QLineEdit;
    QLabel *uriLabel = new QLabel( qtr( "Location:" ) );

    layout->addWidget( IT, 0, 0, 1, 8 );
    layout->addWidget( uriLabel, 1, 0, 1, 1 );
    layout->addWidget( uriLine, 1, 1, 1, 7 );
    layout->addWidget( saveMetaButton, 2, 6 );
    layout->addWidget( closeButton, 2, 7 );

    BUTTONACT( closeButton, close() );

    /* The tabs buttons are shown in the main dialog for space and cosmetics */
    BUTTONACT( saveMetaButton, saveMeta() );

    /* Let the MetaData Panel update the URI */
    CONNECT( MP, uriSet( QString ), uriLine, setText( QString ) );
    CONNECT( MP, editing(), this, showMetaSaveButton() );

    CONNECT( IT, currentChanged( int ), this, updateButtons( int ) );

    /* If using the General Mode */
    if( !p_item )
    {
        msg_Dbg( p_intf, "Using a general windows" );
        CONNECT( THEMIM, inputChanged( input_thread_t * ),
                 this, update( input_thread_t * ) );

        if( THEMIM->getInput() )
            p_item = input_GetItem( THEMIM->getInput() );
    }

    /* Call update by hand, so info is shown from current item too */
    if( p_item )
        update( p_item, true, true );

    if( stats )
        ON_TIMEOUT( updateOnTimeOut() );

    readSettings( "Mediainfo", QSize( 600 , 480 ) );
}

MediaInfoDialog::~MediaInfoDialog()
{
    writeSettings( "Mediainfo" );
}

void MediaInfoDialog::showTab( int i_tab = 0 )
{
    IT->setCurrentIndex( i_tab );
    show();
}

void MediaInfoDialog::showMetaSaveButton()
{
    saveMetaButton->show();
}

void MediaInfoDialog::saveMeta()
{
    MP->saveMeta();
    saveMetaButton->hide();
}

/* Function called on inputChanged-update*/
void MediaInfoDialog::update( input_thread_t *p_input )
{
    if( !p_input || p_input->b_dead )
    {
        if( !b_cleaned )
        {
            clear();
            b_cleaned = true;
        }
        return;
    }

    /* Launch the update in all the panels */
    vlc_object_yield( p_input );

    update( input_GetItem(p_input), true, true);

    vlc_object_release( p_input );
}

void MediaInfoDialog::updateOnTimeOut()
{
    /* Timer runs at 150 ms, dont' update more than 2 times per second */
    i_runs++;
    if( i_runs % 4 != 0 ) return;

    /* Get Input and clear if non-existant */
    input_thread_t *p_input = THEMIM->getInput();

    if( p_input && !p_input->b_dead )
    {
        vlc_object_yield( p_input );
        update( input_GetItem(p_input), false, false);
        vlc_object_release( p_input );
    }
}

void MediaInfoDialog::update( input_item_t *p_item,
                              bool update_info,
                              bool update_meta )
{
    if( update_info )
        IP->update( p_item );
    if( update_meta )
    {
        MP->update( p_item );
        EMP->update( p_item );
    }
    if( stats )
        ISP->update( p_item );
}

void MediaInfoDialog::clear()
{
    IP->clear();
    MP->clear();
    EMP->clear();
    if( stats ) ISP->clear();
    b_cleaned = true;
}

void MediaInfoDialog::close()
{
    toggleVisible();

    /* if dialog is closed, revert editing if not saved */
    if( MP->isInEditMode() )
    {
        MP->setEditMode( false );
        updateButtons( 0 );
    }
    if( mainInput == false ) {
        deleteLater();
    }
}

void MediaInfoDialog::updateButtons( int i_tab )
{
    if( MP->isInEditMode() && i_tab == 0 )
        saveMetaButton->show();
    else
        saveMetaButton->hide();
}
