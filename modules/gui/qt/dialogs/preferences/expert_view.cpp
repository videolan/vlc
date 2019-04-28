/*****************************************************************************
 * expert_view.cpp : Detailed preferences overview - view
 *****************************************************************************
 * Copyright (C) 2019-2022 VLC authors and VideoLAN
 *
 * Authors: Lyndon Brown <jnqnfe@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QString>
#include <QFont>
#include <QGuiApplication>
#include <QClipboard>
#include <QMenu>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QAction>
#include <QContextMenuEvent>

#include "expert_view.hpp"
#include "preferences_widgets.hpp"

#include <vlc_config_cat.h>
#include <vlc_modules.h>

#define COLUMN_COUNT 4
/*********************************************************************
 * The Table
 *********************************************************************/
ExpertPrefsTable::ExpertPrefsTable( QWidget *parent ) :
    QTreeView( parent )
{
    setSelectionBehavior( QAbstractItemView::SelectRows );
    setSelectionMode( QAbstractItemView::SingleSelection );
    setAlternatingRowColors( true );

    setStyleSheet( "QTreeView::item { padding: 9px 0; }" );

    /* edit sub-dialog (reusable) */
    expert_edit = new ExpertPrefsEditDialog( this );

    connect( this, &QAbstractItemView::doubleClicked, this, &ExpertPrefsTable::doubleClicked );

    /* context menu actions */
    reset_action = new QAction( qtr( "&Reset" ), this );
    toggle_action = new QAction( qtr( "&Toggle" ), this );
    modify_action = new QAction( qtr( "&Modify" ), this );
    copy_name_action = new QAction( qtr( "Copy &name" ), this );
    copy_value_action = new QAction( qtr( "Copy &value" ), this );

    connect( reset_action, &QAction::triggered, this, &ExpertPrefsTable::resetItem );
    connect( toggle_action, &QAction::triggered, this, QOverload<>::of(&ExpertPrefsTable::toggleItem) );
    connect( modify_action, &QAction::triggered, this, QOverload<>::of(&ExpertPrefsTable::modifyItem) );
    connect( copy_name_action, &QAction::triggered, this, &ExpertPrefsTable::copyItemName );
    connect( copy_value_action, &QAction::triggered, this, &ExpertPrefsTable::copyItemValue );
}

void ExpertPrefsTable::applyAll()
{
    model()->submit();
}

/* apply filter on tree */
void ExpertPrefsTable::filter( const QString &text, bool modified_only )
{
    bool text_nonempty = !text.isEmpty();

    ExpertPrefsTableModel *model = myModel();
    for( int i = 0 ; i < model->rowCount(); i++ )
    {
        ExpertPrefsTableItem *item = model->itemAt( i );
        bool hide = ( ( modified_only && item->matchesDefault() ) ||
                      ( text_nonempty && !item->contains( text, Qt::CaseInsensitive ) ) );
        setRowHidden( i, QModelIndex(), hide );
    }
}

#ifndef QT_NO_CONTEXTMENU
void ExpertPrefsTable::contextMenuEvent( QContextMenuEvent *event )
{
    QModelIndex index = currentIndex();
    if( !index.isValid() || isRowHidden( index.row(), QModelIndex() ) )
        return;
    /* Avoid menu from right-click on empty space after last item */
    if( event->reason() == QContextMenuEvent::Mouse &&
        !indexAt( viewport()->mapFromGlobal( event->globalPos() ) ).isValid() )
        return;

    ExpertPrefsTableItem *item = myModel()->itemAt( index );

    QMenu *menu = new QMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if( CONFIG_CLASS( item->getType() ) == CONFIG_ITEM_BOOL )
        menu->addAction( toggle_action );
    else
        menu->addAction( modify_action );
    menu->addSeparator();
    menu->addAction( copy_name_action );
    menu->addAction( copy_value_action );
    copy_value_action->setEnabled( item->getType() != CONFIG_ITEM_PASSWORD );
    menu->addSeparator();
    menu->addAction( reset_action );
    reset_action->setEnabled( !item->matchesDefault() );

    menu->popup( event->globalPos() );
}
#endif // QT_NO_CONTEXTMENU

void ExpertPrefsTable::resetItem()
{
    QModelIndex index = currentIndex();
    if( !index.isValid() )
        return;
    myModel()->setItemToDefault( index );
}

void ExpertPrefsTable::toggleItem()
{
    toggleItem( currentIndex() );
}

