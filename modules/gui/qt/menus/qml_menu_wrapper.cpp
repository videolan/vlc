/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#include "qml_menu_wrapper.hpp"
#include "menus.hpp"
#include "util/qml_main_context.hpp"


#include <QSignalMapper>


static inline void addSubMenu( QMenu *func, QString title, QMenu *bar ) {
    func->setTitle( title );
    bar->addMenu( func);
}

QmlGlobalMenu::QmlGlobalMenu(QObject *parent)
    : VLCMenuBar(parent)
{
}

void QmlGlobalMenu::popup(QPoint pos)
{
    if (!m_ctx)
        return;

    intf_thread_t* p_intf = m_ctx->getIntf();
    if (!p_intf)
        return;

    QMenu* menu = new QMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QMenu* submenu;

    QAction* fileMenu = menu->addMenu(FileMenu( p_intf, menu, p_intf->p_sys->p_mi ));
    fileMenu->setText(qtr( "&Media" ));

    /* Dynamic menus, rebuilt before being showed */
    submenu = menu->addMenu(qtr( "P&layback" ));
    NavigMenu( p_intf, submenu );

    submenu = menu->addMenu(qtr( "&Audio" ));
    AudioMenu( p_intf, submenu );

    submenu = menu->addMenu(qtr( "&Video" ));
    VideoMenu( p_intf, submenu );

    submenu = menu->addMenu(qtr( "Subti&tle" ));
    SubtitleMenu( p_intf, submenu );

    submenu = menu->addMenu(qtr( "Tool&s" ));
    ToolsMenu( p_intf, submenu );

    /* View menu, a bit different */
    submenu = menu->addMenu(qtr( "V&iew" ));
    ViewMenu( p_intf, submenu, p_intf->p_sys->p_mi );

    QAction* helpMenu = menu->addMenu( HelpMenu(menu) );
    helpMenu->setText(qtr( "&Help" ));

    menu->popup(pos);
}

