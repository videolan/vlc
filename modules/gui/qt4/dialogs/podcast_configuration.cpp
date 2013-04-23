/*****************************************************************************
 * podcast_configuration.cpp: Podcast configuration dialog
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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

#include "podcast_configuration.hpp"

PodcastConfigDialog::PodcastConfigDialog( intf_thread_t *_p_intf)
                    : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )

{
    ui.setupUi( this );

    ui.podcastDelete->setToolTip( qtr( "Deletes the selected item" ) );
    QPushButton *okButton = new QPushButton( qtr( "&Close" ), this );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ), this );
    ui.okCancel->addButton( okButton, QDialogButtonBox::AcceptRole );
    ui.okCancel->addButton( cancelButton, QDialogButtonBox::RejectRole );

    CONNECT( ui.podcastAdd, clicked(), this, add() );
    CONNECT( ui.podcastDelete, clicked(), this, remove() );
    CONNECT( okButton, clicked(), this, close() );

    char *psz_urls = config_GetPsz( p_intf, "podcast-urls" );
    if( psz_urls )
    {
        char *psz_url = psz_urls;
        while( 1 )
        {
            char *psz_tok = strchr( psz_url, '|' );
            if( psz_tok ) *psz_tok = '\0';
            ui.podcastList->addItem( psz_url );
            if( psz_tok ) psz_url = psz_tok+1;
            else break;
        }
        free( psz_urls );
    }
}

PodcastConfigDialog::~PodcastConfigDialog()
{
}

void PodcastConfigDialog::accept()
{
    QString urls = "";
    for( int i = 0; i < ui.podcastList->count(); i++ )
    {
        urls +=  ui.podcastList->item(i)->text();
        if( i != ui.podcastList->count()-1 ) urls += "|";
    }
    config_PutPsz( p_intf, "podcast-urls", qtu( urls ) );

    if( playlist_IsServicesDiscoveryLoaded( THEPL, "podcast" ) )
    {
        var_SetString( THEPL, "podcast-urls", qtu( urls ) );
        msg_Dbg( p_intf, "You will need to reload the podcast module to take into account deleted podcast urls" );
    }
}

void PodcastConfigDialog::add()
{
    if( ui.podcastURL->text() != QString( "" ) )
    {
        ui.podcastList->addItem( ui.podcastURL->text() );
        ui.podcastURL->clear();
    }
}

void PodcastConfigDialog::remove()
{
    delete ui.podcastList->currentItem();
}
