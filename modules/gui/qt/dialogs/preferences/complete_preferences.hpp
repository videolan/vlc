/*****************************************************************************
 * complete_preferences.hpp : Tree of modules for preferences
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifndef VLC_QT_COMPLETE_PREFERENCES_HPP_
#define VLC_QT_COMPLETE_PREFERENCES_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>

#include <QTreeWidget>
#include <QSet>

#include "qt.hpp"

/**
 * Notes:
 *
 * 1) Core's use of set_category()/set_subcategory() defines the base tree,
 *    with its options spread across it.
 * 2) Certain subcats ('general' type) are not given a node under their cat,
 *    they represent the top level cat's option panel itself (otherwise cat
 *    entries would just have empty panels).
 * 3) Other modules (currently) have their options located under a single tree
 *    node attached to one of the core cat/subcat nodes. The location for this
 *    is chosen based upon the first cat and subcat encountered in the module's
 *    option set. (If the subcat does not belong to the cat, then the node is
 *    attached directly to the cat; If the module's option set has options
 *    before the cat/subcat hint entries, this does not matter; If no cat or
 *    subcat hint is provided in the option set, then no node is created (i.e.
 *    that module's options will not be available in the prefs GUI).
 */

class AdvPrefsPanel;
class QVBoxLayout;

class PrefsItemData : public QObject
{
    Q_OBJECT
public:
    PrefsItemData( QObject * );
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
    module_t *p_module;
};

Q_DECLARE_METATYPE( PrefsItemData* );

class PrefsTree : public QTreeWidget
{
    Q_OBJECT

public:
    PrefsTree( qt_intf_t *, QWidget *, module_t **, size_t );

    void applyAll();
    void cleanAll();
    void filter( const QString &text );
    void setLoadedOnly( bool );

private:
    void doAll( bool );
    bool filterItems( QTreeWidgetItem *item, const QString &text, Qt::CaseSensitivity cs );
    bool collapseUnselectedItems( QTreeWidgetItem *item );
    void updateLoadedStatus( QTreeWidgetItem *item , QSet<QString> *loaded );
    qt_intf_t *p_intf;
    bool b_show_only_loaded;

private slots:
    void resizeColumns();
};

class ConfigControl;

class AdvPrefsPanel : public QWidget
{
    Q_OBJECT
public:
    AdvPrefsPanel( qt_intf_t *, QWidget *, PrefsItemData * );
    AdvPrefsPanel( QWidget *);
    virtual ~AdvPrefsPanel();
    void apply();
    void clean();
private:
    module_config_t *p_config;
    qt_intf_t *p_intf;
    QList<ConfigControl *> controls;
    QVBoxLayout *global_layout;
};

#endif
