/*****************************************************************************
 * interaction.cpp : Interaction stuff
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

#include "dialogs/interaction.hpp"
#include "util/qvlcframe.hpp"
#include <vlc/intf.h>
#include "qt4.hpp"

InteractionDialog::InteractionDialog( intf_thread_t *_p_intf,
                         interaction_dialog_t *_p_dialog ) : QWidget( 0 ),
                          p_intf( _p_intf), p_dialog( _p_dialog )
{
    QVBoxLayout *layout = new QVBoxLayout;
    uiLogin = NULL;
    uiProgress = NULL;
    uiInput = NULL;


    if( p_dialog->i_flags & DIALOG_BLOCKING_ERROR )
    {

    }
    else if( p_dialog->i_flags & DIALOG_NONBLOCKING_ERROR )
    {
        // Create instance of the errors dialog
    }
    else if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
    {
        description = new QLabel( 0 );
        description->setText( qfu(p_dialog->psz_description) );
        layout->addWidget(description);
    }
    else if( p_dialog->i_flags & DIALOG_LOGIN_PW_OK_CANCEL )
    {
        uiLogin = new Ui::LoginDialog;
        uiLogin->setupUi( this );
        uiLogin->description->setText( qfu(p_dialog->psz_description) );
    }
    else if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
    {

    }
    else if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
    {
    }
    else
        msg_Err( p_intf, "unknown dialog type" );

    QVLCFrame::doButtons( this, layout,
                          defaultButton, p_dialog->psz_default_button,
                          altButton, p_dialog->psz_alternate_button,
                          otherButton, p_dialog->psz_other_button );
    if( p_dialog->psz_default_button )
        connect( defaultButton, SIGNAL( clicked() ), this, SLOT( defaultB() ) );
    if( p_dialog->psz_alternate_button )
        connect( altButton, SIGNAL( clicked() ), this, SLOT( altB() ) );
    if( p_dialog->psz_other_button )
        connect( otherButton, SIGNAL( clicked() ), this, SLOT( otherB() ) );
 
    setLayout( layout );
    setWindowTitle( qfu( p_dialog->psz_title ) );
}

void InteractionDialog::Update()
{
}

InteractionDialog::~InteractionDialog()
{
    if( uiInput ) delete uiInput;
    if( uiProgress) delete uiProgress;
    if( uiLogin ) delete uiLogin;
}

void InteractionDialog::defaultB()
{
    Finish( DIALOG_OK_YES );
}
void InteractionDialog::altB()
{
    Finish( DIALOG_NO );
}
void InteractionDialog::otherB()
{
    Finish( DIALOG_CANCELLED );
}

void InteractionDialog::Finish( int i_ret )
{
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock ); 

    if( p_dialog->i_flags & DIALOG_LOGIN_PW_OK_CANCEL )
    {
        p_dialog->psz_returned[0] = strdup(
                               uiLogin->loginEdit->text().toUtf8().data() );
        p_dialog->psz_returned[1] = strdup(
                               uiLogin->passwordEdit->text().toUtf8().data() );
    }
    else if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
    {
        p_dialog->psz_returned[0] = strdup(
                               uiInput->inputEdit->text().toUtf8().data() );
    }
    p_dialog->i_status = ANSWERED_DIALOG;
    p_dialog->i_return = i_ret;
    hide();
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock ); 
}
