/*****************************************************************************
 * expert_model.hpp : Detailed preferences overview - model
 *****************************************************************************
 * Copyright (C) 2019-2022 VLC authors and VideoLAN
 *
 * Authors: Lyndon Brown <jnqnfe@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_EXPERT_PREFERENCES_MODEL_HPP_
#define VLC_QT_EXPERT_PREFERENCES_MODEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <QAbstractListModel>

#include "qt.hpp"

class ConfigControl;
class ExpertPrefsTableModel;

class ExpertPrefsTableItem
{
    friend ExpertPrefsTableModel;

public:
    ExpertPrefsTableItem( module_config_t *, const QString &, const QString &, bool );
    ~ExpertPrefsTableItem();
    int getType() const { return cfg_item->i_type; }
    const QString &getTitle() const { return title; }
    const QString &getDescription();
    module_config_t *getConfig() const { return cfg_item; }
    bool matchesDefault() const { return matches_default; }
    void updateMatchesDefault();
    void updateValueDisplayString();
    /** Search filter helper */
    bool contains( const QString &text, Qt::CaseSensitivity cs );

private:
    void setToDefault();
    void toggleBoolean();
    /** Name shown in table entry, combining module name and option name */
    QString name;
    /** The displayed value text */
    QString displayed_value;
    /** "Pretty" name for info panel */
    QString title;
    /** Description for info panel */
    QString description;
    /** The local copy of the corresponding option */
    module_config_t *cfg_item;
    /** Is item state different to the default value? */
    bool matches_default;
};

class ExpertPrefsTableModel : public QAbstractListModel
{
    Q_OBJECT

public:
    ExpertPrefsTableModel( module_t **, size_t, QWidget * = nullptr );
    ~ExpertPrefsTableModel();
    enum ItemField
    {
        NameField,
        StateField,
        TypeField,
        ValueField,
    };
    enum DataRoles
    {
        TypeClassRole = Qt::UserRole,
        CopyValueRole
    };
    ExpertPrefsTableItem *itemAt( int row ) const
    {
        assert( row < items.count() );
        return items[ row ];
    }
    ExpertPrefsTableItem *itemAt( const QModelIndex &index ) const
    {
        return itemAt( index.row() );
    }
    void toggleBoolean( const QModelIndex & );
    void setItemToDefault( const QModelIndex & );
    void notifyUpdatedRow( int );
    bool submit() override;
    /* Standard model interface */
    QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;
    int rowCount( const QModelIndex &parent = QModelIndex() ) const override;
    int columnCount( const QModelIndex &parent = QModelIndex() ) const override;
    QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const override;

private:
    QList<module_config_t *> config_sets;
    QList<ExpertPrefsTableItem*> items;
    /* Cached translations of common text */
    QString state_same_text;
    QString state_different_text;
    QString true_text;
    QString false_text;
    QString type_boolean_text;
    QString type_float_text;
    QString type_integer_text;
    QString type_color_text;
    QString type_string_text;
    QString type_password_text;
    QString type_module_text;
    QString type_module_list_text;
    QString type_file_text;
    QString type_directory_text;
    QString type_font_text;
    QString type_unknown_text;
};

#endif
