/*****************************************************************************
 * streaminfo.cpp : Information about an item
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

#include <QTabWidget>
#include <QGridLayout>

#include "dialogs/streaminfo.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "components/infopanels.hpp"
#include "qt4.hpp"

/* This is the dialog Windows */
StreamInfoDialog *StreamInfoDialog::instance = NULL;

StreamInfoDialog::StreamInfoDialog( intf_thread_t *_p_intf, bool _main_input ) :
                              QVLCFrame( _p_intf ), main_input( _main_input )
{
    setWindowTitle( _("Stream information" ) );
    QGridLayout *layout = new QGridLayout(this);
    setGeometry(0,0,470,550);

    IT = new InfoTab( this, p_intf) ;
    QPushButton *closeButton = new QPushButton(qtr("&Close"));
    layout->addWidget(IT,0,0,1,3);
    layout->addWidget(closeButton,1,2);

    BUTTONACT( closeButton, close() );
    ON_TIMEOUT( update() );
    p_input = NULL;
}

void StreamInfoDialog::update()
{
    IT->update();
}

StreamInfoDialog::~StreamInfoDialog()
{
}

void StreamInfoDialog::close()
{
    this->toggleVisible();
}

/* This is the tab Widget Inside the windows*/
InfoTab::InfoTab( QWidget *parent,  intf_thread_t *_p_intf ) : 
                    QTabWidget( parent ), p_intf( _p_intf )
{
  setGeometry(0, 0, 400, 500);

  ISP = new InputStatsPanel( NULL, p_intf );
  MP = new MetaPanel(NULL, p_intf);
  IP = new InfoPanel(NULL, p_intf);

  addTab(MP, qtr("&Meta"));
  addTab(ISP, qtr("&Stats"));
  addTab(IP, qtr("&Info"));
}

InfoTab::~InfoTab()
{
}

void InfoTab::update()
{
    if( p_intf )
        p_input = MainInputManager::getInstance( p_intf )->getInput();
    if( p_input && !p_input->b_dead )
        ISP->Update( p_input->input.p_item );
}

