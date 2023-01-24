/*****************************************************************************
 * expert_model.cpp : Detailed preferences overview - model
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QVariant>
#include <QString>
#include <QFont>
#include <QGuiApplication>
#include <QClipboard>
#include <QMenu>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QAction>
#include <QContextMenuEvent>

#include "expert_model.hpp"
#include "preferences_widgets.hpp"

#include <vlc_config_cat.h>
#include <vlc_modules.h>

#define COLUMN_COUNT 4

/*********************************************************************
 * Model Item
 *********************************************************************/
ExpertPrefsTableItem::ExpertPrefsTableItem( module_config_t *config,
                                            const QString &mod_name,
                                            const QString &mod_name_pretty,
                                            bool is_core )
{
    cfg_item = config;

    if( CONFIG_CLASS( getType() ) == CONFIG_ITEM_STRING &&
        config->value.psz != nullptr )
    {
        config->value.psz = strdup( config->value.psz );
    }

    /* Create table entry name from dot-prefixing module name to option name.
       Many plugins dash-prefix their name to their option names, which we must
       skip over here to replace the dash with a dot. */
    QString option_name = config->psz_name;
    int mod_name_len = mod_name.length();
    if( !is_core && option_name.length() > mod_name_len &&
        option_name.startsWith( mod_name ) && option_name[mod_name_len] == '-' )
        option_name.remove( 0, mod_name_len + 1 );
    name = QString( "%1.%2" ).arg( mod_name ).arg( option_name );
    /* Some options use underscores when they shouldn't. Let's not pointlessly
       leak that imperfection into our interface. */
    name.replace( "_", "-" );

    /* Create "title" text label for info panel. */
    title = QString( "%1 :: %2" ).arg( mod_name_pretty ).arg( config->psz_text );
    description = QString(); /* We'll use lazy creation */

    updateMatchesDefault();
    updateValueDisplayString();
}

ExpertPrefsTableItem::~ExpertPrefsTableItem()
{
    if( CONFIG_CLASS( getType() ) == CONFIG_ITEM_STRING )
    {
        free( cfg_item->value.psz );
        cfg_item->value.psz = nullptr;
    }
}

const QString &ExpertPrefsTableItem::getDescription()
{
    if( description.isNull() ) /* Lazy creation */
    {
        if( cfg_item->psz_longtext )
            description = qfut( cfg_item->psz_longtext );
        else if( cfg_item->psz_text )
            description = qfut( cfg_item->psz_text );
        else
            description = QStringLiteral( u"" );
    }
    return description;
}

void ExpertPrefsTableItem::updateMatchesDefault()
{
    switch ( CONFIG_CLASS( getType() ) )
    {
        case CONFIG_ITEM_FLOAT:
            matches_default = (cfg_item->value.f == cfg_item->orig.f);
            break;
        case CONFIG_ITEM_BOOL:
        case CONFIG_ITEM_INTEGER:
            matches_default = (cfg_item->value.i == cfg_item->orig.i);
            break;
        case CONFIG_ITEM_STRING:
        {
            bool orig_is_empty = (cfg_item->orig.psz == nullptr || cfg_item->orig.psz[0] == '\0');
            bool curr_is_empty = (cfg_item->value.psz == nullptr || cfg_item->value.psz[0] == '\0');
            if (orig_is_empty)
                matches_default = curr_is_empty;
            else if( curr_is_empty )
                matches_default = false;
            else
                matches_default = (strcmp( cfg_item->value.psz, cfg_item->orig.psz ) == 0);
            break;
        }
        default:
            vlc_assert_unreachable();
            break;
    }
}

void ExpertPrefsTableItem::updateValueDisplayString()
{
    switch ( CONFIG_CLASS( getType() ) )
    {
        case CONFIG_ITEM_BOOL:
            /* Do nothing - set at the model level to allow reusing cached translation lookup */
            break;
        case CONFIG_ITEM_FLOAT:
            displayed_value = QString( "%L1" ).arg( cfg_item->value.f, 0, 'g' );
            break;
        case CONFIG_ITEM_INTEGER:
            if( cfg_item->i_type == CONFIG_ITEM_RGB )
            {
                QString hex_upper = QString( "%1" ).arg( cfg_item->value.i, 0, 16 ).toUpper();
                displayed_value = QString( "0x" ).append( hex_upper );
            }
            else
                displayed_value = QString( "%L1" ).arg( cfg_item->value.i );
            break;
        case CONFIG_ITEM_STRING:
            if( cfg_item->i_type != CONFIG_ITEM_PASSWORD )
                displayed_value = cfg_item->value.psz;
            else if( cfg_item->value.psz && *cfg_item->value.psz != '\0' )
                displayed_value = QStringLiteral( u"•••••" );
            else
                displayed_value = QStringLiteral( u"" );
            break;
        default:
            break;
    }
}

