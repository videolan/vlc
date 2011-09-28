/*****************************************************************************
 * extensions.cpp: Extensions manager for Qt: dialogs manager
 ****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
 * $Id$
 *
 * Authors: Jean-Philippe André < jpeg # videolan.org >
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

#include "extensions.hpp"
#include "extensions_manager.hpp" // for isUnloading()

#include <vlc_dialog.h>

#include <QGridLayout>
#include <QPushButton>
#include <QSignalMapper>
#include <QLabel>
#include <QPixmap>
#include <QLineEdit>
#include <QTextBrowser>
#include <QCheckBox>
#include <QListWidget>
#include <QComboBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QKeyEvent>
#include "util/customwidgets.hpp"

ExtensionsDialogProvider *ExtensionsDialogProvider::instance = NULL;

static int DialogCallback( vlc_object_t *p_this, const char *psz_variable,
                           vlc_value_t old_val, vlc_value_t new_val,
                           void *param );


ExtensionsDialogProvider::ExtensionsDialogProvider( intf_thread_t *_p_intf,
                                                    extensions_manager_t *p_mgr )
        : QObject( NULL ), p_intf( _p_intf ), p_extensions_manager( p_mgr )
{
    // At this point, we consider that the Qt interface already called
    // dialog_Register() in order to be the extension dialog provider
    var_Create( p_intf, "dialog-extension", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-extension", DialogCallback, NULL );

    CONNECT( this, SignalDialog( extension_dialog_t* ),
             this, UpdateExtDialog( extension_dialog_t* ) );
}

ExtensionsDialogProvider::~ExtensionsDialogProvider()
{
    msg_Dbg( p_intf, "ExtensionsDialogProvider is quitting..." );
    var_DelCallback( p_intf, "dialog-extension", DialogCallback, NULL );
}

/** Create a dialog
 * Note: Lock on p_dialog->lock must be held. */
ExtensionDialog* ExtensionsDialogProvider::CreateExtDialog(
        extension_dialog_t *p_dialog )
{
    ExtensionDialog *dialog = new ExtensionDialog( p_intf,
                                                   p_extensions_manager,
                                                   p_dialog );
    p_dialog->p_sys_intf = (void*) dialog;
    CONNECT( dialog, destroyDialog( extension_dialog_t* ),
             this, DestroyExtDialog( extension_dialog_t* ) );
    return dialog;
}

/** Destroy a dialog
 * Note: Lock on p_dialog->lock must be held. */
int ExtensionsDialogProvider::DestroyExtDialog( extension_dialog_t *p_dialog )
{
    assert( p_dialog );
    ExtensionDialog *dialog = ( ExtensionDialog* ) p_dialog->p_sys_intf;
    if( !dialog )
        return VLC_EGENERIC;
    delete dialog;
    p_dialog->p_sys_intf = NULL;
    vlc_cond_signal( &p_dialog->cond );
    return VLC_SUCCESS;
}

/**
 * Update/Create/Destroy a dialog
 **/
ExtensionDialog* ExtensionsDialogProvider::UpdateExtDialog(
        extension_dialog_t *p_dialog )
{
    assert( p_dialog );

    ExtensionDialog *dialog = ( ExtensionDialog* ) p_dialog->p_sys_intf;
    if( p_dialog->b_kill && !dialog )
    {
        /* This extension could not be activated properly but tried
           to create a dialog. We must ignore it. */
        return NULL;
    }

    vlc_mutex_lock( &p_dialog->lock );
    if( !p_dialog->b_kill && !dialog )
    {
        dialog = CreateExtDialog( p_dialog );
        dialog->setVisible( !p_dialog->b_hide );
        dialog->has_lock = false;
    }
    else if( !p_dialog->b_kill && dialog )
    {
        dialog->has_lock = true;
        dialog->UpdateWidgets();
        if( strcmp( qtu( dialog->windowTitle() ),
                    p_dialog->psz_title ) != 0 )
            dialog->setWindowTitle( qfu( p_dialog->psz_title ) );
        dialog->has_lock = false;
        dialog->setVisible( !p_dialog->b_hide );
    }
    else if( p_dialog->b_kill )
    {
        DestroyExtDialog( p_dialog );
    }
    vlc_cond_signal( &p_dialog->cond );
    vlc_mutex_unlock( &p_dialog->lock );
    return dialog;
}

