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
#include "components/simple_preferences.hpp"
#include "qt4.hpp"

#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>

PrefsDialog *PrefsDialog::instance = NULL;

PrefsDialog::PrefsDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
     QGridLayout *main_layout = new QGridLayout(this);
     setWindowTitle( qtr( "Preferences" ) );
     resize( 800, 600 );
     setMaximumHeight (600);

     tree_panel = new QWidget(0);
     tree_panel_l = new QHBoxLayout;
     tree_panel->setLayout( tree_panel_l );
     main_panel = new QWidget(0);
     main_panel_l = new QHBoxLayout;
     main_panel->setLayout( main_panel_l );

     // Choice for types
     types = new QGroupBox( "Show settings" );
     QHBoxLayout *types_l = new QHBoxLayout(0);
     types_l->setSpacing( 3 ); types_l->setMargin( 3 );
     small = new QRadioButton( "Basic", types ); types_l->addWidget( small );
     all = new QRadioButton( "All", types ); types_l->addWidget( all );
     types->setLayout(types_l);
     small->setChecked( true );

     advanced_tree = NULL;
     simple_tree = NULL;
     simple_panel = NULL;
     advanced_panel = NULL;

     main_layout->addWidget( tree_panel, 0, 0, 3, 1 );
     main_layout->addWidget( types, 3, 0, 1, 1 );

     main_layout->addWidget( main_panel, 0, 1, 4, 1 );

     main_layout->setColumnMinimumWidth( 0, 200 );
     main_layout->setColumnStretch( 0, 1 );
     main_layout->setColumnStretch( 1,3 );

     main_layout->setRowStretch( 2, 4);

     setSmall();

     QPushButton *save, *cancel;
     QHBoxLayout *buttonsLayout = QVLCFrame::doButtons( this, NULL,
                                                        &save, _("Save"),
                                                        &cancel, _("Cancel"),
                                                        NULL, NULL );
     main_layout->addLayout( buttonsLayout, 4, 0, 1 ,3 );
     setLayout( main_layout );


     BUTTONACT( save, save() );
     BUTTONACT( cancel, cancel() );
     BUTTONACT( small, setSmall() );
     BUTTONACT( all, setAll() );

     for( int i = 0; i < SPrefsMax ; i++ ) simple_panels[i] = NULL;
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
         CONNECT( advanced_tree,
                  currentItemChanged( QTreeWidgetItem *, QTreeWidgetItem *),
                  this, changePanel( QTreeWidgetItem * ) );
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
    {
         simple_tree = new SPrefsCatList( p_intf, tree_panel );
         CONNECT( simple_tree,
                  currentItemChanged( QListWidgetItem *, QListWidgetItem *),
                  this,  changeSimplePanel( QListWidgetItem * ) );
    }
    tree_panel_l->addWidget( simple_tree );
    simple_tree->show();

    if( advanced_panel )
    {
        main_panel_l->removeWidget( advanced_panel );
        advanced_panel->hide();
    }
    if( !simple_panel )
        simple_panel = new SPrefsPanel( p_intf, main_panel, SPrefsDefaultCat );
    main_panel_l->addWidget( simple_panel );
    simple_panel->show();
}

void PrefsDialog::changeSimplePanel( QListWidgetItem *item )
{
    int number = item->data( Qt::UserRole ).toInt();
    if( simple_panel )
    {
        main_panel_l->removeWidget( simple_panel );
        simple_panel->hide();
    }
    simple_panel = simple_panels[number];
    if( !simple_panel )
    {
        simple_panel = new SPrefsPanel( p_intf, main_panel, number );
        simple_panels[number] = simple_panel;
    }
    main_panel_l->addWidget( simple_panel );
    simple_panel->show();
//    panel_label->setText(qtr("Test"));
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
        data->panel = new PrefsPanel( p_intf, main_panel , data );

    advanced_panel = data->panel;
    main_panel_l->addWidget( advanced_panel );
    advanced_panel->show();
}

void PrefsDialog::showModulePrefs( char *psz_module )
{
    setAll();
    all->setChecked( true );
    for( int i_cat_index = 0 ; i_cat_index < advanced_tree->topLevelItemCount();
         i_cat_index++ )
    {
        QTreeWidgetItem *cat_item = advanced_tree->topLevelItem( i_cat_index );
        PrefsItemData *data = cat_item->data( 0, Qt::UserRole ).
                                                   value<PrefsItemData *>();
        for( int i_sc_index = 0; i_sc_index < cat_item->childCount();
                                  i_sc_index++ )
        {
            QTreeWidgetItem *subcat_item = cat_item->child( i_sc_index );
            PrefsItemData *sc_data = subcat_item->data(0, Qt::UserRole).
                                                    value<PrefsItemData *>();
            for( int i_module = 0; i_module < subcat_item->childCount();
                                   i_module++ )
            {
                QTreeWidgetItem *module_item = subcat_item->child( i_module );
                PrefsItemData *mod_data = module_item->data(0, Qt::UserRole).
                                                    value<PrefsItemData *>();
                if( !strcmp( mod_data->psz_name, psz_module ) ) {
                    advanced_tree->setCurrentItem( module_item );
                }
            }
        }
    }
    show();
}

void PrefsDialog::save()
{
    if( small->isChecked() && simple_tree )
    {
        for( int i = 0 ; i< SPrefsMax; i++ )
            if( simple_panels[i] ) simple_panels[i]->apply();
    }
    else if( all->isChecked() && advanced_tree )
        advanced_tree->applyAll();
    config_SaveConfigFile( p_intf, NULL );
    hide();
}

void PrefsDialog::cancel()
{
    if( small->isChecked() && simple_tree )
    {
        for( int i = 0 ; i< SPrefsMax; i++ )
            if( simple_panels[i] ) simple_panels[i]->clean();
    }
    else if( all->isChecked() && advanced_tree )
    {
        advanced_tree->cleanAll();
        advanced_panel = NULL;
    }
    hide();
}
