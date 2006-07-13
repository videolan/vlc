/*****************************************************************************
 * prefs_dialog.cpp : Preferences
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

#include "dialogs/prefs_dialog.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"

#include "components/preferences.hpp"
#include "qt4.hpp"

#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
PrefsDialog *PrefsDialog::instance = NULL;

PrefsDialog::PrefsDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
     layout = new QHBoxLayout();
     QVBoxLayout * main_layout = new QVBoxLayout();
     setWindowTitle( _("Preferences" ) );
     resize( 800, 450 );

          advanced_tree = NULL;
     simple_tree = NULL;
     simple_panel = NULL;
     advanced_panel = NULL;

     vertical = new QVBoxLayout();

     // Choice for types
     types = new QGroupBox( "Show settings" );
     QHBoxLayout *tl = new QHBoxLayout();
     tl->setSpacing( 3 );
     small = new QRadioButton( "Basic", types );
     all = new QRadioButton( "All", types );
     tl->addWidget( small );
     tl->addWidget( all );
     types->setLayout(tl );

     layout->addLayout( vertical, 1 );

     all->setChecked( true );

     main_layout->addLayout( layout );
     adv_chk = new QCheckBox("Advanced options");
     main_layout->addWidget( adv_chk );

     setAll();

     connect( adv_chk, SIGNAL( toggled(bool) ),
              this, SLOT( setAdvanced( bool ) ) );
     setLayout( main_layout );

     connect( small, SIGNAL( clicked() ), this, SLOT( setSmall()) );
     connect( all, SIGNAL( clicked() ), this, SLOT( setAll()) );
}

void PrefsDialog::setAdvanced( bool advanced )
{
    if( advanced_panel )
        advanced_panel->setAdvanced( advanced );
}

void PrefsDialog::setAll()
{
    while( (vertical->takeAt(0)) != 0  ) {}
    if( simple_tree )
        simple_tree->hide();

    if( !advanced_tree )
    {
         advanced_tree = new PrefsTree( p_intf, this );
         connect( advanced_tree,
          SIGNAL( currentItemChanged( QTreeWidgetItem *, QTreeWidgetItem *) ),
          this, SLOT( changePanel( QTreeWidgetItem * ) ) );
    }
    advanced_tree->show();
    setAdvanced( adv_chk->isChecked() );
    vertical->addWidget( types );
    vertical->addWidget( advanced_tree );

     if( layout->count() == 2 )
         layout->takeAt(1);

     if( !advanced_panel )
         advanced_panel = new PrefsPanel( this );
     layout->addWidget( advanced_panel, 3 ) ;
}

void PrefsDialog::setSmall()
{
    while( (vertical->takeAt(0)) != 0  ) {}
    if( advanced_tree )
        advanced_tree->hide();

    if( !simple_tree )
         simple_tree = new QTreeWidget();
    simple_tree->show();
    vertical->addWidget( types );
    vertical->addWidget( simple_tree );

    if( layout->count() == 2 )
        layout->takeAt(1);

    if( !simple_panel )
        simple_panel = new QWidget();
    layout->addWidget( simple_panel, 3 ) ;
}


void PrefsDialog::init()
{
}

PrefsDialog::~PrefsDialog()
{
}

void PrefsDialog::changePanel( QTreeWidgetItem *item )
{
    PrefsItemData *data = item->data( 0, Qt::UserRole ).value<PrefsItemData*>();

    if( advanced_panel )
    {
        layout->removeWidget( advanced_panel );
        advanced_panel->hide();
    }
    if( !data->panel )
    {
        data->panel = new PrefsPanel( p_intf, this, data );
    }
    advanced_panel = data->panel;
    advanced_panel->show();
    setAdvanced( adv_chk->isChecked() );
    layout->addWidget( advanced_panel, 3 );

}
