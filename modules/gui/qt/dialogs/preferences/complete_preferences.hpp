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
#include <vlc_config_cat.h>

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSet>

#include "qt.hpp"

/**
 * Notes:
 *
 * 1) Core's use of set_subcategory() defines the base tree, with its options
 *    spread across it.
 * 2) Certain subcats ('general' type) are not given a node under their cat,
 *    they represent the top level cat's option panel itself (otherwise cat
 *    entries would just have empty panels).
 * 3) Plugins (currently) have their option sets located under a single tree
 *    node attached to one of the core cat/subcat nodes. The location for this
 *    is chosen based upon the first subcat encountered in the plugin's option
 *    set (others are ignored). If the plugin's option set has options before
 *    the cat/subcat hint entries, this does not matter; If no cat or subcat
 *    hint is provided in the option set, then no node is created (i.e. that
 *    plugins's options will not be shown).
 */

class AdvPrefsPanel;
class QVBoxLayout;

class PrefsTreeItem : public QTreeWidgetItem
{
public:
    enum PrefsTreeItemType
    {
        CATEGORY_NODE = QTreeWidgetItem::UserType,
        SUBCATEGORY_NODE,
        PLUGIN_NODE
    };
    PrefsTreeItem( PrefsTreeItemType );
    virtual ~PrefsTreeItem() { free( module_name ); }
    PrefsTreeItem *child(int index) const
    {
        return static_cast<PrefsTreeItem *>( QTreeWidgetItem::child( index ) );
    }
    /* Search filter helper */
    bool contains( const QString &text, Qt::CaseSensitivity cs );
    PrefsTreeItemType node_type;
    AdvPrefsPanel *panel;
    QString name;
    QString help;
    QString help_html;
    enum vlc_config_cat cat_id;
    enum vlc_config_subcat subcat_id;
    module_t *p_module;
    char *module_name;
    bool module_is_loaded;
};

class PrefsTree : public QTreeWidget
{
    Q_OBJECT

public:
    PrefsTree( qt_intf_t *, QWidget *, module_t **, size_t );

    void applyAll();
    void filter( const QString &text );
    void setLoadedOnly( bool );
    PrefsTreeItem *topLevelItem(int index) const
    {
        return static_cast<PrefsTreeItem *>( QTreeWidget::topLevelItem( index ) );
    }

private:
    QTreeWidgetItem *createCatNode( enum vlc_config_cat cat );
    QTreeWidgetItem *createSubcatNode( QTreeWidgetItem * cat, enum vlc_config_subcat subcat );
    void createPluginNode( QTreeWidgetItem * parent, module_t *mod );
    QTreeWidgetItem *findCatItem( enum vlc_config_cat cat );
    QTreeWidgetItem *findSubcatItem( enum vlc_config_subcat subcat );
    bool filterItems( PrefsTreeItem *item, const QString &text, Qt::CaseSensitivity cs );
    void unfilterItems( PrefsTreeItem *item );
    void updateLoadedStatus( PrefsTreeItem *item , QSet<QString> *loaded );
    qt_intf_t *p_intf;
    module_t *main_module;
    bool b_show_only_loaded;
    QTreeWidgetItem *catMap[ARRAY_SIZE(categories_array)] = { nullptr };
    QTreeWidgetItem *subcatMap[ARRAY_SIZE(subcategories_array)] = { nullptr };

private slots:
    void resizeColumns();
};

class ConfigControl;

class AdvPrefsPanel : public QWidget
{
    Q_OBJECT
public:
    AdvPrefsPanel( qt_intf_t *, QWidget *, PrefsTreeItem * );
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
