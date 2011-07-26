/*****************************************************************************
 * preferences_tree.hpp : Tree of modules for preferences
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>

#include <QTreeWidget>

enum
{
    TYPE_CATEGORY,
    TYPE_CATSUBCAT,
    TYPE_SUBCATEGORY,
    TYPE_MODULE
};

class AdvPrefsPanel;
class QLabel;
class QVBoxLayout;

class PrefsItemData : public QObject
{
public:
    PrefsItemData()
    { panel = NULL; i_object_id = 0; i_subcat_id = -1; psz_name = NULL; };
    virtual ~PrefsItemData() { free( psz_name ); };
    bool contains( const QString &text, Qt::CaseSensitivity cs );
    AdvPrefsPanel *panel;
    int i_object_id;
    int i_subcat_id;
    int i_type;
    char *psz_name;
    QString name;
    QString help;
};

Q_DECLARE_METATYPE( PrefsItemData* );

class PrefsTree : public QTreeWidget
{
    Q_OBJECT
public:
    PrefsTree( intf_thread_t *, QWidget * );
    virtual ~PrefsTree();

    void applyAll();
    void cleanAll();

private:
    void doAll( bool );
    intf_thread_t *p_intf;
};

class ConfigControl;

class AdvPrefsPanel : public QWidget
{
    Q_OBJECT
public:
    AdvPrefsPanel( intf_thread_t *, QWidget *, PrefsItemData * );
    AdvPrefsPanel( QWidget *);
    virtual ~AdvPrefsPanel();
    void apply();
    void clean();
private:
    intf_thread_t *p_intf;
    QList<ConfigControl *> controls;
    QVBoxLayout *global_layout;
};

#endif
