/*****************************************************************************
 * preferences_tree.hpp : Tree of modules for preferences
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _PREFSTREE_H_
#define _PREFSTREE_H_

#include <QTreeWidget>
#include <vlc/vlc.h>
#include <vlc/intf.h>

enum
{
    TYPE_CATEGORY,
    TYPE_CATSUBCAT,
    TYPE_SUBCATEGORY,
    TYPE_MODULE
};

class PrefsPanel;
class QLabel;
class QVBoxLayout;

class PrefsItemData : public QObject
{
public:
    PrefsItemData()
    { panel = NULL; i_object_id = 0; i_subcat_id = -1; };
    virtual ~PrefsItemData() {};
    PrefsPanel *panel;
    int i_object_id;
    int i_subcat_id;
    int i_type;
    bool b_submodule;
    QString name;
    QString help;
};

Q_DECLARE_METATYPE( PrefsItemData* );

class PrefsTree : public QTreeWidget
{
    Q_OBJECT;
public:
    PrefsTree( intf_thread_t *, QWidget * );
    virtual ~PrefsTree();

    void ApplyAll();
    void CleanAll();

private:
    void DoAll( bool );
    intf_thread_t *p_intf;
};

class ConfigControl;

class PrefsPanel : public QWidget
{
    Q_OBJECT
public:
    PrefsPanel( intf_thread_t *, QWidget *, PrefsItemData *, bool );
    PrefsPanel( QWidget *);
    virtual ~PrefsPanel() {};
    void Apply();
    void Clean();
private:
    intf_thread_t *p_intf;
    QList<ConfigControl *> controls;
    QLabel *some_hidden_text;
    QVBoxLayout *global_layout;
    bool advanced;
public slots:
    void setAdvanced( bool, bool );
    void setAdvanced( bool a ) { return setAdvanced( a, false ); }
};

#endif