/**
 * Ask the dialog manager to create/update/kill the dialog. Thread-safe.
 **/
void ExtensionsDialogProvider::ManageDialog( extension_dialog_t *p_dialog )
{
    assert( p_dialog );
    ExtensionsManager *extMgr = ExtensionsManager::getInstance( p_intf );
    assert( extMgr != NULL );
    if( !extMgr->isUnloading() )
        emit SignalDialog( p_dialog ); // Safe because we signal Qt thread
    else
        UpdateExtDialog( p_dialog ); // This is safe, we're already in Qt thread
}

/**
 * Ask the dialogs provider to create a new dialog
 **/
static int DialogCallback( vlc_object_t *p_this, const char *psz_variable,
                           vlc_value_t old_val, vlc_value_t new_val,
                           void *param )
{
    (void) p_this;
    (void) psz_variable;
    (void) old_val;
    (void) param;

    ExtensionsDialogProvider *p_edp = ExtensionsDialogProvider::getInstance();
    if( !p_edp )
        return VLC_EGENERIC;
    if( !new_val.p_address )
        return VLC_EGENERIC;

    extension_dialog_t *p_dialog = ( extension_dialog_t* ) new_val.p_address;
    p_edp->ManageDialog( p_dialog );
    return VLC_SUCCESS;
}


ExtensionDialog::ExtensionDialog( intf_thread_t *_p_intf,
                                  extensions_manager_t *p_mgr,
                                  extension_dialog_t *_p_dialog )
         : QDialog( NULL ), p_intf( _p_intf ), p_extensions_manager( p_mgr )
         , p_dialog( _p_dialog ), has_lock(true)
{
    assert( p_dialog );
    CONNECT( ExtensionsDialogProvider::getInstance(), destroyed(),
             this, parentDestroyed() );

    msg_Dbg( p_intf, "Creating a new dialog: '%s'", p_dialog->psz_title );
    this->setWindowFlags( Qt::WindowMinMaxButtonsHint
                        | Qt::WindowCloseButtonHint );
    this->setWindowTitle( qfu( p_dialog->psz_title ) );

    layout = new QGridLayout( this );
    clickMapper = new QSignalMapper( this );
    CONNECT( clickMapper, mapped( QObject* ), this, TriggerClick( QObject* ) );
    inputMapper = new QSignalMapper( this );
    CONNECT( inputMapper, mapped( QObject* ), this, SyncInput( QObject* ) );
    selectMapper = new QSignalMapper( this );
    CONNECT( selectMapper, mapped( QObject* ), this, SyncSelection(QObject*) );

    UpdateWidgets();
}

ExtensionDialog::~ExtensionDialog()
{
    msg_Dbg( p_intf, "Deleting extension dialog '%s'", qtu(windowTitle()) );
}