void ExpertPrefsTableItem::setToDefault()
{
    /* Note, this modifies our local copy of the item only */
    if( CONFIG_CLASS( getType() ) == CONFIG_ITEM_STRING )
    {
        free( cfg_item->value.psz );
        cfg_item->value.psz = (cfg_item->orig.psz) ? strdup( cfg_item->orig.psz ) : nullptr;
    }
    else
        cfg_item->value = cfg_item->orig;

    matches_default = true;
    updateValueDisplayString();
}

void ExpertPrefsTableItem::toggleBoolean()
{
    assert( CONFIG_CLASS( getType() ) == CONFIG_ITEM_BOOL );
    /* Note, this modifies our local copy of the item only */
    cfg_item->value.i = !cfg_item->value.i;
    updateMatchesDefault();
    /* Note, display text is updated by the model for efficiency */
}

/* search name and value columns */
bool ExpertPrefsTableItem::contains( const QString &text, Qt::CaseSensitivity cs )
{
    return ( name.contains( text, cs ) || displayed_value.contains( text, cs ) );
}

/*********************************************************************
 * Model
 *********************************************************************/
ExpertPrefsTableModel::ExpertPrefsTableModel( module_t **mod_list, size_t mod_count,
                                              QWidget *parent_ ) :
    QAbstractListModel( parent_ )
{
    /* Cache translations of common text */
    state_same_text       = qtr( "default" );
    state_different_text  = qtr( "modified" );
    true_text             = qtr( "true" );
    false_text            = qtr( "false" );
    type_boolean_text     = qtr( "boolean" );
    type_float_text       = qtr( "float" );
    type_integer_text     = qtr( "integer" );
    type_color_text       = qtr( "color" );
    type_string_text      = qtr( "string" );
    type_password_text    = qtr( "password" );
    type_module_text      = qtr( "module" );
    type_module_list_text = qtr( "module-list" );
    type_file_text        = qtr( "file" );
    type_directory_text   = qtr( "directory" );
    type_font_text        = qtr( "font" );
    type_unknown_text     = qtr( "unknown" );

    items.reserve( 1400 );
    for( size_t i = 0; i < mod_count; i++ )
    {
        module_t *mod = mod_list[i];

        unsigned confsize;
        module_config_t *const config = module_config_get( mod, &confsize );
        if( confsize == 0 )
        {
            /* If has items but none are visible, we still need to deallocate */
            module_config_free( config );
            continue;
        }
        config_sets.append( config );

        bool is_core = module_is_main( mod );
        QString mod_name = module_get_object( mod );
        QString mod_name_pretty = is_core ? qtr( "Core" ) : module_GetShortName( mod );

        enum vlc_config_subcat subcat = SUBCAT_UNKNOWN;

        for( size_t j = 0; j < confsize; j++ )
        {
            module_config_t *cfg_item = config + j;

            if( cfg_item->i_type == CONFIG_SUBCATEGORY )
            {
                subcat = (enum vlc_config_subcat) cfg_item->value.i;
                continue;
            }
            if( subcat == SUBCAT_UNKNOWN || subcat == SUBCAT_HIDDEN )
                continue;
            /* Exclude hotkey items in favour of them being edited exclusively
               via the dedicated hotkey editor. */
            if( cfg_item->i_type == CONFIG_ITEM_KEY )
                continue;

            if( CONFIG_ITEM( cfg_item->i_type ) )
            {
                ExpertPrefsTableItem *item =
                    new ExpertPrefsTableItem( cfg_item, mod_name, mod_name_pretty, is_core );

                if( CONFIG_CLASS( cfg_item->i_type ) == CONFIG_ITEM_BOOL )
                {
                    /* Set the translated display text from cached lookup */
                    item->displayed_value = ( cfg_item->value.i == 0 ) ? false_text : true_text;
                }

                /* Sorted list insertion */
                int insert_index = 0;
                while( insert_index < items.count() )
                {
                    if( item->name.compare( items[insert_index]->name ) < 0 )
                        break;
                    insert_index++;
                }
                items.insert( insert_index, item );
            }
        }
    }
};

ExpertPrefsTableModel::~ExpertPrefsTableModel()
{
    /* We must destroy the items before releasing the config set */
    foreach ( ExpertPrefsTableItem *item, items )
        delete item;
    items.clear();
    foreach ( module_config_t *config_set, config_sets )
        module_config_free( config_set );
}

QVariant ExpertPrefsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if( orientation == Qt::Horizontal && role == Qt::DisplayRole )
    {
        switch ( section )
        {
            case NameField:  return qtr( "Option" );
            case StateField: return qtr( "Status" );
            case TypeField:  return qtr( "Type" );
            case ValueField: return qtr( "Value" );
        }
    }
    return QVariant();
}

int ExpertPrefsTableModel::rowCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : items.count();
}

