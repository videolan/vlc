/*****************************************************************************
 * messages.cpp : Information about an item
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextBlock>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMutex>
#include <QLineEdit>
#include <QScrollBar>
#include <QMutex>
#include <QMutexLocker>

#include <assert.h>

enum {
    MsgEvent_Type = QEvent::User + MsgEventTypeOffset + 1,
};

class MsgEvent : public QEvent
{
public:
    MsgEvent( int, const vlc_log_t *, const char * );

    int priority;
    uintptr_t object_id;
    QString object_type;
    QString header;
    QString module;
    QString text;
};

MsgEvent::MsgEvent( int type, const vlc_log_t *msg, const char *text )
    : QEvent( (QEvent::Type)MsgEvent_Type ),
      priority( type ),
      object_id( msg->i_object_id ),
      object_type( qfu(msg->psz_object_type) ),
      header( qfu(msg->psz_header) ),
      module( qfu(msg->psz_module) ),
      text( qfu(text) )
{
}

MessagesDialog::MessagesDialog( intf_thread_t *_p_intf)
               : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Messages" ) );
    setWindowRole( "vlc-messages" );
    /* Build Ui */
    ui.setupUi( this );
    ui.bottomButtonsBox->addButton( new QPushButton( qtr("&Close"), this ),
                                         QDialogButtonBox::RejectRole );

    /* Modules tree */
    ui.modulesTree->setHeaderHidden( true );

    /* Buttons and general layout */
    ui.saveLogButton->setToolTip( qtr( "Saves all the displayed logs to a file" ) );

    int i_verbosity = var_InheritInteger( p_intf, "verbose" );
    changeVerbosity( i_verbosity );
    ui.verbosityBox->setValue( qMin( i_verbosity, 2 ) );

    getSettings()->beginGroup( "Messages" );
    ui.filterEdit->setText( getSettings()->value( "messages-filter" ).toString() );
    getSettings()->endGroup();

    updateButton = new QPushButton( QIcon(":/update"), "" );
    updateButton->setFlat( true );
    ui.mainTab->setCornerWidget( updateButton );

#ifndef NDEBUG
    QWidget *pldebugTab = new QWidget();
    QVBoxLayout *pldebugTabLayout = new QVBoxLayout();
    pldebugTab->setLayout( pldebugTabLayout );
    ui.mainTab->addTab( pldebugTab, "Playlist Tree" );
    pldebugTree = new QTreeWidget();
    pldebugTree->headerItem()->setText( 0, "Name" );
    pldebugTree->headerItem()->setText( 1, "PL id" );
    pldebugTree->headerItem()->setText( 2, "Item id" );
    pldebugTree->headerItem()->setText( 3, "PL flags" );
    pldebugTree->headerItem()->setText( 4, "Item flags" );
    pldebugTree->setColumnCount( 5 );
    pldebugTabLayout->addWidget( pldebugTree );
#endif

    tabChanged(0);

    BUTTONACT( updateButton, updateOrClear() );
    BUTTONACT( ui.saveLogButton, save() );
    CONNECT( ui.filterEdit, editingFinished(), this, updateConfig() );
    CONNECT( ui.filterEdit, textChanged(QString), this, filterMessages() );
    CONNECT( ui.bottomButtonsBox, rejected(), this, hide() );
    CONNECT( ui.verbosityBox, valueChanged( int ),
             this, changeVerbosity( int ) );

    CONNECT( ui.mainTab, currentChanged( int ), this, tabChanged( int ) );

    /* General action */
    restoreWidgetPosition( "Messages", QSize( 600, 450 ) );

    /* Hook up to LibVLC messaging */
    vlc_LogSet( p_intf->p_libvlc, MsgCallback, this );

    buildTree( NULL, VLC_OBJECT( p_intf->p_libvlc ) );
}

MessagesDialog::~MessagesDialog()
{
    saveWidgetPosition( "Messages" );
    vlc_LogSet( p_intf->p_libvlc, NULL, NULL );
};

void MessagesDialog::changeVerbosity( int i_verbosity )
{
    vlc_atomic_set( &this->verbosity, i_verbosity );
}

void MessagesDialog::updateConfig()
{
    getSettings()->beginGroup( "Messages" );
    getSettings()->setValue( "messages-filter", ui.filterEdit->text() );
    getSettings()->endGroup();
}

void MessagesDialog::filterMessages()
{
    QMutexLocker locker( &messageLocker );
    QPlainTextEdit *messages = ui.messages;
    QTextBlock block = messages->document()->firstBlock();

    while( block.isValid() )
    {
        block.setVisible( matchFilter( block.text().toLower() ) );
        block = block.next();
    }

    /* Consider the whole QTextDocument as dirty now */
    messages->document()->markContentsDirty( 0, messages->document()->characterCount() );

    /* FIXME This solves a bug (Qt?) with the viewport not resizing the
       vertical scroll bar when one or more QTextBlock are hidden */
    QSize vsize = messages->viewport()->size();
    messages->viewport()->resize( vsize + QSize( 1, 1 ) );
    messages->viewport()->resize( vsize );
}

bool MessagesDialog::matchFilter( const QString& text )
{
    const QString& filter = ui.filterEdit->text();

    if( filter.isEmpty() || text.contains( filter.toLower() ) )
        return true;
    return false;
}