QWidget* ExtensionDialog::CreateWidget( extension_widget_t *p_widget )
{
    QLabel *label = NULL;
    QPushButton *button = NULL;
    QTextBrowser *textArea = NULL;
    QLineEdit *textInput = NULL;
    QCheckBox *checkBox = NULL;
    QComboBox *comboBox = NULL;
    QListWidget *list = NULL;
    SpinningIcon *spinIcon = NULL;
    struct extension_widget_t::extension_widget_value_t *p_value = NULL;

    assert( p_widget->p_sys_intf == NULL );

    switch( p_widget->type )
    {
        case EXTENSION_WIDGET_LABEL:
            label = new QLabel( qfu( p_widget->psz_text ), this );
            p_widget->p_sys_intf = label;
            label->setTextFormat( Qt::RichText );
            label->setOpenExternalLinks( true );
            return label;

        case EXTENSION_WIDGET_BUTTON:
            button = new QPushButton( qfu( p_widget->psz_text ), this );
            clickMapper->setMapping( button, new WidgetMapper( p_widget ) );
            CONNECT( button, clicked(), clickMapper, map() );
            p_widget->p_sys_intf = button;
            return button;

        case EXTENSION_WIDGET_IMAGE:
            label = new QLabel( this );
            label->setPixmap( QPixmap( qfu( p_widget->psz_text ) ) );
            if( p_widget->i_width > 0 )
                label->setMaximumWidth( p_widget->i_width );
            if( p_widget->i_height > 0 )
                label->setMaximumHeight( p_widget->i_height );
            label->setScaledContents( true );
            p_widget->p_sys_intf = label;
            return label;

        case EXTENSION_WIDGET_HTML:
            textArea = new QTextBrowser( this );
            textArea->setOpenExternalLinks( true );
            textArea->setHtml( qfu( p_widget->psz_text ) );
            p_widget->p_sys_intf = textArea;
            return textArea;

        case EXTENSION_WIDGET_TEXT_FIELD:
            textInput = new QLineEdit( this );
            textInput->setText( qfu( p_widget->psz_text ) );
            textInput->setReadOnly( false );
            textInput->setEchoMode( QLineEdit::Normal );
            inputMapper->setMapping( textInput, new WidgetMapper( p_widget ) );
            /// @note: maybe it would be wiser to use textEdited here?
            CONNECT( textInput, textChanged(const QString &),
                     inputMapper, map() );
            p_widget->p_sys_intf = textInput;
            return textInput;

        case EXTENSION_WIDGET_PASSWORD:
            textInput = new QLineEdit( this );
            textInput->setText( qfu( p_widget->psz_text ) );
            textInput->setReadOnly( false );
            textInput->setEchoMode( QLineEdit::Password );
            inputMapper->setMapping( textInput, new WidgetMapper( p_widget ) );
            /// @note: maybe it would be wiser to use textEdited here?
            CONNECT( textInput, textChanged(const QString &),
                     inputMapper, map() );
            p_widget->p_sys_intf = textInput;
            return textInput;

        case EXTENSION_WIDGET_CHECK_BOX:
            checkBox = new QCheckBox( this );
            checkBox->setText( qfu( p_widget->psz_text ) );
            checkBox->setChecked( p_widget->b_checked );
            clickMapper->setMapping( checkBox, new WidgetMapper( p_widget ) );
            CONNECT( checkBox, stateChanged( int ), clickMapper, map() );
            p_widget->p_sys_intf = checkBox;
            return checkBox;

        case EXTENSION_WIDGET_DROPDOWN:
            comboBox = new QComboBox( this );
            comboBox->setEditable( false );
            for( p_value = p_widget->p_values;
                 p_value != NULL;
                 p_value = p_value->p_next )
            {
                comboBox->addItem( qfu( p_value->psz_text ), p_value->i_id );
            }
            /* Set current item */
            if( p_widget->psz_text )
            {
                int idx = comboBox->findText( qfu( p_widget->psz_text ) );
                if( idx >= 0 )
                    comboBox->setCurrentIndex( idx );
            }
            selectMapper->setMapping( comboBox, new WidgetMapper( p_widget ) );
            CONNECT( comboBox, currentIndexChanged( const QString& ),
                     selectMapper, map() );
            return comboBox;

        case EXTENSION_WIDGET_LIST:
            list = new QListWidget( this );
            list->setSelectionMode( QAbstractItemView::ExtendedSelection );
            for( p_value = p_widget->p_values;
                 p_value != NULL;
                 p_value = p_value->p_next )
            {
                QListWidgetItem *item =
                    new QListWidgetItem( qfu( p_value->psz_text ) );
                item->setData( Qt::UserRole, p_value->i_id );
                list->addItem( item );
            }
            selectMapper->setMapping( list, new WidgetMapper( p_widget ) );
            CONNECT( list, itemSelectionChanged(),
                     selectMapper, map() );
            return list;

        case EXTENSION_WIDGET_SPIN_ICON:
            spinIcon = new SpinningIcon( this );
            spinIcon->play( p_widget->i_spin_loops );
            p_widget->p_sys_intf = spinIcon;
            return spinIcon;

        default:
            msg_Err( p_intf, "Widget type %d unknown", p_widget->type );
            return NULL;
    }
}

/**
 * Forward click event to the extension
 * @param object A WidgetMapper, whose data() is the p_widget
 **/
