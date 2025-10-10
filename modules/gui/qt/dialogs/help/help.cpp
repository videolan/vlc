/*****************************************************************************
 * help.cpp : Help and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

#include "qt.hpp"
#include "help.hpp"
#include "util/qt_dirs.hpp"
#include "maininterface/mainctx.hpp"

#include <vlc_about.h>
#include <vlc_intf_strings.h>

#ifdef UPDATE_CHECK
# include <vlc_update.h>
#endif

#include <QTextBrowser>
#include <QString>
#include <QDialogButtonBox>
#include <QEvent>
#include <QDate>
#include <QPushButton>

#include <cassert>

HelpDialog::HelpDialog( qt_intf_t *_p_intf ) : QVLCFrame( _p_intf )

{
    setWindowTitle( qtr( "Help" ) );
    setWindowRole( "vlc-help" );
    setMinimumSize( 350, 300 );

    QVBoxLayout *layout = new QVBoxLayout( this );

    QTextBrowser *helpBrowser = new QTextBrowser( this );
    helpBrowser->setOpenExternalLinks( true );
    helpBrowser->setHtml( qfut(I_LONGHELP) );

    QDialogButtonBox *closeButtonBox = new QDialogButtonBox( this );
    closeButtonBox->addButton(
        new QPushButton( qtr("&Close") ), QDialogButtonBox::RejectRole );
    closeButtonBox->setFocus();

    layout->addWidget( helpBrowser );
    layout->addWidget( closeButtonBox );

    connect( closeButtonBox, &QDialogButtonBox::rejected, this, &HelpDialog::close );
    restoreWidgetPosition( "Help", QSize( 500, 450 ) );
}

HelpDialog::~HelpDialog()
{
    saveWidgetPosition( "Help" );
}

AboutDialog::AboutDialog( qt_intf_t *_p_intf)
            : QVLCDialog( nullptr, _p_intf ), b_advanced( false )
{
    /* Build UI */
    ui.setupUi( this );
    setWindowTitle( qtr( "About" ) );
    setWindowRole( "vlc-about" );
    setWindowModality( Qt::WindowModal );

    ui.version->setText(qfu( " " VERSION_MESSAGE ) );
    ui.title->setText("<html><head/><body><p><span style=\" font-size:26pt; color:#353535;\"> " + qtr( "VLC media player" ) + " </span></p></body></html>");

    ui.MainBlabla->setText("<html><head/><body>" +
    qtr( "<p>VLC media player is a free and open source media player, encoder, and streamer made by the volunteers of the <a href=\"https://www.videolan.org/\"><span style=\" text-decoration: underline; color:#0057ae;\">VideoLAN</span></a> community.</p><p>VLC uses its internal codecs, works on essentially every popular platform, and can read almost all files, CDs, DVDs, network streams, capture cards and other media formats!</p><p><a href=\"https://www.videolan.org/contribute/\"><span style=\" text-decoration: underline; color:#0057ae;\">Help and join us!</span></a>" ) +
    "</p></body> </html>");

#if 0
    if( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY && var_InheritBool( p_intf, "qt-icon-change" ) )
        ui.iconVLC->setPixmap( QPixmap( ":/logo/vlc128-xmas.png" ) );
    else
        ui.iconVLC->setPixmap( QPixmap( ":/logo/vlc128.png" ) );
#endif

#if 0
    ifdef UPDATE_CHECK
#else
    ui.update->hide();
#endif

    /* GPL License */
    ui.licensePage->setText( qfu( psz_license ) );

    /* People who helped */
    ui.creditPage->setText( qfu( psz_thanks ) );

    /* People who wrote the software */
    ui.authorsPage->setText( qfu( psz_authors ) );

    ui.licenseButton->setText( "<html><head/><body><p><span style=\" text-decoration: underline; color:#0057ae;\">"+qtr( "License" )+"</span></p></body></html>");
    ui.licenseButton->installEventFilter( this );

    ui.authorsButton->setText( "<html><head/><body><p><span style=\" text-decoration: underline; color:#0057ae;\">"+qtr( "Authors" )+"</span></p></body></html>");
    ui.authorsButton->installEventFilter( this );

    ui.creditsButton->setText( "<html><head/><body><p><span style=\" text-decoration: underline; color:#0057ae;\">"+qtr( "Credits" )+"</span></p></body></html>");
    ui.creditsButton->installEventFilter( this );

    ui.version->installEventFilter( this );
}

