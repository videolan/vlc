/*****************************************************************************
 * preferences_widgets.cpp : Widgets for preferences displays
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

/**
 * Todo:
 *  - Finish implementation (see WX)
 *  - Improvements over WX
 *      - Password field implementation (through "pwd" bool param
 *      - Validator for modulelist
 *  - Implement update stuff using a general Updated signal
 */

#include "components/preferences_widgets.hpp"
#include "qt4.hpp"
#include <QLineEdit>
#include <QString>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QVariant>
#include <QComboBox>

ConfigControl::ConfigControl( vlc_object_t *_p_this, module_config_t *p_item,
                              QWidget *_parent ) : QWidget( _parent ),
                              p_this( _p_this ), _name( p_item->psz_name )
{
}

ConfigControl::~ConfigControl() {}

ConfigControl *ConfigControl::createControl( vlc_object_t *p_this,
                                    module_config_t *p_item, QWidget *parent )
{
    ConfigControl *p_control = NULL;
    if( p_item->psz_current ) return NULL;

    switch( p_item->i_type )
    {
    case CONFIG_ITEM_MODULE:
        p_control = new ModuleConfigControl( p_this, p_item, parent, false );
        break;
    case CONFIG_ITEM_MODULE_CAT:
        p_control = new ModuleConfigControl( p_this, p_item, parent, true );
        break;
    case CONFIG_ITEM_STRING:
        if( !p_item->i_list )
            p_control = new StringConfigControl( p_this, p_item, parent,false );
        else
            fprintf(stderr, "TODO\n" );
        break;
    default:
        break;
    }
    return p_control;
}

/**************************************************************************
 * String-based controls
 *************************************************************************/

/*********** String **************/
StringConfigControl::StringConfigControl( vlc_object_t *_p_this,
                     module_config_t *p_item, QWidget *_parent, bool pwd )
                           : VStringConfigControl( _p_this, p_item, _parent )
{
    QLabel *label = new QLabel( qfu(p_item->psz_text) );
    text = new QLineEdit( qfu(p_item->psz_value) );
    text->setToolTip( qfu(p_item->psz_longtext) );
    label->setToolTip( qfu(p_item->psz_longtext) );

    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget( label, 0 ); layout->addWidget( text, 1 );
    setLayout( layout );
}

StringConfigControl::~StringConfigControl() {}

QString StringConfigControl::getValue() { return text->text(); };


/********* Module **********/
ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
                module_config_t *p_item, QWidget *_parent,
                bool bycat ) : VStringConfigControl( _p_this, p_item, _parent )
{
    vlc_list_t *p_list;
    module_t *p_parser;

    QLabel *label = new QLabel( qfu(p_item->psz_text) );
    combo = new QComboBox();
    combo->setEditable( false );

    /* build a list of available modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    combo->addItem( qtr("Default") );
    for( int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( bycat )
        {
            if( !strcmp( p_parser->psz_object_name, "main" ) ) continue;

            module_config_t *p_config = p_parser->p_config;
            if( p_config ) do
            {
                /* Hack: required subcategory is stored in i_min */
                if( p_config->i_type == CONFIG_SUBCATEGORY &&
                    p_config->i_value == p_item->i_min )
                    combo->addItem( qfu(p_parser->psz_longname),
                                    QVariant( p_parser->psz_object_name ) );
                if( p_item->psz_value && !strcmp( p_item->psz_value,
                                                  p_parser->psz_object_name) )
                    combo->setCurrentIndex( combo->count() - 1 );
            } while( p_config->i_type != CONFIG_HINT_END && p_config++ );
        }
        else if( !strcmp( p_parser->psz_capability, p_item->psz_type ) )
        {
            combo->addItem( qfu(p_parser->psz_longname),
                            QVariant( p_parser->psz_object_name ) );
            if( p_item->psz_value && !strcmp( p_item->psz_value,
                                              p_parser->psz_object_name) )
                combo->setCurrentIndex( combo->count() - 1 );
        }
    }
    vlc_list_release( p_list );
    combo->setToolTip( qfu(p_item->psz_longtext) );
    label->setToolTip( qfu(p_item->psz_longtext) );

    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget( label ); layout->addWidget( combo );
    setLayout( layout );
}

ModuleConfigControl::~ModuleConfigControl() {};

QString ModuleConfigControl::getValue()
{
    return combo->itemData( combo->currentIndex() ).toString();
}
