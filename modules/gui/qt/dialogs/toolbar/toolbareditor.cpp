/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "toolbareditor.hpp"
#include "util/i18n.hpp"
#include "util/recent_media_model.hpp"
#include "util/settings.hpp"
#include "util/systempalette.hpp"
#include "util/qml_main_context.hpp"
#include "player/playercontrolbarmodel.hpp"
#include "maininterface/main_interface.hpp"

#include <QInputDialog>
#include <QQuickItem>
#include <QtQml/QQmlContext>
#include <QQmlEngine>

#define PROFILE_NAME_1 "Minimalist Style"
#define VALUE_1 "0;64;3;1;4;64;11;64;34;64;9;64;33 | 3;0;1;4"
#define PROFILE_NAME_2 "One-Liner Style"
#define VALUE_2 "0;64;3;1;4;64;7;9;8;64;64;11;10;12;13;65;33 | 17;3;0;1;4;18"
#define PROFILE_NAME_3 "Simplest Style"
#define VALUE_3 "33;65;0;4;1;65;7 | 3;0;4"

ToolbarEditorDialog::ToolbarEditorDialog( QWidget *_w, intf_thread_t *_p_intf)
    : QVLCDialog( _w,  _p_intf )
{
    setWindowTitle( qtr( "Toolbars Editor" ) );
    setWindowRole( "vlc-toolbars-editor" );
    setMinimumWidth( 800 );
    setMinimumHeight( 600 );

    /* Profile */
    QGridLayout *mainLayout = new QGridLayout( this );
    QHBoxLayout *profileBoxLayout = new QHBoxLayout();

    profileCombo = new QComboBox;

    QToolButton *newButton = new QToolButton;
    newButton->setIcon( QIcon( ":/new.svg" ) );
    newButton->setToolTip( qtr("New profile") );
    QToolButton *deleteButton = new QToolButton;
    deleteButton->setIcon( QIcon( ":/toolbar/clear.svg" ) );
    deleteButton->setToolTip( qtr( "Delete the current profile" ) );

    profileBoxLayout->addWidget( new QLabel( qtr( "Select profile:" ) ) );
    profileBoxLayout->addWidget( profileCombo );
    profileBoxLayout->addWidget( newButton );
    profileBoxLayout->addWidget( deleteButton );

    mainLayout->addLayout( profileBoxLayout, 0, 0, 1, 9 );

    /* Fill combos */
    int i_size = getSettings()->beginReadArray( "ToolbarProfiles" );
    for( int i = 0; i < i_size; i++ )
    {
        getSettings()->setArrayIndex(i);
        profileCombo->addItem( getSettings()->value( "ProfileName" ).toString(),
                               getSettings()->value( "Value" ).toString() );
    }
    getSettings()->endArray();

    /* Load defaults ones if we have no combos */
    if( i_size == 0 )
    {
        profileCombo->addItem( PROFILE_NAME_1, QString( VALUE_1 ) );
        profileCombo->addItem( PROFILE_NAME_2, QString( VALUE_2 ) );
        profileCombo->addItem( PROFILE_NAME_3, QString( VALUE_3 ) );
    }
    profileCombo->setCurrentIndex( -1 );

    /* Drag and Drop */
    editorView = new QQuickWidget(this);
    editorView->setClearColor(Qt::transparent);
    QQmlContext* rootCtx = editorView->rootContext();
    QQmlEngine* engine = editorView->engine();

    intf_sys_t* p_sys = p_intf->p_sys;
    MainInterface* mainInterface = p_sys->p_mi;

    rootCtx->setContextProperty( "player", p_sys->p_mainPlayerController );
    rootCtx->setContextProperty( "i18n", new I18n(engine) );
    rootCtx->setContextProperty( "mainctx", new QmlMainContext(p_intf, mainInterface, engine));
    rootCtx->setContextProperty( "mainInterface", mainInterface);
    rootCtx->setContextProperty( "topWindow", mainInterface->windowHandle());
    rootCtx->setContextProperty( "settings",  new Settings( p_intf, engine));
    rootCtx->setContextProperty( "systemPalette", new SystemPalette(engine));
    rootCtx->setContextProperty( "medialib", nullptr );
    rootCtx->setContextProperty( "recentsMedias",  new VLCRecentMediaModel( p_intf, engine ));
    rootCtx->setContextProperty( "toolbareditor",  this);

    editorView->setSource( QUrl ( QStringLiteral("qrc:/dialogs/ToolbarEditor.qml") ) );

    mainLayout->addWidget(editorView, 0, 0);

    editorView->setResizeMode( QQuickWidget::SizeRootObjectToView );

    mainLayout->addWidget( editorView, 1, 0, 1, 9 );
    editorView->show();

    /* Buttons */
    QDialogButtonBox *okCancel = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( qtr( "Cl&ose" ), this );
    okButton->setDefault( true );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ), this );
    okCancel->addButton( okButton, QDialogButtonBox::AcceptRole );
    okCancel->addButton( cancelButton, QDialogButtonBox::RejectRole );

    BUTTONACT( deleteButton, deleteProfile() );
    BUTTONACT( newButton, newProfile() );
    CONNECT( profileCombo, currentIndexChanged( int ), this, changeProfile( int ) );
    BUTTONACT( okButton, close() );
    BUTTONACT( cancelButton, cancel() );
    mainLayout->addWidget( okCancel, 2, 0, 1, 9 );
}
ToolbarEditorDialog::~ToolbarEditorDialog()
{
    getSettings()->beginWriteArray( "ToolbarProfiles" );
    for( int i = 0; i < profileCombo->count(); i++ )
    {
        getSettings()->setArrayIndex(i);
        getSettings()->setValue( "ProfileName", profileCombo->itemText( i ) );
        getSettings()->setValue( "Value", profileCombo->itemData( i ) );
    }
    getSettings()->endArray();
}

void ToolbarEditorDialog::close()
{
    emit saveConfig();
    accept();
}

void ToolbarEditorDialog::cancel()
{
    reject();
}

void ToolbarEditorDialog::newProfile()
{
    bool ok;
    QString name =  QInputDialog::getText( this, qtr( "Profile Name" ),
                                           qtr( "Please enter the new profile name." ), QLineEdit::Normal, 0, &ok );
    if( !ok ) return;

    QVariant config;
    QMetaObject::invokeMethod(editorView->rootObject(),"getProfileConfig",
                              Q_RETURN_ARG(QVariant, config));

    profileCombo->addItem( name, config.toString() );
    profileCombo->setCurrentIndex( profileCombo->count() - 1 );
}

void ToolbarEditorDialog::deleteProfile()
{
    profileCombo->removeItem( profileCombo->currentIndex() );
}

void ToolbarEditorDialog::changeProfile( int i )
{
    QStringList qs_list = profileCombo->itemData( i ).toString().split( "|" );
    if( qs_list.count() < 2 )
            return;
    emit updatePlayerModel("MainPlayerToolbar",qs_list[0]);
    emit updatePlayerModel("MiniPlayerToolbar",qs_list[1]);
}

void ToolbarEditorDialog::deleteCursor()
{
    QApplication::setOverrideCursor(Qt::ForbiddenCursor);
}

void ToolbarEditorDialog::restoreCursor()
{
    QApplication::restoreOverrideCursor();
}
