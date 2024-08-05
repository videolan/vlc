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

#ifndef SYSTRAY_HPP
#define SYSTRAY_HPP

#include <QWidget>
#include <QSystemTrayIcon>

class MainCtx;
struct qt_intf_t;

class VLCSystray : public QSystemTrayIcon
{
    Q_OBJECT
public:
    VLCSystray(MainCtx* ctx, QObject* parent = nullptr);
    virtual ~VLCSystray();

    bool isAvailableAndVisible() const;

    void update();

public slots:
    void hideUpdateMenu();
    void toggleUpdateMenu();
    void showUpdateMenu();

private slots:
    void updateTooltipName( const QString& );
    void handleClick( QSystemTrayIcon::ActivationReason );

private:
    MainCtx* m_ctx = nullptr;
    qt_intf_t* m_intf = nullptr;

    int m_notificationSetting = 0;

    std::unique_ptr<QMenu> m_menu;
};

#endif // SYSTRAY_HPP
