/*****************************************************************************
 * extensions.hpp: Extensions manager for Qt: dialogs manager
 ****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
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

#ifndef EXTENSIONS_HPP
#define EXTENSIONS_HPP

#include "qt.hpp"

#include <cassert>
#include <unordered_set>
#include <QHash>

#include <QDialog>
class QObject;
class QGridLayout;
class QSignalMapper;
class QCloseEvent;
class QKeyEvent;

class ExtensionsDialogProvider;
class ExtensionDialog;

extern "C" {
    struct extensions_manager_t;
    struct extension_widget_t;
    typedef struct extension_dialog_t extension_dialog_t;
    typedef struct extension_t extension_t;
};

class ExtensionsDialogProvider : public QObject
{
    /** This is the dialog provider for Extensions dialogs
     * @todo Add a setExtManager() function (with vlc_object_hold)
     **/
    Q_OBJECT

public:
    ExtensionsDialogProvider( qt_intf_t *p_intf = nullptr);
    virtual ~ExtensionsDialogProvider();

    void UpdateExtDialog( extension_dialog_t *p_dialog );

private:
    ExtensionDialog* CreateExtDialog( extension_dialog_t *p_dialog );
    int DestroyExtDialog( extension_dialog_t *p_dialog );

    std::unordered_set<ExtensionDialog*> m_dialogs;
    qt_intf_t *p_intf = nullptr;
};


class ExtensionDialog : public QDialog
{
    Q_OBJECT
private:
    qt_intf_t *p_intf;
    extension_t *p_extension;
    extension_dialog_t *p_dialog;
    bool has_lock; ///< Indicates whether Qt thread owns the lock
    QGridLayout *layout;
    QHash<QObject*, extension_widget_t*> m_widgetMapping;

    QWidget *CreateWidget( extension_widget_t *p_widget );
    QWidget *UpdateWidget( extension_widget_t *p_widget );
    void DestroyWidget( extension_widget_t *p_widget, bool b_cond = true );

    void setWidgetMapping(QObject* widget, extension_widget_t *ext_widget);
    extension_widget_t* getWidgetMapping(QObject* widget) const;

protected:
    void closeEvent( QCloseEvent* ) override;
    void keyPressEvent( QKeyEvent* ) override;

private slots:
    int TriggerClick();
    void SyncInput();
    void SyncSelection();

public:
    ExtensionDialog( qt_intf_t *p_intf,
                     extension_dialog_t *p_dialog );
    virtual ~ExtensionDialog();

    void UpdateWidgets();

    // FIXME: This totally sucks (access to has_lock)
    friend class ExtensionsDialogProvider;
};

#endif // EXTENSIONS_HPP