/* this obviously only applies to boolean options! */
void ExpertPrefsTable::toggleItem( const QModelIndex &index )
{
    if( !index.isValid() )
        return;
    myModel()->toggleBoolean( index );
}

void ExpertPrefsTable::modifyItem()
{
    QModelIndex index = currentIndex();
    if( !index.isValid() )
        return;
    modifyItem( index );
}

void ExpertPrefsTable::modifyItem( const QModelIndex &index )
{
    ExpertPrefsTableItem *item = myModel()->itemAt( index );
    module_config_t *cfg_item = item->getConfig();
    /* For colour items it's much cleaner here to directly show a `QColorDialog`
       than provide indirect access to one via an `ExpertPrefsEditDialog` with a
       `ColorConfigControl`. */
    if( item->getType() == CONFIG_ITEM_RGB )
    {
        QColor color = QColorDialog::getColor( QColor( cfg_item->value.i ) );
        if( color.isValid() )
        {
            cfg_item->value.i = (color.red() << 16) + (color.green() << 8) + color.blue();
            item->updateMatchesDefault();
            item->updateValueDisplayString();
            myModel()->notifyUpdatedRow( index.row() );
        }
    }
    else
    {
        ConfigControl *control = ConfigControl::createControl( cfg_item, nullptr );
        expert_edit->setControl( control, item );
        expert_edit->exec();
    }
}

void ExpertPrefsTable::copyItemName()
{
    QModelIndex index = currentIndex();
    if( !index.isValid() )
        return;

    QClipboard *clipboard = QGuiApplication::clipboard();

    QModelIndex name_index = myModel()->index( index.row(), ExpertPrefsTableModel::NameField );
    clipboard->setText( name_index.data( Qt::DisplayRole ).toString() );
}

void ExpertPrefsTable::copyItemValue()
{
    QModelIndex index = currentIndex();
    if( !index.isValid() )
        return;

    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText( index.data( ExpertPrefsTableModel::CopyValueRole ).toString() );
}

void ExpertPrefsTable::doubleClicked( const QModelIndex &index )
{
    if( index.data( ExpertPrefsTableModel::TypeClassRole ).toInt() == CONFIG_ITEM_BOOL )
    {
        toggleItem( index );
        myModel()->notifyUpdatedRow( index.row() );
    }
    else
        modifyItem( index );
}

/*********************************************************************
 * The Edit Dialog
 *********************************************************************/
ExpertPrefsEditDialog::ExpertPrefsEditDialog( ExpertPrefsTable *_table ) :
    QDialog( _table ), table( _table )
{
    table_item = nullptr;
    control = nullptr;
    control_widget = nullptr;

    setWindowTitle( qtr( "Set option value" ) );
    setWindowRole( "vlc-preferences" );
    setWindowModality( Qt::WindowModal );

    setMinimumSize( 380, 110 );

    layout = new QVBoxLayout( this );

    QDialogButtonBox *buttonBox = new QDialogButtonBox();
    QPushButton *ok = new QPushButton( qtr( "&Ok" ) );
    QPushButton *cancel = new QPushButton( qtr( "&Cancel" ) );
    buttonBox->addButton( ok, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( cancel, QDialogButtonBox::RejectRole );
    layout->addWidget( buttonBox );

    connect( buttonBox, &QDialogButtonBox::accepted, this, &ExpertPrefsEditDialog::accept );
    connect( buttonBox, &QDialogButtonBox::rejected, this, &ExpertPrefsEditDialog::reject );

    setLayout( layout );
}

void ExpertPrefsEditDialog::setControl( ConfigControl *control_, ExpertPrefsTableItem *table_item_ )
{
    table_item = table_item_;
    control = control_;
    control_widget = new QWidget( this );
    control_widget->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    QVBoxLayout *control_layout = new QVBoxLayout( control_widget );
    control->insertInto( control_layout );
    layout->insertWidget( 0, control_widget );
}

void ExpertPrefsEditDialog::clearControl()
{
    delete control;
    delete control_widget;
    control = nullptr;
    control_widget = nullptr;
    table_item = nullptr;
}

void ExpertPrefsEditDialog::accept()
{
    control->storeValue();
    table_item->updateMatchesDefault();
    table_item->updateValueDisplayString();
    clearControl();
    QDialog::accept();
}

void ExpertPrefsEditDialog::reject()
{
    clearControl();
    QDialog::reject();
}
