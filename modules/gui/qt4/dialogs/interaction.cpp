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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/errors.hpp"
#include "dialogs/interaction.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QMessageBox>
#include <QDialogButtonBox>

#include <assert.h>

InteractionDialog::InteractionDialog( intf_thread_t *_p_intf,
                         interaction_dialog_t *_p_dialog ) : QObject( 0 ),
                          p_intf( _p_intf), p_dialog( _p_dialog )
{
    QVBoxLayout *layout = NULL;
    description = NULL;
    panel = NULL;
    dialog = NULL;
    altButton = NULL;

    if( (p_dialog->i_flags & DIALOG_INTF_PROGRESS ) ||
             ( p_dialog->i_flags & DIALOG_USER_PROGRESS ) )
    {
        dialog = new QWidget; layout = new QVBoxLayout( dialog );
        layout->setMargin( 2 );
        description = new QLabel( qfu( p_dialog->psz_description ) );
        layout->addWidget( description );

        progressBar = new QProgressBar;
        progressBar->setMaximum( 1000 );
        progressBar->setTextVisible( true );
        progressBar->setOrientation( Qt::Horizontal );
        layout->addWidget( progressBar );
    }
    else
    {
        msg_Err( p_intf, "Unknown dialog type %i", p_dialog->i_flags );
        return;
    }

    /* Custom dialog, finish it */
    {
        assert( dialog );
        /* Start the DialogButtonBox config */
        QDialogButtonBox *buttonBox = new QDialogButtonBox;

        if( p_dialog->psz_alternate_button )
        {
            altButton = new QPushButton;
            altButton->setText( "&" + qfu( p_dialog->psz_alternate_button ) );
            buttonBox->addButton( altButton, QDialogButtonBox::RejectRole );
        }
        layout->addWidget( buttonBox );
        /* End the DialogButtonBox */

        /* CONNECTs */
        if( p_dialog->psz_alternate_button ) BUTTONACT( altButton, altB() );

        /* set the layouts and thte title */
        dialog->setLayout( layout );
        dialog->setWindowTitle( qfu( p_dialog->psz_title ) );
    }
}

void InteractionDialog::update()
{
    if( p_dialog->i_flags & DIALOG_USER_PROGRESS ||
        p_dialog->i_flags & DIALOG_INTF_PROGRESS )
    {
        assert( progressBar );
        progressBar->setValue( (int)( p_dialog->val.f_float * 10 ) );
        if( description )
            description->setText( qfu( p_dialog->psz_description ) );
    }
    else return;

    if( ( p_dialog->i_flags & DIALOG_INTF_PROGRESS ) &&
        ( p_dialog->val.f_float >= 100.0 ) )
    {
        progressBar->hide();
        msg_Dbg( p_intf, "Progress Done" );
    }

    if( ( p_dialog->i_flags & DIALOG_USER_PROGRESS ) &&
        ( p_dialog->val.f_float >= 100.0 ) )
    {
        assert( altButton );
        altButton->setText( qtr( "&Close" ) );
    }
}

InteractionDialog::~InteractionDialog()
{
    delete dialog;
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
    vlc_mutex_lock( p_dialog->p_lock );

    /* Special cases when we have to return psz to the core */
    if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
    {
        p_dialog->psz_returned[0] = strdup( qtu( inputEdit->text() ) );
    }

    /* We finished the dialog, answer it */
    p_dialog->i_status = ANSWERED_DIALOG;
    p_dialog->i_return = i_ret;

    /* Alert the Dialog_*_Progress that the user had clicked on "cancel" */
    if( p_dialog->i_flags & DIALOG_USER_PROGRESS ||
        p_dialog->i_flags & DIALOG_INTF_PROGRESS )
        p_dialog->b_cancelled = true;

    vlc_mutex_unlock( p_dialog->p_lock );

    hide();
}

