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

#include "input_manager.hpp"
#include "dialogs/streaminfo.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "components/infopanels.hpp"
#include "qt4.hpp"

StreamInfoDialog *StreamInfoDialog::instance = NULL;

StreamInfoDialog::StreamInfoDialog( intf_thread_t *_p_intf, bool _main_input ) :
                              QVLCFrame( _p_intf ), main_input( _main_input )
{
    setWindowTitle( _("Stream information" ) );
    ISP = new InputStatsPanel( this, p_intf );
    ON_TIMEOUT( update() );
    p_input = NULL;
}

void StreamInfoDialog::update()
{
    if( main_input )
        p_input = MainInputManager::getInstance( p_intf )->getInput();
    if( p_input && !p_input->b_dead )
        ISP->Update( p_input->input.p_item );
}

StreamInfoDialog::~StreamInfoDialog()
{
}
