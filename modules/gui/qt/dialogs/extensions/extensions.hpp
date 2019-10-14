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
#include <vlc_extensions.h>

#include "assert.h"

#include <QDialog>
class QObject;
class QGridLayout;
class QSignalMapper;
class QCloseEvent;
class QKeyEvent;

class ExtensionsDialogProvider;
class ExtensionDialog;

class ExtensionsDialogProvider : public QObject
{
    /** This is the dialog provider for Extensions dialogs
     * @todo Make this class be a public Singleton<EDP>
     * @todo Add a setExtManager() function (with vlc_object_hold)
     **/

    Q_OBJECT

private:
    static ExtensionsDialogProvider *instance;
    intf_thread_t *p_intf;
    extensions_manager_t *p_extensions_manager;

private slots:
    ExtensionDialog* CreateExtDialog( extension_dialog_t *p_dialog );
    int DestroyExtDialog( extension_dialog_t *p_dialog );
    ExtensionDialog* UpdateExtDialog( extension_dialog_t *p_dialog );

public:
    ExtensionsDialogProvider( intf_thread_t *p_intf,
                              extensions_manager_t *p_mgr );
    virtual ~ExtensionsDialogProvider();

    static ExtensionsDialogProvider* getInstance( intf_thread_t *p_intf = NULL,
                                                  extensions_manager_t *p_mgr = NULL )
    {
        if( !instance )
        {
            assert( p_intf != NULL && p_mgr != NULL );
            instance = new ExtensionsDialogProvider( p_intf, p_mgr );
        }
        return instance;
    }
    static void killInstance()
    {
        delete instance;
        instance = NULL;
    }

    void ManageDialog( extension_dialog_t *p_dialog );

signals:
    void SignalDialog( extension_dialog_t *p_dialog );
};


class ExtensionDialog : public QDialog
{
    Q_OBJECT
private:
    intf_thread_t *p_intf;
    extensions_manager_t *p_extensions_manager;
    extension_t *p_extension;
    extension_dialog_t *p_dialog;
    bool has_lock; ///< Indicates whether Qt thread owns the lock
    QGridLayout *layout;
    QSignalMapper *clickMapper;
    QSignalMapper *inputMapper;
    QSignalMapper *selectMapper;

    QWidget *CreateWidget( extension_widget_t *p_widget );
    QWidget *UpdateWidget( extension_widget_t *p_widget );
    void DestroyWidget( extension_widget_t *p_widget, bool b_cond = true );

protected:
    void closeEvent( QCloseEvent* ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent* ) Q_DECL_OVERRIDE;

private slots:
    int TriggerClick( QObject *object );
    void SyncInput( QObject *object );
    void SyncSelection( QObject *object );
    void parentDestroyed();

signals:
    void destroyDialog( extension_dialog_t *p_dialog );

public:
    ExtensionDialog( intf_thread_t *p_intf,
                     extensions_manager_t *p_mgr,
                     extension_dialog_t *p_dialog );
    virtual ~ExtensionDialog();

    void UpdateWidgets();

    // FIXME: This totally sucks (access to has_lock)
    friend class ExtensionsDialogProvider;
};

class WidgetMapper : public QObject
{
    Q_OBJECT
private:
    extension_widget_t *p_widget;
public:
    WidgetMapper( QObject* parent, extension_widget_t *_p_widget ) :
            QObject(parent), p_widget(_p_widget) {}
    extension_widget_t* getWidget() { return p_widget; }
};

#endif // EXTENSIONS_HPP