void AboutDialog::showLicense()
{
    ui.stackedWidget->setCurrentWidget( ui.licensePage );
}

void AboutDialog::showAuthors()
{
    ui.stackedWidget->setCurrentWidget( ui.authorsPage );
}

void AboutDialog::showCredit()
{
    ui.stackedWidget->setCurrentWidget( ui.creditPage );
}

bool AboutDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress )
    {
        if( obj == ui.version )
        {
            if( !b_advanced )
            {
                ui.version->setText(qfu( VLC_CompileBy() )+ "@" + qfu( VLC_CompileHost() )
                    + " " + __DATE__ + " " + __TIME__);
                b_advanced = true;
            }
            else
            {
                ui.version->setText(qfu( " " VERSION_MESSAGE ) );
                b_advanced = false;
            }
            return true;
        }
        else if( obj == ui.licenseButton )
            showLicense();
        else if( obj == ui.authorsButton )
            showAuthors();
        else if( obj == ui.creditsButton )
            showCredit();

        return false;
    }

    return QVLCDialog::eventFilter( obj, event);
}

void AboutDialog::showEvent( QShowEvent *event )
{
    ui.stackedWidget->setCurrentWidget( ui.blablaPage );
    QVLCDialog::showEvent( event );
}

#ifdef UPDATE_CHECK

class UpdateModelPrivate
{
public:
    Q_DECLARE_PUBLIC(UpdateModel)
    UpdateModelPrivate(UpdateModel * pub)
        : q_ptr(pub)
    {
    }

    update_t* m_update = nullptr;

    update_release_t* m_release = nullptr;
    UpdateModel::Status m_status = UpdateModel::Unchecked;

    UpdateModel* q_ptr = nullptr;
};

static void UpdateCallback(void *data, bool b_ret)
{
    auto that = (UpdateModelPrivate*)data;
    QMetaObject::invokeMethod(that->q_func(), [that, b_ret](){
        if (!b_ret)
        {
            that->m_status = UpdateModel::CheckFailed;
            that->m_release = nullptr;
        }
        else
        {
            bool needUpdate = update_NeedUpgrade( that->m_update );
            if (!needUpdate)
            {
                that->m_status = UpdateModel::UpToDate;
                that->m_release = nullptr;
            }
            else
            {
                that->m_status = UpdateModel::NeedUpdate;
                that->m_release = update_GetRelease(that->m_update);
            }
        }
        emit that->q_func()->updateStatusChanged();
    });
}

UpdateModel::UpdateModel(qt_intf_t * p_intf)
    : d_ptr(new UpdateModelPrivate(this))
{
    Q_D(UpdateModel);
    d->m_update = update_New( p_intf );
}

UpdateModel::~UpdateModel()
{
    Q_D(UpdateModel);
    update_Delete( d->m_update );
}

void UpdateModel::checkUpdate()
{
    Q_D(UpdateModel);
    if (d->m_status == Checking)
        return;
    d->m_release = nullptr;
    d->m_status = Checking;
    emit updateStatusChanged();
    update_Check( d->m_update, UpdateCallback, d );
}

bool UpdateModel::download(QString destDir)
{
    Q_D(UpdateModel);
    if (d->m_status != NeedUpdate)
        return false;
    update_Download( d->m_update, qtu( destDir ) );
    return true;
}

UpdateModel::Status UpdateModel::updateStatus() const
{
    Q_D(const UpdateModel);
    return d->m_status;
}

