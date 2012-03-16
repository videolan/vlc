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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/preferences.hpp"
#include "util/qvlcframe.hpp"
#include "dialogs/errors.hpp"

#include "components/complete_preferences.hpp"
#include "components/simple_preferences.hpp"
#include "util/searchlineedit.hpp"
#include "main_interface.hpp"

#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QPushButton>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QStackedWidget>

PrefsDialog::PrefsDialog( QWidget *parent, intf_thread_t *_p_intf )
            : QVLCDialog( parent, _p_intf )
{
    QGridLayout *main_layout = new QGridLayout( this );
    setWindowTitle( qtr( "Preferences" ) );
    setWindowRole( "vlc-preferences" );
    setWindowModality( Qt::WindowModal );

    /* Whether we want it or not, we need to destroy on close to get
       consistency when reset */
    setAttribute( Qt::WA_DeleteOnClose );

    /* Create Panels */
    simple_tree_panel = new QWidget;
    simple_tree_panel->setLayout( new QVBoxLayout );

    advanced_tree_panel = new QWidget;
    advanced_tree_panel->setLayout( new QVBoxLayout );

    advanced_main_panel = new QWidget;
    advanced_main_panel->setLayout( new QHBoxLayout );

    /* Choice for types */
    types = new QGroupBox( qtr("Show settings") );
    types->setAlignment( Qt::AlignHCenter );
    QHBoxLayout *types_l = new QHBoxLayout;
    types_l->setSpacing( 3 ); types_l->setMargin( 3 );
    small = new QRadioButton( qtr( "Simple" ), types );
    small->setToolTip( qtr( "Switch to simple preferences view" ) );
    types_l->addWidget( small );
    all = new QRadioButton( qtr("All"), types ); types_l->addWidget( all );
    all->setToolTip( qtr( "Switch to full preferences view" ) );
    types->setLayout( types_l );
    small->setChecked( true );

    /* Tree and panel initialisations */
    advanced_tree = NULL;
    tree_filter = NULL;
    simple_tree = NULL;
    simple_panels_stack = new QStackedWidget;
    advanced_panel = NULL;

    /* Buttons */
    QDialogButtonBox *buttonsBox = new QDialogButtonBox();
    QPushButton *save = new QPushButton( qtr( "&Save" ) );
    save->setToolTip( qtr( "Save and close the dialog" ) );
    QPushButton *cancel = new QPushButton( qtr( "&Cancel" ) );
    QPushButton *reset = new QPushButton( qtr( "&Reset Preferences" ) );

    buttonsBox->addButton( save, QDialogButtonBox::AcceptRole );
    buttonsBox->addButton( cancel, QDialogButtonBox::RejectRole );
    buttonsBox->addButton( reset, QDialogButtonBox::ResetRole );


    simple_split_widget = new QWidget();
    simple_split_widget->setLayout( new QHBoxLayout );

    advanced_split_widget = new QWidget();
    advanced_split_widget->setLayout( new QHBoxLayout );

    stack = new QStackedWidget();
    stack->insertWidget( SIMPLE, simple_split_widget );
    stack->insertWidget( ADVANCED, advanced_split_widget );

    simple_split_widget->layout()->addWidget( simple_tree_panel );
    simple_split_widget->layout()->addWidget( simple_panels_stack );
    simple_split_widget->layout()->setMargin( 0 );

    advanced_split_widget->layout()->addWidget( advanced_tree_panel );
    advanced_split_widget->layout()->addWidget( advanced_main_panel );
    advanced_split_widget->layout()->setMargin( 0 );

    /* Layout  */
    main_layout->addWidget( stack, 0, 0, 3, 3 );
    main_layout->addWidget( types, 3, 0, 2, 1 );
    main_layout->addWidget( buttonsBox, 4, 2, 1 ,1 );
    main_layout->setRowStretch( 2, 4 );
    main_layout->setMargin( 9 );
    setLayout( main_layout );

    /* Margins */
    simple_tree_panel->layout()->setMargin( 1 );
    simple_panels_stack->layout()->setContentsMargins( 6, 0, 0, 3 );

    b_small = (p_intf->p_sys->i_screenHeight < 750);
    if( b_small ) msg_Dbg( p_intf, "Small Resolution");
    setMaximumHeight( p_intf->p_sys->i_screenHeight );
    for( int i = 0; i < SPrefsMax ; i++ ) simple_panels[i] = NULL;

    if( var_InheritBool( p_intf, "qt-advanced-pref" )
     || var_InheritBool( p_intf, "advanced" ) )
        setAdvanced();
    else
        setSmall();

    BUTTONACT( save, save() );
    BUTTONACT( cancel, cancel() );
    BUTTONACT( reset, reset() );

    BUTTONACT( small, setSmall() );
    BUTTONACT( all, setAdvanced() );

    resize( 780, sizeHint().height() );
}