int ExpertPrefsTableModel::columnCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

QVariant ExpertPrefsTableModel::data( const QModelIndex &index, int role ) const
{
    ExpertPrefsTableItem *item = itemAt( index );
    switch ( role )
    {
        case Qt::DisplayRole:
        {
            switch ( index.column() )
            {
                case NameField:  return item->name;
                case ValueField: return item->displayed_value;
                case StateField: return (item->matches_default) ? state_same_text
                                                                : state_different_text;
                case TypeField:
                {
                    switch ( item->cfg_item->i_type )
                    {
                        case CONFIG_ITEM_BOOL:             return type_boolean_text;     break;
                        case CONFIG_ITEM_FLOAT:            return type_float_text;       break;
                        case CONFIG_ITEM_INTEGER:          return type_integer_text;     break;
                        case CONFIG_ITEM_RGB:              return type_color_text;       break;
                        case CONFIG_ITEM_STRING:           return type_string_text;      break;
                        case CONFIG_ITEM_PASSWORD:         return type_password_text;    break;
                        case CONFIG_ITEM_MODULE_CAT:
                        case CONFIG_ITEM_MODULE:           return type_module_text;      break;
                        case CONFIG_ITEM_MODULE_LIST_CAT:
                        case CONFIG_ITEM_MODULE_LIST:      return type_module_list_text; break;
                        case CONFIG_ITEM_LOADFILE:
                        case CONFIG_ITEM_SAVEFILE:         return type_file_text;        break;
                        case CONFIG_ITEM_DIRECTORY:        return type_directory_text;   break;
                        case CONFIG_ITEM_FONT:             return type_font_text;        break;
                        default:                           return type_unknown_text;     break;
                    }
                }
            }
            break;
        }
        case Qt::FontRole:
        {
            QFont font = QFont();
            font.setBold( (item->matches_default) ? false : true );
            return font;
        }
        case TypeClassRole:
            return CONFIG_CLASS( item->getType() );
        case CopyValueRole:
        {
            switch ( CONFIG_CLASS( item->getType() ) )
            {
                case CONFIG_ITEM_BOOL:
                    // Note, no translation wanted here!
                    return (item->cfg_item->value.i == 0) ? QStringLiteral( u"false")
                                                          : QStringLiteral( u"true" );
                case CONFIG_ITEM_FLOAT:
                    return QString( "%1" ).arg( item->cfg_item->value.f );
                case CONFIG_ITEM_INTEGER:
                    // For RGB it is presumably more useful to give a hex form that can be
                    // copy-pasted into a colour selection dialog rather than the raw int.
                    if( item->getType() == CONFIG_ITEM_RGB )
                        return QString( "#%1" ).arg( item->cfg_item->value.i, 0, 16 );
                    return QString( "%1" ).arg( item->cfg_item->value.i );
                case CONFIG_ITEM_STRING:
                    return QVariant( qfu(item->cfg_item->value.psz) );
                default:
                    break;
            }
        }
        default: break;
    }
    return QVariant();
}

void ExpertPrefsTableModel::toggleBoolean( const QModelIndex &index )
{
    ExpertPrefsTableItem *item = itemAt( index );
    item->toggleBoolean();
    /* Set the translated display text from cached lookup */
    item->displayed_value = ( item->cfg_item->value.i == 0 ) ? false_text : true_text;
    notifyUpdatedRow( index.row() );
}

void ExpertPrefsTableModel::setItemToDefault( const QModelIndex &index )
{
    ExpertPrefsTableItem *item = itemAt( index );
    item->setToDefault();
    if( CONFIG_CLASS( item->cfg_item->i_type ) == CONFIG_ITEM_BOOL )
    {
        /* Set the translated display text from cached lookup */
        item->displayed_value = ( item->cfg_item->value.i == 0 ) ? false_text : true_text;
    }
    notifyUpdatedRow( index.row() );
}

void ExpertPrefsTableModel::notifyUpdatedRow( int row )
{
    emit dataChanged( index( row, 0 ), index( row, COLUMN_COUNT - 1 ) );
}

bool ExpertPrefsTableModel::submit()
{
    for( int i= 0 ; i < items.count(); i++ )
    {
        ExpertPrefsTableItem *item = items[i];

        /* save from copy to actual */
        switch ( CONFIG_CLASS( item->getType() ) )
        {
            case CONFIG_ITEM_BOOL:
            case CONFIG_ITEM_INTEGER:
                config_PutInt( item->cfg_item->psz_name, item->cfg_item->value.i );
                break;
            case CONFIG_ITEM_FLOAT:
                config_PutFloat( item->cfg_item->psz_name, item->cfg_item->value.f );
                break;
            case CONFIG_ITEM_STRING:
                config_PutPsz( item->cfg_item->psz_name, item->cfg_item->value.psz );
                break;
        }
    }
    return true;
}
