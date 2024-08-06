/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include "systray.hpp"
#include "maininterface/mainctx.hpp"
#include "menus/menus.hpp"
#include <QSystemTrayIcon>
#include "playlist/playlist_controller.hpp"
#include "dialogs/dialogs_provider.hpp"

using namespace vlc::playlist;

VLCSystray::VLCSystray(MainCtx* ctx, QObject* parent)
    : QSystemTrayIcon(parent)
    , m_ctx(ctx)
    , m_intf(ctx->getIntf())
{
    assert(ctx);
    assert(m_intf);

    m_notificationSetting = var_InheritInteger(m_intf, "qt-notification");

    QIcon iconVLC;
    if( m_ctx->useXmasCone() )
        iconVLC = QIcon::fromTheme( "vlc-xmas", QIcon( ":/logo/vlc128-xmas.png" ) );
    else
        iconVLC = QIcon::fromTheme( "vlc", QIcon( ":/logo/vlc256.png" ) );

    setIcon(iconVLC);
    setToolTip( qtr( "VLC media player" ));

    m_menu = std::make_unique<VLCMenu>( qtr( "VLC media player"), m_intf );
    m_menu->setIcon( iconVLC );
    setContextMenu(m_menu.get());
    update();
    show();

    connect( this, &QSystemTrayIcon::activated,
            this, &VLCSystray::handleClick );

    /* Connects on nameChanged() */
    connect( m_intf->p_mainPlayerController, &PlayerController::nameChanged,
            this, &VLCSystray::updateTooltipName );
    /* Connect PLAY_STATUS on the systray */
    connect( m_intf->p_mainPlayerController, &PlayerController::playingStateChanged,
            this, &VLCSystray::update );
}

VLCSystray::~VLCSystray()
{}

/**
 * Updates the VLCSystray Icon's menu and toggle the main interface
 */
void VLCSystray::toggleUpdateMenu()
{
    m_ctx->toggleWindowVisibility();
    update();
}

/* First Item of the systray menu */
void VLCSystray::showUpdateMenu()
{
    m_ctx->setInterfaceVisibible(true);
    update();
}

/* First Item of the systray menu */
void VLCSystray::hideUpdateMenu()
{
    m_ctx->setInterfaceVisibible(false);
    update();
}

/* Click on systray Icon */
void VLCSystray::handleClick(
    QSystemTrayIcon::ActivationReason reason )
{
    switch( reason )
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
#ifdef Q_OS_MAC
        VLCMenuBar::updateSystrayMenu( this, p_intf );
#else
        toggleUpdateMenu();
#endif
        break;
    case QSystemTrayIcon::MiddleClick:
        if (PlaylistController* const playlistController = m_intf->p_mainPlaylistController)
            playlistController->togglePlayPause();
        break;
    default:
        break;
    }
}

/**
 * Updates the name of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void VLCSystray::updateTooltipName( const QString& name )
{
    if( name.isEmpty() )
    {
        setToolTip( qtr( "VLC media player" ) );
    }
    else
    {
        setToolTip( name );
        const auto windowVisiblity = m_ctx->interfaceVisibility();
        if( ( m_notificationSetting == NOTIFICATION_ALWAYS ) ||
            ( m_notificationSetting == NOTIFICATION_MINIMIZED && (windowVisiblity == QWindow::Hidden || windowVisiblity == QWindow::Minimized)))
        {
            showMessage( qtr( "VLC media player" ), name,
                                 QSystemTrayIcon::NoIcon, 3000 );
        }
    }
    update();
}

void VLCSystray::update()
{
    // explictly delete submenus, see QTBUG-11070
    for (QAction *action : m_menu->actions()) {
        if (action->menu()) {
            delete action->menu();
        }
    }
    m_menu->clear();

#ifndef Q_OS_MAC
    /* Hide / Show VLC and cone */
    if( m_ctx->interfaceVisibility() != QWindow::Hidden )
    {
        m_menu->addAction(
            QIcon( ":/logo/vlc16.png" ), qtr( "&Hide VLC media player in taskbar" ),
            this, &VLCSystray::hideUpdateMenu);
    }
    else
    {
        m_menu->addAction(
            QIcon( ":/logo/vlc16.png" ), qtr( "Sho&w VLC media player" ),
            this, &VLCSystray::showUpdateMenu);
    }
    m_menu->addSeparator();
#endif

    VLCMenuBar::PopupMenuPlaylistEntries( m_menu.get(), m_intf );
    VLCMenuBar::PopupMenuControlEntries( m_menu.get(), m_intf, false );

    VLCMenuBar::VolumeEntries( m_intf, m_menu.get() );
    m_menu->addSeparator();
    m_menu->addAction(
        QIcon(":/menu/file.svg"), qtr( "&Open Media" ),
        THEDP, &DialogsProvider::openFileDialog);

    m_menu->addAction(
        QIcon(":/menu/exit.svg"), qtr( "&Quit" ),
        THEDP, &DialogsProvider::quit);

    /* Set the menu */
    setContextMenu( m_menu.get() );
}