int ExtensionDialog::TriggerClick( QObject *object )
{
    assert( object != NULL );
    WidgetMapper *mapping = static_cast< WidgetMapper* >( object );
    extension_widget_t *p_widget = mapping->getWidget();

    QCheckBox *checkBox = NULL;
    int i_ret = VLC_EGENERIC;

    bool lockedHere = false;
    if( !has_lock )
    {
        vlc_mutex_lock( &p_dialog->lock );
        has_lock = true;
        lockedHere = true;
    }

    switch( p_widget->type )
    {
        case EXTENSION_WIDGET_BUTTON:
            i_ret = extension_WidgetClicked( p_dialog, p_widget );
            break;

        case EXTENSION_WIDGET_CHECK_BOX:
            checkBox = static_cast< QCheckBox* >( p_widget->p_sys_intf );
            p_widget->b_checked = checkBox->isChecked();
            i_ret = VLC_SUCCESS;
            break;

        default:
            msg_Dbg( p_intf, "A click event was triggered by a wrong widget" );
            break;
    }

    if( lockedHere )
    {
        vlc_mutex_unlock( &p_dialog->lock );
        has_lock = false;
    }

    return i_ret;
}

/**
 * Synchronize psz_text with the widget's text() value on update
 * @param object A WidgetMapper
 **/
void ExtensionDialog::SyncInput( QObject *object )
{
    assert( object != NULL );

    bool lockedHere = false;
    if( !has_lock )
    {
        vlc_mutex_lock( &p_dialog->lock );
        has_lock = true;
        lockedHere = true;
    }

    WidgetMapper *mapping = static_cast< WidgetMapper* >( object );
    extension_widget_t *p_widget = mapping->getWidget();
    assert( p_widget->type == EXTENSION_WIDGET_TEXT_FIELD
            || p_widget->type == EXTENSION_WIDGET_PASSWORD );
    /* Synchronize psz_text with the new value */
    QLineEdit *widget = static_cast< QLineEdit* >( p_widget->p_sys_intf );
    char *psz_text = widget->text().isNull() ? NULL : strdup( qtu( widget->text() ) );
    free( p_widget->psz_text );
    p_widget->psz_text =  psz_text;

    if( lockedHere )
    {
        vlc_mutex_unlock( &p_dialog->lock );
        has_lock = false;
    }
}

/**
 * Synchronize parameter b_selected in the values list
 * @param object A WidgetMapper
 **/
void ExtensionDialog::SyncSelection( QObject *object )
{
    assert( object != NULL );
    struct extension_widget_t::extension_widget_value_t *p_value;

    bool lockedHere = false;
    if( !has_lock )
    {
        vlc_mutex_lock( &p_dialog->lock );
        has_lock = true;
        lockedHere = true;
    }

    WidgetMapper *mapping = static_cast< WidgetMapper* >( object );
    extension_widget_t *p_widget = mapping->getWidget();
    assert( p_widget->type == EXTENSION_WIDGET_DROPDOWN
            || p_widget->type == EXTENSION_WIDGET_LIST );

    if( p_widget->type == EXTENSION_WIDGET_DROPDOWN )
    {
        QComboBox *combo = static_cast< QComboBox* >( p_widget->p_sys_intf );
        for( p_value = p_widget->p_values;
             p_value != NULL;
             p_value = p_value->p_next )
        {
//             if( !qstrcmp( p_value->psz_text, qtu( combo->currentText() ) ) )
            if( combo->itemData( combo->currentIndex(), Qt::UserRole ).toInt()
                == p_value->i_id )
            {
                p_value->b_selected = true;
            }
            else
            {
                p_value->b_selected = false;
            }
        }
        free( p_widget->psz_text );
        p_widget->psz_text = strdup( qtu( combo->currentText() ) );
    }
    else if( p_widget->type == EXTENSION_WIDGET_LIST )
    {
        QListWidget *list = static_cast<QListWidget*>( p_widget->p_sys_intf );
        QList<QListWidgetItem *> selection = list->selectedItems();
        for( p_value = p_widget->p_values;
             p_value != NULL;
             p_value = p_value->p_next )
        {
            bool b_selected = false;
            foreach( const QListWidgetItem *item, selection )
            {
//                 if( !qstrcmp( qtu( item->text() ), p_value->psz_text ) )
                if( item->data( Qt::UserRole ).toInt() == p_value->i_id )
                {
                    b_selected = true;
                    break;
                }
            }
            p_value->b_selected = b_selected;
        }
    }

    if( lockedHere )
    {
        vlc_mutex_unlock( &p_dialog->lock );
        has_lock = false;
    }
}

