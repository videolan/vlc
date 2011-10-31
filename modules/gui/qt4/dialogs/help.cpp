/*****************************************************************************
 * Help.cpp : Help and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
 *          Rémi Duraffort <ivoire (at) via.ecp.fr>
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

#include "qt4.hpp"
#include "dialogs/help.hpp"
#include "util/qt_dirs.hpp"

#include <vlc_about.h>
#include <vlc_intf_strings.h>

#ifdef UPDATE_CHECK
# include <vlc_update.h>
#endif

#include <QTextBrowser>
#include <QTabWidget>
#include <QLabel>
#include <QString>
#include <QDialogButtonBox>
#include <QEvent>
#include <QDate>
#include <QPushButton>

#include <assert.h>

HelpDialog::HelpDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )

{
    setWindowTitle( qtr( "Help" ) );
    setWindowRole( "vlc-help" );
    setMinimumSize( 350, 300 );

    QVBoxLayout *layout = new QVBoxLayout( this );

    QTextBrowser *helpBrowser = new QTextBrowser( this );
    helpBrowser->setOpenExternalLinks( true );
    helpBrowser->setHtml( qtr(I_LONGHELP) );

    QDialogButtonBox *closeButtonBox = new QDialogButtonBox( this );
    closeButtonBox->addButton(
        new QPushButton( qtr("&Close") ), QDialogButtonBox::RejectRole );
    closeButtonBox->setFocus();

    layout->addWidget( helpBrowser );
    layout->addWidget( closeButtonBox );

    CONNECT( closeButtonBox, rejected(), this, close() );
    readSettings( "Help", QSize( 500, 450 ) );
}

HelpDialog::~HelpDialog()
{
    writeSettings( "Help" );
}

AboutDialog::AboutDialog( intf_thread_t *_p_intf)
            : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    /* Build UI */
    ui.setupUi( this );
    ui.closeButtonBox->addButton(
        new QPushButton( qtr("&Close"), this ), QDialogButtonBox::RejectRole );

    setWindowTitle( qtr( "About" ) );
    setWindowRole( "vlc-about" );
    setMinimumSize( 600, 500 );
    resize( 600, 500 );
    setWindowModality( Qt::WindowModal );

    CONNECT( ui.closeButtonBox, rejected(), this, close() );
    ui.closeButtonBox->setFocus();

    ui.introduction->setText(
            qtr( "VLC media player" ) + qfu( " " VERSION_MESSAGE ) );

    if( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY && var_InheritBool( p_intf, "qt-icon-change" ) )
        ui.iconVLC->setPixmap( QPixmap( ":/logo/vlc128-xmas.png" ) );
    else
        ui.iconVLC->setPixmap( QPixmap( ":/logo/vlc128.png" ) );

    /* Main Introduction */
    ui.infoLabel->setText(
            qtr( "VLC media player is a free media player, "
                "encoder and streamer that can read from files, "
                "CDs, DVDs, network streams, capture cards and even more!\n"
                "VLC uses its internal codecs and works on essentially every "
                "popular platform.\n\n" )
            + qtr( "This version of VLC was compiled by:\n " )
            + qfu( VLC_CompileBy() )+ " on " + qfu( VLC_CompileHost() ) +
            + " ("__DATE__" "__TIME__").\n"
            + qtr( "Compiler: " ) + qfu( VLC_Compiler() ) + ".\n"
            + qtr( "You are using the Qt4 Interface.\n\n" )
            + qtr( "Copyright (C) " ) + COPYRIGHT_YEARS
            + qtr( " by the VideoLAN Team.\n" )
            + "vlc@videolan.org, http://www.videolan.org" );

    /* GPL License */
    ui.licenseEdit->setText( qfu( psz_license ) );

    /* People who helped */
    ui.thanksEdit->setText( qfu( psz_thanks ) );

    /* People who wrote the software */
    ui.authorsEdit->setText( qfu( psz_authors ) );
}

#ifdef UPDATE_CHECK

/*****************************************************************************
 * UpdateDialog
 *****************************************************************************/
