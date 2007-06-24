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

#include "dialogs/mediainfo.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "components/infopanels.hpp"
#include "qt4.hpp"

#include <QTabWidget>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>

static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param );
MediaInfoDialog *MediaInfoDialog::instance = NULL;

MediaInfoDialog::MediaInfoDialog( intf_thread_t *_p_intf, bool _mainInput,
                                  bool _stats ) :
                                  QVLCFrame( _p_intf ), mainInput(_mainInput),
                                  stats( _stats )
{
    i_runs = 0;
    p_input = NULL;

    setWindowTitle( qtr( "Media information" ) );
    resize( 700 , 450 );

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
        IT->addTab( ISP, qtr( "&Stats" ) );
    }

    QGridLayout *layout = new QGridLayout( this );

    /* FIXME GNOME/KDE ? */
    saveMetaButton = new QPushButton( qtr( "&Save Metadata" ) );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    closeButton->setDefault( true );

    uriLine = new QLineEdit;
    QLabel *uriLabel = new QLabel( qtr( "Location :" ) );

    layout->addWidget( IT, 0, 0, 1, 8 );
    layout->addWidget( uriLabel, 1, 0, 1, 1 );
    layout->addWidget( uriLine, 1, 1, 1, 7 );
    layout->addWidget( saveMetaButton, 2, 6 );
    layout->addWidget( closeButton, 2, 7 );

    BUTTONACT( closeButton, close() );

    /* The tabs buttons are shown in the main dialog for space and cosmetics */
    CONNECT( saveMetaButton, clicked(), MP, saveMeta() );

    /* Let the MetaData Panel update the URI */
    CONNECT( MP, uriSet( QString ), uriLine, setText( QString ) );

    CONNECT( IT, currentChanged ( int ), this, updateButtons( int ) );


    /* Create the main Update function with a time (150ms) */
    if( mainInput ) {
        ON_TIMEOUT( update() );
        var_AddCallback( THEPL, "item-change", ItemChanged, this );
    }
}

MediaInfoDialog::~MediaInfoDialog()
{
    if( mainInput ) {
        var_DelCallback( THEPL, "item-change", ItemChanged, this );
    }
    writeSettings( "mediainfo" );
}

void MediaInfoDialog::showTab( int i_tab = 0 )
{
    this->show();
    IT->setCurrentIndex( i_tab );
}

static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MediaInfoDialog *p_d = (MediaInfoDialog *)param;
    p_d->need_update = VLC_TRUE;
    return VLC_SUCCESS;
}

void MediaInfoDialog::setInput( input_item_t *p_input )
{
    clear();
    vlc_mutex_lock( &p_input->lock );
    update( p_input, true, true );
    vlc_mutex_unlock( &p_input->lock );
}

void MediaInfoDialog::update()
{
    /* Timer runs at 150 ms, dont' update more than 2 times per second */
    if( i_runs % 4 != 0 ) return;
    i_runs++;

    /* Get Input and clear if non-existant */
    input_thread_t *p_input =
                     MainInputManager::getInstance( p_intf )->getInput();
    if( !p_input || p_input->b_dead )
    {
        clear();
        return;
    }

    vlc_object_yield( p_input );
    vlc_mutex_lock( &input_GetItem(p_input)->lock );

    update( input_GetItem(p_input), need_update, need_update );
    need_update = false;

    vlc_mutex_unlock( &input_GetItem(p_input)->lock );
    vlc_object_release( p_input );
}

void MediaInfoDialog::update( input_item_t *p_item, bool update_info,
                                                    bool update_meta )
{
    MP->p_input = p_item;
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
}

void MediaInfoDialog::close()
{
    this->toggleVisible();

    if( mainInput == false ) {
        deleteLater();
    }
}

void MediaInfoDialog::updateButtons( int i_tab )
{
    msg_Dbg( p_intf, "Coin Coin, Tab number: %i", i_tab );

    if( i_tab == 0 ) saveMetaButton->show();
    else saveMetaButton->hide();
}
