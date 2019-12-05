/*****************************************************************************
 * messages.cpp : Information about an item
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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

#include <vlc_common.h>
#include <vlc_input_item.h>

#include "dialogs/messages/messages.hpp"

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

    updateButton = new QPushButton( QIcon(":/update.svg"), "" );
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
    static const struct vlc_logger_operations log_ops =
    {
        MessagesDialog::MsgCallback,
        NULL
    };
    libvlc_int_t *vlc = vlc_object_instance(p_intf);

    vlc_LogSet( vlc, &log_ops, this );

    buildTree( NULL, VLC_OBJECT(vlc) );
}

MessagesDialog::~MessagesDialog()
{
    saveWidgetPosition( "Messages" );
    vlc_LogSet( vlc_object_instance(p_intf), NULL, NULL );
};

void MessagesDialog::changeVerbosity( int i_verbosity )
{
    verbosity = i_verbosity;
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

    /* Start a new logic block */
    if( !messages->document()->isEmpty() )
        messages->textCursor().insertBlock();

    /* Insert the prefix */
    QTextCharFormat format;
    format.setProperty( QTextFormat::FontItalic, true );
    format.setForeground( Qt::darkBlue );

    messages->textCursor().insertText( msg->module, format );

    switch ( msg->priority )
    {
        case VLC_MSG_INFO:
            format.setForeground( Qt::darkBlue );
            messages->textCursor().insertText( " info: ", format );
            break;
        case VLC_MSG_ERR:
            format.setForeground( Qt::darkRed );
            messages->textCursor().insertText( " error: ", format );
            break;
        case VLC_MSG_WARN:
            format.setForeground( Qt::darkGreen );
            messages->textCursor().insertText( " warning: ", format );
            break;
        case VLC_MSG_DBG:
        default:
            format.setForeground( Qt::darkGray );
            messages->textCursor().insertText( " debug: ", format );
            break;
    }

    /* Insert the message */
    format.setProperty( QTextFormat::FontItalic, false );
    format.setForeground( messages->palette().windowText() );
    messages->textCursor().insertText( msg->text, format );

    /* Pass the new message thru the filter */
    QTextBlock b = messages->document()->lastBlock();
    b.setVisible( matchFilter( b.text() ) );

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
            qtr( "Texts/Logs (*.log *.txt);; All (*.*)") );

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
                   .arg( qfu( vlc_object_typename(p_obj) ) )
                   .arg( ( name != NULL )
                         ? QString( " \"%1\"" ).arg( qfu( name ) )
                             : "" )
                   .arg( (uintptr_t)p_obj, 0, 16 )
                 );
    free( name );
    item->setExpanded( true );

    size_t count = 0, size;
    vlc_object_t **tab = NULL;

    do
    {
        delete[] tab;
        size = count;
        tab = new vlc_object_t *[size];
        count = vlc_list_children(p_obj, tab, size);
    }
    while (size < count);

    for (size_t i = 0; i < count ; i++)
    {
        buildTree( item, tab[i] );
        vlc_object_release(tab[i]);
    }

    delete[] tab;
}

void MessagesDialog::updateOrClear()
{
    if( ui.mainTab->currentIndex() == 1)
    {
        ui.modulesTree->clear();
        buildTree( NULL, VLC_OBJECT( vlc_object_instance(p_intf) ) );
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
    updateButton->setIcon( i != 0 ? QIcon(":/update.svg") : QIcon(":/toolbar/clear.svg") );
    updateButton->setToolTip( i != 0 ? qtr("Update the tree")
                                     : qtr("Clear the messages") );
}

void MessagesDialog::MsgCallback( void *self, int type, const vlc_log_t *item,
                                  const char *format, va_list ap )
{
    MessagesDialog *dialog = (MessagesDialog *)self;
    char *str;
    int verbosity = dialog->verbosity.load();

    if( verbosity < 0 || verbosity < (type - VLC_MSG_ERR)
     || unlikely(vasprintf( &str, format, ap ) == -1) )
        return;

    int canc = vlc_savecancel();
    QApplication::postEvent( dialog, new MsgEvent( type, item, str ) );
    vlc_restorecancel( canc );
    free( str );
}

#ifndef NDEBUG

void MessagesDialog::updatePLTree()
{
    pldebugTree->clear();
    {
        vlc_playlist_t* playlist = p_intf->p_sys->p_playlist;
        vlc_playlist_Lock(playlist);
        size_t count = vlc_playlist_Count( playlist );
        for (size_t i = 0; i < count; i++)
        {
            QTreeWidgetItem *current = new QTreeWidgetItem();
            vlc_playlist_item_t* item = vlc_playlist_Get( playlist, i );
            input_item_t* media = vlc_playlist_item_GetMedia( item );
            current->setText( 0, qfu( media->psz_name ) );
            current->setToolTip( 0, qfu( media->psz_uri ) );
            current->setText( 1, QString("%1").arg( i ) );
            current->setText( 2, QString("%1").arg( (uintptr_t)media ) );
            //current->setText( 3, QString("0x%1").arg( p_node->i_flags, 0, 16 ) );
            current->setText( 3, QString("0x%1").arg( media->i_type, 0, 16 ) );
            pldebugTree->addTopLevelItem( current );
        }
        vlc_playlist_Unlock(playlist);
    }

    pldebugTree->expandAll();
    for ( int i=0; i< 4; i++ )
        pldebugTree->resizeColumnToContents( i );
}
#endif
