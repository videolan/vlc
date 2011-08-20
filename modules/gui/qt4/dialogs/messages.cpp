/*****************************************************************************
 * Messages.cpp : Information about an item
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
#include <vlc_atomic.h>

#include <QTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMutex>
#include <QLineEdit>
#include <QScrollBar>

#include <assert.h>

enum {
    MsgEvent_Type = QEvent::User + MsgEventType + 1,
};

class MsgEvent : public QEvent
{
public:
    MsgEvent( int, const msg_item_t *, const char * );

    int priority;
    uintptr_t object_id;
    QString object_type;
    QString header;
    QString module;
    QString text;
};

MsgEvent::MsgEvent( int type, const msg_item_t *msg, const char *text )
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
    updateTree();

    /* Modules tree */
    ui.modulesTree->setHeaderHidden( true );

    /* Buttons and general layout */
    ui.saveLogButton->setToolTip( qtr( "Saves all the displayed logs to a file" ) );

    int verbosity = var_InheritInteger( p_intf, "verbose" );
    vlc_atomic_set( &this->verbosity, verbosity );
    ui.verbosityBox->setValue( verbosity );

    char *objs = var_InheritString( p_intf, "verbose-objects" );
    if( objs != NULL )
    {
        ui.vbobjectsEdit->setText( qfu(objs) );
        free( objs );
    }
    updateConfig();
    ui.vbobjectsEdit->setToolTip( "verbose-objects usage: \n"
                            "--verbose-objects=+printthatobject,-dontprintthatone\n"
                            "(keyword 'all' to applies to all objects)");

    updateButton = new QPushButton( QIcon(":/update"), "" );
    updateButton->setToolTip( qtr("Update the tree") );
    ui.mainTab->setCornerWidget( updateButton );
    updateButton->setVisible( false );
    updateButton->setFlat( true );

    BUTTONACT( ui.clearButton, clear() );
    BUTTONACT( updateButton, updateTree() );
    BUTTONACT( ui.saveLogButton, save() );
    CONNECT( ui.vbobjectsEdit, editingFinished(), this, updateConfig());
    CONNECT( ui.bottomButtonsBox, rejected(), this, hide() );
    CONNECT( ui.verbosityBox, valueChanged( int ),
             this, changeVerbosity( int ) );

    CONNECT( ui.mainTab, currentChanged( int ), this, tabChanged( int ) );

    /* General action */
    readSettings( "Messages", QSize( 600, 450 ) );

    /* Hook up to LibVLC messaging */
    sub = vlc_Subscribe( MsgCallback, this );
}

MessagesDialog::~MessagesDialog()
{
    writeSettings( "Messages" );
    vlc_Unsubscribe( sub );
};

void MessagesDialog::changeVerbosity( int verbosity )
{
    vlc_atomic_set( &this->verbosity, verbosity );
}

void MessagesDialog::updateConfig()
{
    const QString& objects = ui.vbobjectsEdit->text();
    /* FIXME: config item should be part of Qt4 module */
    config_PutPsz(p_intf, "verbose-objects", qtu(objects));

    QStringList filterOut, filterIn;
    /* If a filter is set, disable by default */
    /* If no filters are set, enable */
    filterDefault = objects.isEmpty();
    foreach( const QString& elem, objects.split(QChar(',')) )
    {
        QString object = elem;
        bool add = true;

        if( elem.startsWith(QChar('-')) )
        {
            add = false;
            object.remove( 0, 1 );
        }
        else if( elem.startsWith(QChar('+')) )
            object.remove( 0, 1 );

        if( object.compare(qfu("all"), Qt::CaseInsensitive) == 0 )
            filterDefault = add;
        else
            (add ? &filterIn : &filterOut)->append( object );
    }
    filter = filterDefault ? filterOut : filterIn;
    filter.removeDuplicates();
}

void MessagesDialog::sinkMessage( const MsgEvent *msg )
{
    if( (filter.contains(msg->module) || filter.contains(msg->object_type))
                                                            == filterDefault )
        return;

    QTextEdit *messages = ui.messages;
    /* Only scroll if the viewport is at the end.
       Don't bug user by auto-changing/loosing viewport on insert(). */
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

    messages->setFontItalic( true );
    messages->setTextColor( "darkBlue" );
    messages->insertPlainText( msg->module );

    switch (msg->priority)
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
    messages->insertPlainText( msg->text );
    messages->insertPlainText( "\n" );
    if ( b_autoscroll ) messages->ensureCursorVisible();
}

void MessagesDialog::customEvent( QEvent *event )
{
    MsgEvent *msge = static_cast<MsgEvent *>(event);

    assert( msge );
    sinkMessage( msge );
}

void MessagesDialog::clear()
{
    ui.messages->clear();
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
        out << ui.messages->toPlainText() << "\n";

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

void MessagesDialog::updateTree()
{
    ui.modulesTree->clear();
    buildTree( NULL, VLC_OBJECT( p_intf->p_libvlc ) );
}

void MessagesDialog::tabChanged( int i )
{
    updateButton->setVisible( i == 1 );
}

void MessagesDialog::MsgCallback( void *self, int type, const msg_item_t *item,
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
