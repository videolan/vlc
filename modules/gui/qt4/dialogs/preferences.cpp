/*****************************************************************************
 * preferences.cpp : Preferences
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include "dialogs/preferences.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"

#include "components/complete_preferences.hpp"
#include "components/simple_preferences.hpp"
#include "qt4.hpp"

#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QMessageBox>
#include <QDialogButtonBox>

PrefsDialog *PrefsDialog::instance = NULL;

PrefsDialog::PrefsDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    QGridLayout *main_layout = new QGridLayout( this );
    setWindowTitle( qtr( "Preferences" ) );
    resize( 750, 550 );

    /* Create Panels */
    tree_panel = new QWidget( 0 );
    tree_panel_l = new QHBoxLayout;
    tree_panel->setLayout( tree_panel_l );
    main_panel = new QWidget( 0 );
    main_panel_l = new QHBoxLayout;
    main_panel->setLayout( main_panel_l );

    /* Choice for types */
    types = new QGroupBox( "Show settings" );
    types->setAlignment( Qt::AlignHCenter );
    QHBoxLayout *types_l = new QHBoxLayout(0);
    types_l->setSpacing( 3 ); types_l->setMargin( 3 );
    small = new QRadioButton( qtr("Basic"), types );
    types_l->addWidget( small );
    all = new QRadioButton( qtr("All"), types ); types_l->addWidget( all );
    types->setLayout( types_l );
    small->setChecked( true );

    /* Tree and panel initialisations */
    advanced_tree = NULL;
    simple_tree = NULL;
    simple_panel = NULL;
    advanced_panel = NULL;

    /* Buttons */
    QDialogButtonBox *buttonsBox = new QDialogButtonBox();
    QPushButton *save = new QPushButton( qtr( "&Save" ) );
    QPushButton *cancel = new QPushButton( qtr( "&Cancel" ) );
    QPushButton *reset = new QPushButton( qtr( "&Reset Preferences" ) );

    buttonsBox->addButton( save, QDialogButtonBox::AcceptRole );
    buttonsBox->addButton( cancel, QDialogButtonBox::RejectRole );
    buttonsBox->addButton( reset, QDialogButtonBox::ActionRole );

    /* Layout  */
    main_layout->addWidget( tree_panel, 0, 0, 3, 1 );
    main_layout->addWidget( types, 3, 0, 2, 1 );
    main_layout->addWidget( main_panel, 0, 1, 4, 1 );
    main_layout->addWidget( buttonsBox, 4, 1, 1 ,2 );

    main_layout->setColumnMinimumWidth( 0, 150 );
    main_layout->setColumnStretch( 0, 1 );
    main_layout->setColumnStretch( 1, 3 );

    main_layout->setRowStretch( 2, 4);

    setLayout( main_layout );

    /* Margins */
    tree_panel_l->setMargin( 1 );
    main_panel_l->setMargin( 3 );

    if( config_GetInt( p_intf, "qt-advanced-pref") == 1 )
    {
        setAll();
    }
    else
    {
        setSmall();
    }

    BUTTONACT( save, save() );
    BUTTONACT( cancel, cancel() );
    BUTTONACT( reset, reset() );
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
    all->setChecked( true );
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
                  currentItemChanged( int ),
                  this,  changeSimplePanel( int ) );
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
    small->setChecked( true );
    simple_panel->show();
}

void PrefsDialog::changeSimplePanel( int number )
{
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
                PrefsItemData *mod_data = module_item->data( 0, Qt::UserRole ).
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
    apply();
    hide();
}

void PrefsDialog::apply()
{
    if( small->isChecked() && simple_tree )
    {
        for( int i = 0 ; i< SPrefsMax; i++ )
            if( simple_panels[i] ) simple_panels[i]->apply();
    }
    else if( all->isChecked() && advanced_tree )
        advanced_tree->applyAll();
    config_SaveConfigFile( p_intf, NULL );

    /* Delete the other panel in order to force its reload after clicking
       on apply - UGLY but will work for now. */
    if( simple_panel && simple_panel->isVisible() && advanced_panel )
    {
        delete advanced_panel;
        advanced_panel = NULL;
    }
    if( advanced_panel && advanced_panel->isVisible() && simple_panel )
    {
        delete simple_panel;
        simple_panel = NULL;
    }
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

void PrefsDialog::reset()
{
    int ret = QMessageBox::question(this, qtr("Reset Preferences"),
                 qtr("This will reset your VLC media player preferences.\n"
                         "Are you sure you want to continue?"),
                  QMessageBox::Ok | QMessageBox::Cancel,
                                                         QMessageBox::Ok);
    if ( ret == QMessageBox::Ok )
    {
        config_ResetAll( p_intf );
        config_SaveConfigFile( p_intf, NULL );
    }
}
