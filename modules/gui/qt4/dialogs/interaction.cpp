/*****************************************************************************
 * interaction.cpp : Interaction stuff
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

#include "dialogs/interaction.hpp"
#include <vlc/intf.h>

InteractionDialog::InteractionDialog( intf_thread_t *_p_intf,
                                      interaction_dialog_t *_p_dialog ) :
                                        p_intf( _p_intf), p_dialog( _p_dialog )
{
    if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
    {
        uiOkCancel = new Ui::OKCancelDialog;
        uiOkCancel->setupUi( this );
        uiOkCancel->description->setText( p_dialog->psz_description );
        connect( uiOkCancel->okButton, SIGNAL( clicked() ),
                 this, SLOT( OK() ) );
        connect( uiOkCancel->cancelButton, SIGNAL( clicked() ),
                 this, SLOT( cancel() ) );
    }
    else if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
    {
      
    }
    else if( p_dialog->i_flags & DIALOG_LOGIN_PW_OK_CANCEL )
    {

    }
    else if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
    {

    }
    else if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
    {
    }
    else
        msg_Err( p_intf, "unknown dialog type" );
}

void InteractionDialog::Update()
{
}

InteractionDialog::~InteractionDialog()
{
}

void InteractionDialog::OK()
{
    Finish( DIALOG_OK_YES, NULL, NULL );
}

void InteractionDialog::cancel()
{
    Finish( DIALOG_CANCELLED, NULL, NULL );
}

void InteractionDialog::Finish( int i_ret, QString *r1, QString *r2 )
{
   vlc_mutex_lock( &p_dialog->p_interaction->object_lock ); 

   p_dialog->i_status = ANSWERED_DIALOG;
   p_dialog->i_return = i_ret;
   hide();
   vlc_mutex_unlock( &p_dialog->p_interaction->object_lock ); 
}