/* callback to get information from the core */
static void UpdateCallback( void *data, bool b_ret )
{
    UpdateDialog* UDialog = (UpdateDialog *)data;
    QEvent* event;

    if( b_ret )
        event = new QEvent( (QEvent::Type)UDOkEvent );
    else
        event = new QEvent( (QEvent::Type)UDErrorEvent );

    QApplication::postEvent( UDialog, event );
}

UpdateDialog::UpdateDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    /* build Ui */
    ui.setupUi( this );
    ui.updateDialogButtonBox->addButton( new QPushButton( qtr("&Close"), this ),
                                         QDialogButtonBox::RejectRole );
    QPushButton *recheckButton = new QPushButton( qtr("&Recheck version"), this );
    ui.updateDialogButtonBox->addButton( recheckButton, QDialogButtonBox::ActionRole );

    ui.updateNotifyButtonBox->addButton( new QPushButton( qtr("&Yes"), this ),
                                         QDialogButtonBox::AcceptRole );
    ui.updateNotifyButtonBox->addButton( new QPushButton( qtr("&No"), this ),
                                         QDialogButtonBox::RejectRole );

    setWindowTitle( qtr( "VLC media player updates" ) );
    setWindowRole( "vlc-update" );

    BUTTONACT( recheckButton, UpdateOrDownload() );
    CONNECT( ui.updateDialogButtonBox, rejected(), this, close() );

    CONNECT( ui.updateNotifyButtonBox, accepted(), this, UpdateOrDownload() );
    CONNECT( ui.updateNotifyButtonBox, rejected(), this, close() );

    /* Create the update structure */
    p_update = update_New( p_intf );
    b_checked = false;

    setMinimumSize( 300, 300 );
    setMaximumSize( 400, 300 );

    readSettings( "Update", QSize( 300, 250 ) );

    /* Check for updates */
    UpdateOrDownload();
}

UpdateDialog::~UpdateDialog()
{
    update_Delete( p_update );
    writeSettings( "Update" );
}

/* Check for updates */
void UpdateDialog::UpdateOrDownload()
{
    if( !b_checked )
    {
        ui.stackedWidget->setCurrentWidget( ui.updateRequestPage );
        update_Check( p_update, UpdateCallback, this );
    }
    else
    {
        QString dest_dir = QDir::tempPath();
        if( !dest_dir.isEmpty() )
        {
            dest_dir = toNativeSepNoSlash( dest_dir ) + DIR_SEP;
            msg_Dbg( p_intf, "Downloading to folder: %s", qtu( dest_dir ) );
            toggleVisible();
            update_Download( p_update, qtu( dest_dir ) );
            /* FIXME: We should trigger a change to another dialog here ! */
        }
    }
}

/* Handle the events */
void UpdateDialog::customEvent( QEvent *event )
{
    if( event->type() == UDOkEvent )
        updateNotify( true );
    else
        updateNotify( false );
}

/* Notify the end of the update_Check */
void UpdateDialog::updateNotify( bool b_result )
{
    /* The update finish without errors */
    if( b_result )
    {
        if( update_NeedUpgrade( p_update ) )
        {
            ui.stackedWidget->setCurrentWidget( ui.updateNotifyPage );
            update_release_t *p_release = update_GetRelease( p_update );
            assert( p_release );
            b_checked = true;
            QString message = QString(
                    qtr( "A new version of VLC (%1.%2.%3%4) is available." ) )
                .arg( QString::number( p_release->i_major ) )
                .arg( QString::number( p_release->i_minor ) )
                .arg( QString::number( p_release->i_revision ) )
                .arg( p_release->i_extra == 0 ? "" : "." + QString::number( p_release->i_extra ) );

            ui.updateNotifyLabel->setText( message );
            ui.updateNotifyTextEdit->setText( qfu( p_release->psz_desc ) );

            /* Force the dialog to be shown */
            this->show();
        }
        else
        {
            ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
            ui.updateDialogLabel->setText(
                    qtr( "You have the latest version of VLC media player." ) );
        }
    }
    else
    {
        ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
        ui.updateDialogLabel->setText(
                    qtr( "An error occurred while checking for updates..." ) );
    }
}

#endif
