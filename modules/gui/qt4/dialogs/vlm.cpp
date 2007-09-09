/*****************************************************************************
 * sout.cpp : Stream output dialog ( old-style )
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id: sout.cpp 21875 2007-09-08 16:01:33Z jb $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-François Massol <jf.massol -at- gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include "dialogs/vlm.hpp"
#include "qt4.hpp"
#include <vlc_streaming.h>

#include <iostream>
#include <QString>
#include <QFileDialog>

VLMDialog *VLMDialog::instance = NULL;


VLMDialog::VLMDialog( intf_thread_t *_p_intf) : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "VLM front-end" ) );

    /* UI stuff */
    ui.setupUi( this );
}

VLMDialog::~VLMDialog(){}

void VLMDialog::close(){
    close();
}
    