void ExtensionDialog::UpdateWidgets()
{
    assert( p_dialog );
    extension_widget_t *p_widget;
    FOREACH_ARRAY( p_widget, p_dialog->widgets )
    {
        if( !p_widget ) continue; /* Some widgets may be NULL at this point */
        QWidget *widget;
        int row = p_widget->i_row - 1;
        int col = p_widget->i_column - 1;
        if( row < 0 )
        {
            row = layout->rowCount();
            col = 0;
        }
        else if( col < 0 )
            col = layout->columnCount();
        int hsp = __MAX( 1, p_widget->i_horiz_span );
        int vsp = __MAX( 1, p_widget->i_vert_span );
        if( !p_widget->p_sys_intf && !p_widget->b_kill )
        {
            widget = CreateWidget( p_widget );
            if( !widget )
            {
                msg_Warn( p_intf, "Could not create a widget for dialog %s",
                          p_dialog->psz_title );
                continue;
            }
            widget->setVisible( !p_widget->b_hide );
            layout->addWidget( widget, row, col, vsp, hsp );
            if( ( p_widget->i_width > 0 ) && ( p_widget->i_height > 0 ) )
                widget->resize( p_widget->i_width, p_widget->i_height );
            p_widget->p_sys_intf = widget;
            this->resize( sizeHint() );
            /* If an update was required, cancel it as we just created the widget */
            p_widget->b_update = false;
        }
        else if( p_widget->p_sys_intf && !p_widget->b_kill
                 && p_widget->b_update )
        {
            widget = UpdateWidget( p_widget );
            if( !widget )
            {
                msg_Warn( p_intf, "Could not update a widget for dialog %s",
                          p_dialog->psz_title );
                return;
            }
            widget->setVisible( !p_widget->b_hide );
            layout->addWidget( widget, row, col, vsp, hsp );
            if( ( p_widget->i_width > 0 ) && ( p_widget->i_height > 0 ) )
                widget->resize( p_widget->i_width, p_widget->i_height );
            p_widget->p_sys_intf = widget;
            this->resize( sizeHint() );

            /* Do not update again */
            p_widget->b_update = false;
        }
        else if( p_widget->p_sys_intf && p_widget->b_kill )
        {
            DestroyWidget( p_widget );
            p_widget->p_sys_intf = NULL;
            this->resize( sizeHint() );
        }
    }
    FOREACH_END()
}