int UpdateModel::getMajor() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_major;
}
int UpdateModel::getMinor() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_minor;
}
int UpdateModel::getRevision() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_revision;
}
int UpdateModel::getExtra() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_extra;
}
QString UpdateModel::getDescription() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return qfu( d->m_release->psz_desc );
}
QString UpdateModel::getUrl() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return qfu( d->m_release->psz_desc );
}



/*****************************************************************************
 * UpdateDialog
 *****************************************************************************/

UpdateDialog::UpdateDialog( qt_intf_t *_p_intf ) : QVLCFrame( _p_intf )
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

    BUTTONACT( recheckButton, &UpdateDialog::checkOrDownload );
    connect( ui.updateDialogButtonBox, &QDialogButtonBox::rejected, this, &UpdateDialog::close );

    connect( ui.updateNotifyButtonBox, &QDialogButtonBox::accepted, this, &UpdateDialog::checkOrDownload );
    connect( ui.updateNotifyButtonBox, &QDialogButtonBox::rejected, this, &UpdateDialog::close );

    setMinimumSize( 300, 300 );
    setMaximumSize( 500, 300 );

    restoreWidgetPosition( "Update", maximumSize() );

    m_model = p_intf->p_mi->getUpdateModel();
    connect(m_model, &UpdateModel::updateStatusChanged, this, &UpdateDialog::updateUI);
    /* update status*/
    updateUI();
}

UpdateDialog::~UpdateDialog()
{
    saveWidgetPosition( "Update" );
}

/* Check for updates */
void UpdateDialog::checkOrDownload()
{
    switch (m_model->updateStatus()) {
    case UpdateModel::Unchecked:
    case UpdateModel::UpToDate:
    case UpdateModel::CheckFailed:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateRequestPage );
        m_model->checkUpdate();
        break;
    }
    case UpdateModel::NeedUpdate:
    {
        QString dest_dir = QDir::tempPath();
        if( !dest_dir.isEmpty() )
        {
            dest_dir = toNativeSepNoSlash( std::move(dest_dir) ) + DIR_SEP;
            msg_Dbg( p_intf, "Downloading to folder: %s", qtu( dest_dir ) );
            toggleVisible();
            m_model->download(dest_dir);
            /* FIXME: We should trigger a change to another dialog here ! */
        }
        break;
    }
    default: // Checking
        break;
    }
}

/* Notify the end of the update_Check */
void UpdateDialog::updateUI( )
{
    switch (m_model->updateStatus()) {
    case UpdateModel::NeedUpdate:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateNotifyPage );
        int extra = m_model->getExtra();
        QString message = QString(
                              qtr( "A new version of VLC (%1.%2.%3%4) is available." ) )
                              .arg( m_model->getMajor() )
                              .arg( m_model->getMinor() )
                              .arg( m_model->getRevision()  )
                              .arg( extra == 0 ? QStringLiteral("") : QStringLiteral(".") + QString::number( extra ) );

        ui.updateNotifyLabel->setText( message );
        message = m_model->getDescription().replace( "\n", "<br/>" );

        /* Try to highlight releases featuring security changes */
        int i_index = message.indexOf( "security", Qt::CaseInsensitive );
        if ( i_index >= 0 )
        {
            message.insert( i_index + 8, "</font>" );
            message.insert( i_index, "<font style=\"color:red\">" );
        }
        ui.updateNotifyTextEdit->setHtml( message );
        break;
    }
    case UpdateModel::UpToDate:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
        ui.updateDialogLabel->setText(
            qtr( "You have the latest version of VLC media player." ) );
        break;
    }
    case UpdateModel::CheckFailed:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
        ui.updateDialogLabel->setText(
            qtr( "An error occurred while checking for updates..." ) );
        break;
    }
    case UpdateModel::Checking:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
        ui.updateDialogLabel->setText(
            qtr( "Checking for updates..." ) );
        break;
    }
    case UpdateModel::Unchecked:
        // do nothing
        break;
    }
}

#endif
