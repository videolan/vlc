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
     QGridLayout *main_layout = new QGridLayout(this);
     setWindowTitle( qtr("Preferences" ) );
     resize( 800, 450 );

     tree_panel = new QWidget(0);
     tree_panel_l = new QHBoxLayout;
     tree_panel->setLayout( tree_panel_l );
     main_panel = new QWidget(0);
     main_panel_l = new QHBoxLayout;
     main_panel->setLayout( main_panel_l );

     // Choice for types
     types = new QGroupBox( "Show settings" );
     QHBoxLayout *tl = new QHBoxLayout(0);
     tl->setSpacing( 3 ); tl->setMargin( 3 );
     small = new QRadioButton( "Basic", types ); tl->addWidget( small );
     all = new QRadioButton( "All", types ); tl->addWidget( all );
     types->setLayout(tl);
     all->setChecked( true );

     adv_chk = new QCheckBox("Advanced options");

     advanced_tree = NULL;
     simple_tree = NULL;
     simple_panel = NULL;
     advanced_panel = NULL;

     main_layout->addWidget( types, 0,0,1,1 );
     main_layout->addWidget( tree_panel, 1,0,1,1 );
     main_layout->addWidget( adv_chk , 2,0,1,1 );
     main_layout->addWidget( main_panel, 0, 1, 3, 1 );

     main_layout->setColumnMinimumWidth( 0, 200 );
     main_layout->setColumnStretch( 0, 1 );
     main_layout->setColumnStretch( 1,3 );

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
    if( simple_tree )
    {
        tree_panel_l->removeWidget( simple_tree );
        simple_tree->hide();
    }

    if( !advanced_tree )
    {
         advanced_tree = new PrefsTree( p_intf, tree_panel );
         connect( advanced_tree,
          SIGNAL( currentItemChanged( QTreeWidgetItem *, QTreeWidgetItem *) ),
          this, SLOT( changePanel( QTreeWidgetItem * ) ) );
    }
    tree_panel_l->addWidget( advanced_tree );
    advanced_tree->show();

    if( simple_panel )
    {
        main_panel_l->removeWidget( simple_panel );
        simple_panel->hide();
    }
    if( !advanced_panel )
         advanced_panel = new PrefsPanel( main_panel );
    main_panel_l->addWidget( advanced_panel );
    advanced_panel->show();
}

void PrefsDialog::setSmall()
{
    if( advanced_tree )
    {
        tree_panel_l->removeWidget( advanced_tree );
        advanced_tree->hide();
    }
    if( !simple_tree )
         simple_tree = new QTreeWidget();
    tree_panel_l->addWidget( simple_tree );
    simple_tree->show();

    if( advanced_panel )
    {
        main_panel_l->removeWidget( advanced_panel );
        advanced_panel->hide();
    }
    if( !simple_panel )
        simple_panel = new QWidget();
    main_panel_l->addWidget( simple_panel );
    simple_panel->show();
}

PrefsDialog::~PrefsDialog()
{
}

void PrefsDialog::changePanel( QTreeWidgetItem *item )
{
    PrefsItemData *data = item->data( 0, Qt::UserRole ).value<PrefsItemData*>();

    if( advanced_panel )
    {
        main_panel_l->removeWidget( advanced_panel );
        advanced_panel->hide();
    }
    if( !data->panel )
    {
        data->panel = new PrefsPanel( p_intf, main_panel , data,
                                      adv_chk->isChecked() );
    }
    advanced_panel = data->panel;
    main_panel_l->addWidget( advanced_panel );
    advanced_panel->show();
    setAdvanced( adv_chk->isChecked() );
}
