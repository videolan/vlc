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
#include <QGridLayout>

ConfigControl *ConfigControl::createControl( vlc_object_t *p_this,
                                             module_config_t *p_item,
                                             QWidget *parent )
{
    return createControl( p_this, p_item, parent, NULL, 0 );
}

ConfigControl *ConfigControl::createControl( vlc_object_t *p_this,
                                             module_config_t *p_item,
                                             QWidget *parent,
                                             QGridLayout *l, int line )
{
    ConfigControl *p_control = NULL;
    if( p_item->psz_current ) return NULL;

    switch( p_item->i_type )
    {
    case CONFIG_ITEM_MODULE:
        p_control = new ModuleConfigControl( p_this, p_item, parent, false,
                                             l, line );
        break;
    case CONFIG_ITEM_MODULE_CAT:
        p_control = new ModuleConfigControl( p_this, p_item, parent, true,
                                             l, line );
        break;
    case CONFIG_ITEM_STRING:
        if( !p_item->i_list )
            p_control = new StringConfigControl( p_this, p_item, parent,
                                                 l, line, false );
        else
            fprintf(stderr, "TODO\n" );
        break;
    case CONFIG_ITEM_INTEGER:
        if( p_item->i_list )
            fprintf( stderr, "Todo\n" );
        else if( p_item->i_min || p_item->i_max )
            fprintf( stderr, "Todo\n" );
        else
            p_control = new IntegerConfigControl( p_this, p_item, parent,
                                                  l, line );
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
                                          module_config_t *_p_item,
                                          QWidget *_parent, QGridLayout *l,
                                          int line, bool pwd ) :
                           VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qfu(p_item->psz_text) );
    text = new QLineEdit( qfu(p_item->psz_value) );
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 ); layout->addWidget( text, 1 );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 ); l->addWidget( text, line, 1 );
    }
}

StringConfigControl::StringConfigControl( vlc_object_t *_p_this,
                                   module_config_t *_p_item,
                                   QLabel *_label, QLineEdit *_text, bool pwd ):
                           VStringConfigControl( _p_this, _p_item )
{
    text = _text;
    label = _label;
    finish( );
}

void StringConfigControl::finish()
{
    text->setText( qfu(p_item->psz_value) );
    text->setToolTip( qfu(p_item->psz_longtext) );
    label->setToolTip( qfu(p_item->psz_longtext) );
}

/********* Module **********/
ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent, bool bycat,
               QGridLayout *l, int line) :
               VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qfu(p_item->psz_text) );
    combo = new QComboBox();
    finish( bycat );
    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label ); layout->addWidget( combo );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( combo, line, 1, Qt::AlignRight );
    }
}
ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool bycat ) : VStringConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;
    finish( bycat );
}

void ModuleConfigControl::finish( bool bycat )
{
    vlc_list_t *p_list;
    module_t *p_parser;

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
}

QString ModuleConfigControl::getValue()
{
    return combo->itemData( combo->currentIndex() ).toString();
}

/**************************************************************************
 * Integer-based controls
 *************************************************************************/

/*********** Integer **************/
IntegerConfigControl::IntegerConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QWidget *_parent, QGridLayout *l,
                                            int line ) :
                           VIntConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qfu(p_item->psz_text) );
    spin = new QSpinBox; spin->setMinimumWidth( 80 );
    spin->setMaximumWidth( 90 );
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 ); layout->addWidget( spin, 1 );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 );
        l->addWidget( spin, line, 1, Qt::AlignRight );
    }
}
IntegerConfigControl::IntegerConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QSpinBox *_spin ) :
                                      VIntConfigControl( _p_this, _p_item )
{
    spin = _spin;
    label = _label;
    finish();
}

void IntegerConfigControl::finish()
{
    spin->setMaximum( 2000000000 );
    spin->setValue( p_item->i_value );
    spin->setToolTip( qfu(p_item->psz_longtext) );
    label->setToolTip( qfu(p_item->psz_longtext) );
}

int IntegerConfigControl::getValue()
{
    return spin->value();
}
