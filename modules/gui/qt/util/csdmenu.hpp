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
#ifndef CSDMENU_HPP
#define CSDMENU_HPP

#include <QQmlEngine>
#include <QMenu>

Q_MOC_INCLUDE("maininterface/mainctx.hpp")
class MainCtx;

class CSDMenuPrivate;
class CSDMenu : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged FINAL)
    Q_PROPERTY(bool menuVisible READ getMenuVisible NOTIFY menuVisibleChanged FINAL)

public:
    explicit CSDMenu(QObject* parent = nullptr);
    ~CSDMenu();

public:
    Q_INVOKABLE void popup(const QPoint &windowpos);

    MainCtx* getCtx() const;
    void setCtx(MainCtx* ctx);

    bool getMenuVisible() const;

signals:
    void ctxChanged(MainCtx*);
    void menuVisibleChanged(bool);

protected:
    QScopedPointer<CSDMenuPrivate> d_ptr;
    Q_DECLARE_PRIVATE(CSDMenu)
};

#endif // CSDMENU_HPP
