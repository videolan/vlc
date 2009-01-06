/*****************************************************************************
 * Messages.cpp : Information about an item
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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

#include "dialogs/messages.hpp"

#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QMutex>

MessagesDialog *MessagesDialog::instance = NULL;

struct msg_cb_data_t
{
    MessagesDialog *self;
    QMutex          lock; /**< protects MessagesDialog::messages */
};

MessagesDialog::MessagesDialog( intf_thread_t *_p_intf)
               : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Messages" ) );

    /* General widgets */
    QGridLayout *mainLayout = new QGridLayout( this );
    mainTab = new QTabWidget( this );
    mainTab->setTabPosition( QTabWidget::North );


    /* Messages */
    QWidget     *msgWidget = new QWidget;
    QGridLayout *msgLayout = new QGridLayout( msgWidget );

    messages = new QTextEdit();
    messages->setReadOnly( true );
    messages->setGeometry( 0, 0, 440, 600 );
    messages->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

    msgLayout->addWidget( messages, 0, 0, 1, 0 );
    mainTab->addTab( msgWidget, qtr( "Messages" ) );


    /* Modules tree */
    QWidget     *treeWidget = new QWidget;
    QGridLayout *treeLayout = new QGridLayout( treeWidget );

    modulesTree = new QTreeWidget();
    modulesTree->header()->hide();

    treeLayout->addWidget( modulesTree, 0, 0, 1, 0 );
    mainTab->addTab( treeWidget, qtr( "Modules tree" ) );


    /* Buttons and general layout */
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    closeButton->setDefault( true );
    clearUpdateButton = new QPushButton( qtr( "C&lear" ) );
    saveLogButton = new QPushButton( qtr( "&Save as..." ) );
    saveLogButton->setToolTip( qtr( "Saves all the displayed logs to a file" ) );

    verbosityBox = new QSpinBox();
    verbosityBox->setRange( 0, 2 );
    verbosityBox->setValue( config_GetInt( p_intf, "verbose" ) );
    verbosityBox->setWrapping( true );
    verbosityBox->setMaximumWidth( 50 );

    verbosityLabel = new QLabel( qtr( "Verbosity Level" ) );

    mainLayout->addWidget( mainTab, 0, 0, 1, 0 );
    mainLayout->addWidget( verbosityLabel, 1, 0, 1, 1 );
    mainLayout->addWidget( verbosityBox, 1, 1 );
    mainLayout->setColumnStretch( 2, 10 );
    mainLayout->addWidget( saveLogButton, 1, 3 );
    mainLayout->addWidget( clearUpdateButton, 1, 4 );
    mainLayout->addWidget( closeButton, 1, 5 );

    BUTTONACT( closeButton, hide() );
    BUTTONACT( clearUpdateButton, clearOrUpdate() );
    BUTTONACT( saveLogButton, save() );
    CONNECT( mainTab, currentChanged( int ),
             this, updateTab( int ) );

    /* General action */
    readSettings( "Messages", QSize( 600, 450 ) );


    /* Hook up to LibVLC messaging */
    cb_data = new msg_cb_data_t;
    cb_data->self = this;
    sub = msg_Subscribe (_p_intf->p_libvlc, sinkMessage, cb_data);
}

MessagesDialog::~MessagesDialog ()
{
    writeSettings( "Messages" );
    msg_Unsubscribe (sub);
    delete cb_data;
};

void MessagesDialog::updateTab( int index )
{
    /* Second tab : modules tree */
    if( index == 1 )
    {
        verbosityLabel->hide();
        verbosityBox->hide();
        clearUpdateButton->setText( qtr( "&Update" ) );
        saveLogButton->hide();
        updateTree();
    }
    /* First tab : messages */
    else
    {
        verbosityLabel->show();
        verbosityBox->show();
        clearUpdateButton->setText( qtr( "&Clear" ) );
        saveLogButton->show();
    }
}

void MessagesDialog::sinkMessage (msg_cb_data_t *data, msg_item_t *item,
                                  unsigned overruns)
{
    MessagesDialog *self = data->self;
    int canc = vlc_savecancel ();
    QMutexLocker locker (&data->lock);

    self->sinkMessage (item, overruns);
    vlc_restorecancel (canc);
}