QWidget* ExtensionDialog::UpdateWidget( extension_widget_t *p_widget )
{
    QLabel *label = NULL;
    QPushButton *button = NULL;
    QTextBrowser *textArea = NULL;
    QLineEdit *textInput = NULL;
    QCheckBox *checkBox = NULL;
    QComboBox *comboBox = NULL;
    QListWidget *list = NULL;
    SpinningIcon *spinIcon = NULL;
    struct extension_widget_t::extension_widget_value_t *p_value = NULL;

    assert( p_widget->p_sys_intf != NULL );

    switch( p_widget->type )
    {
        case EXTENSION_WIDGET_LABEL:
            label = static_cast< QLabel* >( p_widget->p_sys_intf );
            label->setText( qfu( p_widget->psz_text ) );
            return label;

        case EXTENSION_WIDGET_BUTTON:
            // FIXME: looks like removeMappings does not work
            button = static_cast< QPushButton* >( p_widget->p_sys_intf );
            button->setText( qfu( p_widget->psz_text ) );
            clickMapper->removeMappings( button );
            clickMapper->setMapping( button, new WidgetMapper( p_widget ) );
            CONNECT( button, clicked(), clickMapper, map() );
            return button;

        case EXTENSION_WIDGET_IMAGE:
            label = static_cast< QLabel* >( p_widget->p_sys_intf );
            label->setPixmap( QPixmap( qfu( p_widget->psz_text ) ) );
            return label;

        case EXTENSION_WIDGET_HTML:
            textArea = static_cast< QTextBrowser* >( p_widget->p_sys_intf );
            textArea->setHtml( qfu( p_widget->psz_text ) );
            return textArea;

        case EXTENSION_WIDGET_TEXT_FIELD:
            textInput = static_cast< QLineEdit* >( p_widget->p_sys_intf );
            textInput->setText( qfu( p_widget->psz_text ) );
            return textInput;

        case EXTENSION_WIDGET_PASSWORD:
            textInput = static_cast< QLineEdit* >( p_widget->p_sys_intf );
            textInput->setText( qfu( p_widget->psz_text ) );
            return textInput;

        case EXTENSION_WIDGET_CHECK_BOX:
            checkBox = static_cast< QCheckBox* >( p_widget->p_sys_intf );
            checkBox->setText( qfu( p_widget->psz_text ) );
            checkBox->setChecked( p_widget->b_checked );
            return checkBox;

        case EXTENSION_WIDGET_DROPDOWN:
            comboBox = static_cast< QComboBox* >( p_widget->p_sys_intf );
            // method widget:clear()
            if ( p_widget->p_values == NULL )
            {
                comboBox->clear();
                return comboBox;
            }
            // method widget:addvalue()
            for( p_value = p_widget->p_values;
                 p_value != NULL;
                 p_value = p_value->p_next )
            {
                if ( comboBox->findText( qfu( p_value->psz_text ) ) < 0 )
                    comboBox->addItem( qfu( p_value->psz_text ), p_value->i_id );
            }
            return comboBox;

        case EXTENSION_WIDGET_LIST:
            list = static_cast< QListWidget* >( p_widget->p_sys_intf );
            list->clear();
            for( p_value = p_widget->p_values;
                 p_value != NULL;
                 p_value = p_value->p_next )
            {
                QListWidgetItem *item =
                        new QListWidgetItem( qfu( p_value->psz_text ) );
                item->setData( Qt::UserRole, p_value->i_id );
                list->addItem( item );
            }
            return list;

        case EXTENSION_WIDGET_SPIN_ICON:
            spinIcon = static_cast< SpinningIcon* >( p_widget->p_sys_intf );
            if( !spinIcon->isPlaying() && p_widget->i_spin_loops != 0 )
                spinIcon->play( p_widget->i_spin_loops );
            else if( spinIcon->isPlaying() && p_widget->i_spin_loops == 0 )
                spinIcon->stop();
            p_widget->i_height = p_widget->i_width = 16;
            return spinIcon;

        default:
            msg_Err( p_intf, "Widget type %d unknown", p_widget->type );
            return NULL;
    }
}

void ExtensionDialog::DestroyWidget( extension_widget_t *p_widget,
                                     bool b_cond )
{
    assert( p_widget && p_widget->b_kill );
    QWidget *widget = static_cast< QWidget* >( p_widget->p_sys_intf );
    delete widget;
    p_widget->p_sys_intf = NULL;
    if( b_cond )
        vlc_cond_signal( &p_dialog->cond );
}

/** Implement closeEvent() in order to intercept the event */
void ExtensionDialog::closeEvent( QCloseEvent * )
{
    assert( p_dialog != NULL );
    msg_Dbg( p_intf, "Dialog '%s' received a closeEvent",
             p_dialog->psz_title );
    extension_DialogClosed( p_dialog );
}

/** Grab some keyboard input (ESC, ...) and handle actions manually */
void ExtensionDialog::keyPressEvent( QKeyEvent *event )
{
    assert( p_dialog != NULL );
    switch( event->key() )
    {
    case Qt::Key_Escape:
        close();
        return;
    default:
        QDialog::keyPressEvent( event );
        return;
    }
}

void ExtensionDialog::parentDestroyed()
{
    msg_Dbg( p_intf, "About to destroy dialog '%s'", p_dialog->psz_title );
    deleteLater(); // May not work at this point (event loop can be ended)
    p_dialog->p_sys_intf = NULL;
    vlc_cond_signal( &p_dialog->cond );
}
