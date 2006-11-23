/*****************************************************************************
 * streaminfo.cpp : Information about an item
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#include <QTabWidget>
#include <QGridLayout>

#include "dialogs/streaminfo.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "components/infopanels.hpp"
#include "qt4.hpp"

static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param );
MediaInfoDialog *MediaInfoDialog::instance = NULL;

MediaInfoDialog::MediaInfoDialog( intf_thread_t *_p_intf ) :QVLCFrame( _p_intf )
{
    i_runs = 0;
    p_input = NULL;

    setWindowTitle( qtr( "Media information" ) );

    QGridLayout *layout = new QGridLayout(this);
    IT = new InfoTab( this, p_intf, true ) ;
    QPushButton *closeButton = new QPushButton(qtr("&Close"));

    layout->addWidget(IT,0,0,1,3);
    layout->addWidget(closeButton,1,2);

    BUTTONACT( closeButton, close() );
    ON_TIMEOUT( update() );

    var_AddCallback( THEPL, "item-change", ItemChanged, this );
    readSettings( "StreamInfo" , QSize( 500, 450 ) );
}

MediaInfoDialog::~MediaInfoDialog()
{
    var_DelCallback( THEPL, "item-change", ItemChanged, this );
    writeSettings( "StreamInfo" );
}

static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MediaInfoDialog *p_d = (MediaInfoDialog *)param;
    p_d->need_update = VLC_TRUE;
    return VLC_SUCCESS;
}

void MediaInfoDialog::update()
{
    // Timer runs at 150 ms, dont' update more than 2 times per second
    i_runs++;
    if( i_runs % 3 != 0 ) return;

    input_thread_t *p_input = MainInputManager::getInstance( p_intf )->getInput();
    if( !p_input || p_input->b_dead )
    {
        IT->clear();
        return;
    }

    vlc_object_yield( p_input );
    vlc_mutex_lock( &p_input->input.p_item->lock );

    IT->update( p_input->input.p_item, need_update, need_update );
    need_update = false;

    vlc_mutex_unlock( &p_input->input.p_item->lock );
    vlc_object_release( p_input );
}

void MediaInfoDialog::close()
{
    this->toggleVisible();
}