void MessagesDialog::sinkMessage( const MsgEvent *msg )
{
    QMutexLocker locker( &messageLocker );

    QPlainTextEdit *messages = ui.messages;
    /* Only scroll if the viewport is at the end.
       Don't bug user by auto-changing/losing viewport on insert(). */
    bool b_autoscroll = ( messages->verticalScrollBar()->value()
                          + messages->verticalScrollBar()->pageStep()
                          >= messages->verticalScrollBar()->maximum() );

    /* Copy selected text to the clipboard */
    if( messages->textCursor().hasSelection() )
        messages->copy();

    /* Fix selected text bug */
    if( !messages->textCursor().atEnd() ||
         messages->textCursor().anchor() != messages->textCursor().position() )
         messages->moveCursor( QTextCursor::End );

    /* Start a new logic block so we can hide it on-demand */
    messages->textCursor().insertBlock();

    QString buf = QString( "<i><font color='darkblue'>%1</font>" ).arg( msg->module );

    switch ( msg->priority )
    {
        case VLC_MSG_INFO:
            buf += "<font color='blue'> info: </font>";
            break;
        case VLC_MSG_ERR:
            buf += "<font color='red'> error: </font>";
            break;
        case VLC_MSG_WARN:
            buf += "<font color='green'> warning: </font>";
            break;
        case VLC_MSG_DBG:
        default:
            buf += "<font color='grey'> debug: </font>";
            break;
    }

    /* Insert the prefix */
    messages->textCursor().insertHtml( buf /* + "</i>" */ );

    /* Insert the message */
    messages->textCursor().insertHtml( msg->text );

    /* Pass the new message thru the filter */
    QTextBlock b = messages->document()->lastBlock();
    b.setVisible( matchFilter( b.text() ) );

    /* Tell the QTextDocument to recompute the size of the given area */
    messages->document()->markContentsDirty( b.position(), b.length() );

    if ( b_autoscroll ) messages->ensureCursorVisible();
}

void MessagesDialog::customEvent( QEvent *event )
{
    MsgEvent *msge = static_cast<MsgEvent *>(event);

    assert( msge );
    sinkMessage( msge );
}

bool MessagesDialog::save()
{
    QString saveLogFileName = QFileDialog::getSaveFileName(
            this, qtr( "Save log file as..." ),
            QVLCUserDir( VLC_DOCUMENTS_DIR ),
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

        QTextBlock block = ui.messages->document()->firstBlock();
        while( block.isValid() )
        {
            if( block.isVisible() )
                out << block.text() << "\n";

            block = block.next();
        }
        return true;
    }
    return false;
}

void MessagesDialog::buildTree( QTreeWidgetItem *parentItem,
                                vlc_object_t *p_obj )
{
    QTreeWidgetItem *item;

    if( parentItem )
        item = new QTreeWidgetItem( parentItem );
    else
        item = new QTreeWidgetItem( ui.modulesTree );

    char *name = vlc_object_get_name( p_obj );
    item->setText( 0, QString("%1%2 (0x%3)")
                   .arg( qfu( p_obj->psz_object_type ) )
                   .arg( ( name != NULL )
                         ? QString( " \"%1\"" ).arg( qfu( name ) )
                             : "" )
                   .arg( (uintptr_t)p_obj, 0, 16 )
                 );
    free( name );
    item->setExpanded( true );

    vlc_list_t *l = vlc_list_children( p_obj );
    for( int i=0; i < l->i_count; i++ )
        buildTree( item, l->p_values[i].p_object );
    vlc_list_release( l );
}

void MessagesDialog::updateOrClear()
{
    if( ui.mainTab->currentIndex() == 1)
    {
        ui.modulesTree->clear();
        buildTree( NULL, VLC_OBJECT( p_intf->p_libvlc ) );
    }
    else if( ui.mainTab->currentIndex() == 0 )
        ui.messages->clear();
#ifndef NDEBUG
    else
        updatePLTree();
#endif
}

void MessagesDialog::tabChanged( int i )
{
    updateButton->setIcon( i != 0 ? QIcon(":/update") : QIcon(":/toolbar/clear") );
    updateButton->setToolTip( i != 0 ? qtr("Update the tree")
                                     : qtr("Clear the messages") );
}

void MessagesDialog::MsgCallback( void *self, int type, const vlc_log_t *item,
                                  const char *format, va_list ap )
{
    MessagesDialog *dialog = (MessagesDialog *)self;
    char *str;
    int verbosity = vlc_atomic_get( &dialog->verbosity );

    if( verbosity < 0 || verbosity < (type - VLC_MSG_ERR)
     || unlikely(vasprintf( &str, format, ap ) == -1) )
        return;

    int canc = vlc_savecancel();
    QApplication::postEvent( dialog, new MsgEvent( type, item, str ) );
    vlc_restorecancel( canc );
    free( str );
}

#ifndef NDEBUG
static QTreeWidgetItem * PLWalk( playlist_item_t *p_node )
{
    QTreeWidgetItem *current = new QTreeWidgetItem();
    current->setText( 0, qfu( p_node->p_input->psz_name ) );
    current->setToolTip( 0, qfu( p_node->p_input->psz_uri ) );
    current->setText( 1, QString("%1").arg( p_node->i_id ) );
    current->setText( 2, QString("%1").arg( p_node->p_input->i_id ) );
    current->setText( 3, QString("0x%1").arg( p_node->i_flags, 0, 16 ) );
    current->setText( 4, QString("0x%1").arg(  p_node->p_input->i_type, 0, 16 ) );
    for ( int i = 0; p_node->i_children > 0 && i < p_node->i_children; i++ )
        current->addChild( PLWalk( p_node->pp_children[ i ] ) );
    return current;
}

void MessagesDialog::updatePLTree()
{
    playlist_t *p_playlist = THEPL;
    pldebugTree->clear();
    PL_LOCK;
    pldebugTree->addTopLevelItem( PLWalk( p_playlist->p_root_category ) );
    PL_UNLOCK;
    pldebugTree->expandAll();
    for ( int i=0; i< 5; i++ )
        pldebugTree->resizeColumnToContents( i );
}
#endif