void MessagesDialog::sinkMessage (msg_item_t *item, unsigned)
{
    if ((item->i_type == VLC_MSG_WARN && verbosityBox->value() < 1)
     || (item->i_type == VLC_MSG_DBG && verbosityBox->value() < 2 ))
        return;

    // Saving cursor selection
    int startPos = messages->textCursor().selectionStart();
    int endPos = messages->textCursor().selectionEnd();

    messages->moveCursor( QTextCursor::End );
    messages->setFontItalic( true );
    messages->setTextColor( "darkBlue" );
    messages->insertPlainText( qfu( item->psz_module ) );

    switch (item->i_type)
    {
        case VLC_MSG_INFO:
            messages->setTextColor( "blue" );
            messages->insertPlainText( " info: " );
            break;
        case VLC_MSG_ERR:
            messages->setTextColor( "red" );
            messages->insertPlainText( " error: " );
            break;
        case VLC_MSG_WARN:
            messages->setTextColor( "green" );
            messages->insertPlainText( " warning: " );
            break;
        case VLC_MSG_DBG:
        default:
            messages->setTextColor( "grey" );
            messages->insertPlainText( " debug: " );
            break;
    }

    /* Add message Regular black Font */
    messages->setFontItalic( false );
    messages->setTextColor( "black" );
    messages->insertPlainText( qfu(item->psz_msg) );
    messages->insertPlainText( "\n" );
    messages->ensureCursorVisible();

    // Restoring saved cursor selection
    QTextCursor cur = messages->textCursor();
    cur.movePosition( QTextCursor::Start );
    cur.movePosition( QTextCursor::NextCharacter, QTextCursor::MoveAnchor, startPos );
    cur.movePosition( QTextCursor::NextCharacter, QTextCursor::KeepAnchor, endPos - startPos );
    messages->setTextCursor( cur );
}

void MessagesDialog::buildTree( QTreeWidgetItem *parentItem,
                                vlc_object_t *p_obj )
{
    QTreeWidgetItem *item;

    if( parentItem )
        item = new QTreeWidgetItem( parentItem );
    else
        item = new QTreeWidgetItem( modulesTree );

    if( p_obj->psz_object_name )
        item->setText( 0, qfu( p_obj->psz_object_type ) + " \"" +
                       qfu( p_obj->psz_object_name ) + "\" (" +
                       QString::number((uintptr_t)p_obj) + ")" );
    else
        item->setText( 0, qfu( p_obj->psz_object_type ) + " (" +
                       QString::number((uintptr_t)p_obj) + ")" );

    item->setExpanded( true );

    vlc_list_t *l = vlc_list_children( p_obj );
    for( int i=0; i < l->i_count; i++ )
        buildTree( item, l->p_values[i].p_object );
    vlc_list_release( l );
}

void MessagesDialog::clearOrUpdate()
{
    if( mainTab->currentIndex() )
        updateTree();
    else
        clear();
}

void MessagesDialog::updateTree()
{
    modulesTree->clear();
    buildTree( NULL, VLC_OBJECT( p_intf->p_libvlc ) );
}

void MessagesDialog::clear()
{
    QMutexLocker locker (&cb_data->lock);
    messages->clear();
}

bool MessagesDialog::save()
{
    QString saveLogFileName = QFileDialog::getSaveFileName(
            this, qtr( "Save log file as..." ),
            qfu( config_GetHomeDir() ),
            qtr( "Texts / Logs (*.log *.txt);; All (*.*) ") );

    if( !saveLogFileName.isNull() )
    {
        QFile file( saveLogFileName );
        if ( !file.open( QFile::WriteOnly | QFile::Text ) ) {
            QMessageBox::warning( this, qtr( "Application" ),
                    qtr( "Cannot write to file %1:\n%2." )
                    .arg( saveLogFileName )
                    .arg( file.errorString() ) );
            return false;
        }

        QTextStream out( &file );
        QMutexLocker locker (&cb_data->lock);
        out << messages->toPlainText() << "\n";

        return true;
    }
    return false;
}
