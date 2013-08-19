/*****************************************************************************
 * complete_preferences.hpp : Tree of modules for preferences
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
#include <QSet>

class AdvPrefsPanel;
class QLabel;
class QVBoxLayout;

class PrefsItemData : public QObject
{
    Q_OBJECT
public:
    PrefsItemData();
    virtual ~PrefsItemData() { free( psz_shortcut ); };
    bool contains( const QString &text, Qt::CaseSensitivity cs );
    AdvPrefsPanel *panel;
    int i_object_id;
    int i_subcat_id;
    enum prefsType
    {
        TYPE_CATEGORY,
        TYPE_CATSUBCAT,
        TYPE_SUBCATEGORY,
        TYPE_MODULE
    };
    prefsType i_type;
    char *psz_shortcut;
    bool b_loaded;
    QString name;
    QString help;
};

Q_DECLARE_METATYPE( PrefsItemData* );

class PrefsTree : public QTreeWidget
{
    Q_OBJECT

public:
    PrefsTree( intf_thread_t *, QWidget * );

    void applyAll();
    void cleanAll();
    void filter( const QString &text );
    void setLoadedOnly( bool );

private:
    void doAll( bool );
    bool filterItems( QTreeWidgetItem *item, const QString &text, Qt::CaseSensitivity cs );
    bool collapseUnselectedItems( QTreeWidgetItem *item );
    void updateLoadedStatus( QTreeWidgetItem *item , QSet<QString> *loaded );
    intf_thread_t *p_intf;
    bool b_show_only_loaded;

private slots:
    void resizeColumns();
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
    module_config_t *p_config;
    intf_thread_t *p_intf;
    QList<ConfigControl *> controls;
    QVBoxLayout *global_layout;
};

#endif