void PrefsDialog::setAdvanced()
{
    if ( !tree_filter )
    {
        tree_filter = new SearchLineEdit( simple_tree_panel );
        tree_filter->setMinimumHeight( 26 );

        CONNECT( tree_filter, textChanged( const QString &  ),
                this, advancedTreeFilterChanged( const QString & ) );

        advanced_tree_panel->layout()->addWidget( tree_filter );
    }

    /* If don't have already and advanced TREE, then create it */
    if( !advanced_tree )
    {
        /* Creation */
         advanced_tree = new PrefsTree( p_intf, simple_tree_panel );
        /* and connections */
         CONNECT( advanced_tree,
                  currentItemChanged( QTreeWidgetItem *, QTreeWidgetItem * ),
                  this, changeAdvPanel( QTreeWidgetItem * ) );
        advanced_tree_panel->layout()->addWidget( advanced_tree );
        advanced_tree_panel->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );
    }

    /* If no advanced Panel exist, create one, attach it and show it*/
    if( !advanced_panel )
    {
        advanced_panel = new AdvPrefsPanel( advanced_main_panel );
        advanced_main_panel->layout()->addWidget( advanced_panel );
    }

    /* Select the first Item of the preferences. Maybe you want to select a specified
       category... */
    advanced_tree->setCurrentIndex(
            advanced_tree->model()->index( 0, 0, QModelIndex() ) );

    all->setChecked( true );
    stack->setCurrentIndex( ADVANCED );
}

void PrefsDialog::setSmall()
{
    /* If no simple_tree, create one, connect it */
    if( !simple_tree )
    {
         simple_tree = new SPrefsCatList( p_intf, simple_tree_panel, b_small );
         CONNECT( simple_tree,
                  currentItemChanged( int ),
                  this,  changeSimplePanel( int ) );
        simple_tree_panel->layout()->addWidget( simple_tree );
        simple_tree_panel->setMinimumSize( QSize( 150, 0 ) );
        simple_tree_panel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );
    }

    if( ! simple_panels[SPrefsDefaultCat] )
        changeSimplePanel( SPrefsDefaultCat );

    small->setChecked( true );
    stack->setCurrentIndex( SIMPLE );
}

/* Switching from on simple panel to another */
void PrefsDialog::changeSimplePanel( int number )
{
    if( ! simple_panels[number] )
    {
        SPrefsPanel *insert = new SPrefsPanel( p_intf, simple_panels_stack, number, b_small ) ;
        simple_panels_stack->insertWidget( number, insert );
        simple_panels[number] = insert;
    }
    simple_panels_stack->setCurrentWidget( simple_panels[number] );
}

/* Changing from one Advanced Panel to another */
void PrefsDialog::changeAdvPanel( QTreeWidgetItem *item )
{
    if( item == NULL ) return;
    PrefsItemData *data = item->data( 0, Qt::UserRole ).value<PrefsItemData*>();

    if( advanced_panel )
        if( advanced_panel->isVisible() ) advanced_panel->hide();

    if( !data->panel )
    {
        data->panel = new AdvPrefsPanel( p_intf, advanced_main_panel, data );
        advanced_main_panel->layout()->addWidget( data->panel );
    }

    advanced_panel = data->panel;
    advanced_panel->show();
}

#if 0
/*Called from extended settings, is not used anymore, but could be useful one day*/
void PrefsDialog::showModulePrefs( char *psz_module )
{
    setAdvanced();
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
#endif

/* Actual apply and save for the preferences */
void PrefsDialog::save()
{
    if( small->isChecked() && simple_tree->isVisible() )
    {
        msg_Dbg( p_intf, "Saving the simple preferences" );
        for( int i = 0 ; i< SPrefsMax; i++ ){
            if( simple_panels_stack->widget(i) )
                qobject_cast<SPrefsPanel *>(simple_panels_stack->widget(i))->apply();
        }
    }
    else if( all->isChecked() && advanced_tree->isVisible() )
    {
        msg_Dbg( p_intf, "Saving the advanced preferences" );
        advanced_tree->applyAll();
    }

    /* Save to file */
    if( config_SaveConfigFile( p_intf ) != 0 )
    {
        ErrorsDialog::getInstance (p_intf)->addError( qtr( "Cannot save Configuration" ),
            qtr("Preferences file could not be saved") );
    }

    if( p_intf->p_sys->p_mi )
        p_intf->p_sys->p_mi->reloadPrefs();
    accept();
}

/* Clean the preferences, dunno if it does something really */
void PrefsDialog::cancel()
{
    reject();
}

/* Reset all the preferences, when you click the button */
void PrefsDialog::reset()
{
    int ret = QMessageBox::question(
                 this,
                 qtr( "Reset Preferences" ),
                 qtr( "Are you sure you want to reset your VLC media player preferences?" ),
                 QMessageBox::Ok | QMessageBox::Cancel,
                 QMessageBox::Ok);

    if( ret == QMessageBox::Ok )
    {
        config_ResetAll( p_intf );
        config_SaveConfigFile( p_intf );
        getSettings()->clear();

        accept();
    }
}

void PrefsDialog::advancedTreeFilterChanged( const QString & text )
{
    advanced_tree->filter( text );
}
