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
#include <QFileDialog>
#include <QDate>

#include <assert.h>

HelpDialog::HelpDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )

{
    setWindowTitle( qtr( "Help" ) );
    setWindowRole( "vlc-help" );
    setMinimumSize( 350, 300 );

    QGridLayout *layout = new QGridLayout( this );
    QTextBrowser *helpBrowser = new QTextBrowser( this );
    helpBrowser->setOpenExternalLinks( true );
    helpBrowser->setHtml( qtr(I_LONGHELP) );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    closeButton->setDefault( true );

    layout->addWidget( helpBrowser, 0, 0, 1, 0 );
    layout->addWidget( closeButton, 1, 3 );

    BUTTONACT( closeButton, close() );
    readSettings( "Help", QSize( 500, 450 ) );
}

HelpDialog::~HelpDialog()
{
    writeSettings( "Help" );
}

void HelpDialog::close()
{
    toggleVisible();
}

AboutDialog::AboutDialog( intf_thread_t *_p_intf)
            : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    setWindowTitle( qtr( "About" ) );
    setWindowRole( "vlc-about" );
    resize( 600, 500 );
    setMinimumSize( 600, 500 );
    setWindowModality( Qt::WindowModal );

    QGridLayout *layout = new QGridLayout( this );
    QTabWidget *tab = new QTabWidget( this );

    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    closeButton->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );
    closeButton->setDefault( true );

    QLabel *introduction = new QLabel(
            qtr( "VLC media player" ) + qfu( " " VERSION_MESSAGE ) );
    QLabel *iconVLC = new QLabel;
    if( QDate::currentDate().dayOfYear() >= 354 )
        iconVLC->setPixmap( QPixmap( ":/logo/vlc48-christmas.png" ) );
    else
        iconVLC->setPixmap( QPixmap( ":/logo/vlc48.png" ) );
    layout->addWidget( iconVLC, 0, 0, 1, 1 );
    layout->addWidget( introduction, 0, 1, 1, 7 );
    layout->addWidget( tab, 1, 0, 1, 8 );
    layout->addWidget( closeButton, 2, 6, 1, 2 );

    /* Main Introduction */
    QWidget *infoWidget = new QWidget( this );
    QHBoxLayout *infoLayout = new QHBoxLayout( infoWidget );
    QLabel *infoLabel = new QLabel(
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
    infoLabel->setWordWrap( infoLabel );

    QLabel *iconVLC2 = new QLabel;
    if( QDate::currentDate().dayOfYear() >= 354 )
        iconVLC2->setPixmap( QPixmap( ":/logo/vlc128-christmas.png" ) );
    else
        iconVLC2->setPixmap( QPixmap( ":/logo/vlc128.png" ) );
    infoLayout->addWidget( iconVLC2 );
    infoLayout->addWidget( infoLabel );

    /* GPL License */
    QTextEdit *licenseEdit = new QTextEdit( this );
    licenseEdit->setText( qfu( psz_license ) );
    licenseEdit->setReadOnly( true );

    /* People who helped */
    QWidget *thanksWidget = new QWidget( this );
    QVBoxLayout *thanksLayout = new QVBoxLayout( thanksWidget );

    QLabel *thanksLabel = new QLabel( qtr( "We would like to thank the whole "
                "VLC community, the testers, our users and the following people "
                "(and the missing ones...) for their collaboration to "
                "create the best free software." ) );
    thanksLabel->setWordWrap( true );
    thanksLayout->addWidget( thanksLabel );
    QTextEdit *thanksEdit = new QTextEdit( this );
    thanksEdit->setText( qfu( psz_thanks ) );
    thanksEdit->setReadOnly( true );
    thanksLayout->addWidget( thanksEdit );

    /* People who wrote the software */
    QTextEdit *authorsEdit = new QTextEdit( this );
    authorsEdit->setText( qfu( psz_authors ) );
    authorsEdit->setReadOnly( true );

    /* add the tabs to the Tabwidget */
    tab->addTab( infoWidget, qtr( "About" ) );
    tab->addTab( authorsEdit, qtr( "Authors" ) );
    tab->addTab( thanksWidget, qtr("Thanks") );
    tab->addTab( licenseEdit, qtr("License") );

    BUTTONACT( closeButton, close() );
}

AboutDialog::~AboutDialog()
{
}

void AboutDialog::close()
{
    toggleVisible();
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
    setWindowTitle( qtr( "VLC media player updates" ) );
    setWindowRole( "vlc-update" );

    QGridLayout *layout = new QGridLayout( this );

    QPushButton *closeButton = new QPushButton( qtr( "&Cancel" ) );
    updateButton = new QPushButton( qtr( "&Recheck version" ) );
    updateButton->setDefault( true );

    QDialogButtonBox *buttonBox = new QDialogButtonBox( Qt::Horizontal );
    buttonBox->addButton( updateButton, QDialogButtonBox::ActionRole );
    buttonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );

    updateLabelTop = new QLabel( qtr( "Checking for an update..." ) );
    updateLabelTop->setWordWrap( true );
    updateLabelTop->setMargin( 8 );

    updateLabelDown = new QLabel( qtr( "\nDo you want to download it?\n" ) );
    updateLabelDown->setWordWrap( true );
    updateLabelDown->hide();

    updateText = new QTextEdit( this );
    updateText->setAcceptRichText(false);
    updateText->setTextInteractionFlags( Qt::TextSelectableByKeyboard|
                                         Qt::TextSelectableByMouse);
    updateText->setEnabled( false );

    layout->addWidget( updateLabelTop, 0, 0 );
    layout->addWidget( updateText, 1, 0 );
    layout->addWidget( updateLabelDown, 2, 0 );
    layout->addWidget( buttonBox, 3, 0 );

    BUTTONACT( updateButton, UpdateOrDownload() );
    BUTTONACT( closeButton, close() );

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

void UpdateDialog::close()
{
    toggleVisible();
}

/* Check for updates */
void UpdateDialog::UpdateOrDownload()
{
    if( !b_checked )
    {
        updateButton->setEnabled( false );
        updateLabelTop->setText( qtr( "Launching an update request..." ) );
        update_Check( p_update, UpdateCallback, this );
    }
    else
    {
        QString dest_dir = QFileDialog::getExistingDirectory( this,
                                 qtr( I_OP_SEL_DIR ),
                                 QVLCUserDir( VLC_DOWNLOAD_DIR ) );

        if( !dest_dir.isEmpty() )
        {
            dest_dir = toNativeSepNoSlash( dest_dir ) + DIR_SEP;
            msg_Dbg( p_intf, "Downloading to folder: %s", qtu( dest_dir ) );
            toggleVisible();
            update_Download( p_update, qtu( dest_dir ) );
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
            update_release_t *p_release = update_GetRelease( p_update );
            assert( p_release );
            b_checked = true;
            updateButton->setText( qtr( "&Yes" ) );
            QString message = qtr( "A new version of VLC(" )
                              + QString::number( p_release->i_major ) + "."
                              + QString::number( p_release->i_minor ) + "."
                              + QString::number( p_release->i_revision );
            if( p_release->extra )
                message += p_release->extra;
            message += qtr( ") is available.");
            updateLabelTop->setText( message );

            updateText->setText( qfu( p_release->psz_desc ) );
            updateText->setEnabled( true );

            updateLabelDown->show();

            /* Force the dialog to be shown */
            this->show();
        }
        else
            updateLabelTop->setText(
                    qtr( "You have the latest version of VLC media player." ) );
    }
    else
        updateLabelTop->setText(
                    qtr( "An error occurred while checking for updates..." ) );

    updateButton->setEnabled( true );
}

#endif

