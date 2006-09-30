/*****************************************************************************
 * preferences_widgets.cpp : Widgets for preferences displays
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea@videolan.org>
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
    int i=0;
    return createControl( p_this, p_item, parent, NULL, i );
}

ConfigControl *ConfigControl::createControl( vlc_object_t *p_this,
                                             module_config_t *p_item,
                                             QWidget *parent,
                                             QGridLayout *l, int &line )
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
    case CONFIG_ITEM_MODULE_LIST:
        p_control = new ModuleListConfigControl( p_this, p_item, parent, false,
                                             l, line );
        break;
    case CONFIG_ITEM_MODULE_LIST_CAT:
        p_control = new ModuleListConfigControl( p_this, p_item, parent, true,
                                             l, line );
        break;
    case CONFIG_ITEM_STRING:
        if( !p_item->i_list )
            p_control = new StringConfigControl( p_this, p_item, parent,
                                                 l, line, false );
        else
            p_control = new StringListConfigControl( p_this, p_item,
                                            parent, false, l, line );
        break;
    case CONFIG_ITEM_INTEGER:
        if( p_item->i_list )
            p_control = new IntegerListConfigControl( p_this, p_item,
                                            parent, false, l, line );
        else if( p_item->i_min || p_item->i_max )
            p_control = new IntegerRangeConfigControl( p_this, p_item, parent,
                                                       l, line );
        else
            p_control = new IntegerConfigControl( p_this, p_item, parent,
                                                  l, line );
        break;
    case CONFIG_ITEM_FILE:
        fprintf( stderr, "Todo (CONFIG_ITEM_FILE)\n" );
        break;
    case CONFIG_ITEM_DIRECTORY:
        fprintf( stderr, "Todo (CONFIG_ITEM_DIRECTORY)\n" );
        break;
    case CONFIG_ITEM_BOOL:
        p_control = new BoolConfigControl( p_this, p_item, parent, l, line );
        break;
    case CONFIG_ITEM_FLOAT:
        if( p_item->f_min || p_item->f_max )
            p_control = new FloatRangeConfigControl( p_this, p_item, parent,
                                                     l, line );
        else
            p_control = new FloatConfigControl( p_this, p_item, parent,
                                                  l, line );
        break;
    default:
        break;
    }
    return p_control;
}

void ConfigControl::doApply( intf_thread_t *p_intf )
{
    switch( getType() )
    {
        case 1:
        {
            VIntConfigControl *vicc = qobject_cast<VIntConfigControl *>(this);
            config_PutInt( p_intf, vicc->getName(), vicc->getValue() );
            break;
        }
        case 2:
        {
            VFloatConfigControl *vfcc = 
                                    qobject_cast<VFloatConfigControl *>(this);
            config_PutFloat( p_intf, vfcc->getName(), vfcc->getValue() );
            break;
        }
        case 3:
        {
            VStringConfigControl *vscc =
                            qobject_cast<VStringConfigControl *>(this);
            config_PutPsz( p_intf, vscc->getName(), qta( vscc->getValue() ) );
        }
    }
}

/**************************************************************************
 * String-based controls
 *************************************************************************/

/*********** String **************/
StringConfigControl::StringConfigControl( vlc_object_t *_p_this,
                                          module_config_t *_p_item,
                                          QWidget *_parent, QGridLayout *l,
                                          int &line, bool pwd ) :
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
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

/********* String / choice list **********/
StringListConfigControl::StringListConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent, bool bycat,
               QGridLayout *l, int &line) :
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
StringListConfigControl::StringListConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool bycat ) : VStringConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;
    finish( bycat );
}

void StringListConfigControl::finish( bool bycat )
{
    combo->setEditable( false );

    for( int i_index = 0; i_index < p_item->i_list; i_index++ )
    {
        combo->addItem( qfu(p_item->ppsz_list_text ?
                            p_item->ppsz_list_text[i_index] :
                            p_item->ppsz_list[i_index] ),
                        QVariant( p_item->ppsz_list[i_index] ) );
        if( p_item->psz_value && !strcmp( p_item->psz_value,
                                          p_item->ppsz_list[i_index] ) )
            combo->setCurrentIndex( combo->count() - 1 );
    }
    combo->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

QString StringListConfigControl::getValue()
{
    return combo->itemData( combo->currentIndex() ).toString();
}

/********* Module **********/
ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent, bool bycat,
               QGridLayout *l, int &line) :
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
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

QString ModuleConfigControl::getValue()
{
    return combo->itemData( combo->currentIndex() ).toString();
}

/********* Module list **********/
ModuleListConfigControl::ModuleListConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent, bool bycat,
               QGridLayout *l, int &line) :
               VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qfu(p_item->psz_text) );
    text = new QLineEdit();
    finish( bycat );

    bool pom = false;
    if( !l )
    {
        l = new QGridLayout();
        line = 0;
        pom = true;
    }
    for( QVector<QCheckBox*>::iterator it = modules.begin();
         it != modules.end(); it++ )
    {
        l->addWidget( *it, line++, 1 );
    }
    l->addWidget( label, line, 0 );
    l->addWidget( text, line, 1 );
    if( pom )
        widget->setLayout( l );
}
#if 0
ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool bycat ) : VStringConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;
    finish( bycat );
}
#endif

ModuleListConfigControl::~ModuleListConfigControl()
{
    for( QVector<QCheckBox*>::iterator it = modules.begin();
         it != modules.end(); it++ )
    {
        delete *it;
    }
    delete label;
    delete text;
}

void ModuleListConfigControl::finish( bool bycat )
{
    vlc_list_t *p_list;
    module_t *p_parser;

    /* build a list of available modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
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
                {
                    QCheckBox *cb =
                        new QCheckBox( qfu( p_parser->psz_object_name ) );
                    cb->setToolTip( qfu(p_parser->psz_longname) );
                    modules.push_back( cb );
                }
            } while( p_config->i_type != CONFIG_HINT_END && p_config++ );
        }
        else if( !strcmp( p_parser->psz_capability, p_item->psz_type ) )
        {
            QCheckBox *cb =
                new QCheckBox( qfu( p_parser->psz_object_name ) );
            cb->setToolTip( qfu(p_parser->psz_longname) );
            modules.push_back( cb );
        }
    }
    vlc_list_release( p_list );
    text->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

QString ModuleListConfigControl::getValue()
{
    return text->text();
}

void ModuleListConfigControl::hide()
{
    for( QVector<QCheckBox*>::iterator it = modules.begin();
         it != modules.end(); it++ )
    {
        (*it)->hide();
    }
    text->hide();
    label->hide();
}

void ModuleListConfigControl::show()
{
    for( QVector<QCheckBox*>::iterator it = modules.begin();
         it != modules.end(); it++ )
    {
        (*it)->show();
    }
    text->show();
    label->show();
}


void ModuleListConfigControl::wakeUp_TheUserJustClickedOnSomething( int value )
{
    text->clear();
    for( QVector<QCheckBox*>::iterator it = modules.begin();
         it != modules.end(); it++ )
    {
    }
}

/**************************************************************************
 * Integer-based controls
 *************************************************************************/

/*********** Integer **************/
IntegerConfigControl::IntegerConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QWidget *_parent, QGridLayout *l,
                                            int &line ) :
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
    spin->setMinimum( -2000000000 );
    spin->setValue( p_item->i_value );
    spin->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

int IntegerConfigControl::getValue()
{
    return spin->value();
}

/********* Integer range **********/
IntegerRangeConfigControl::IntegerRangeConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QWidget *_parent, QGridLayout *l,
                                            int &line ) :
            IntegerConfigControl( _p_this, _p_item, _parent, l, line )
{
    finish();
}

IntegerRangeConfigControl::IntegerRangeConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QSpinBox *_spin ) :
            IntegerConfigControl( _p_this, _p_item, _label, _spin )
{
    finish();
}

void IntegerRangeConfigControl::finish()
{
    spin->setMaximum( p_item->i_max );
    spin->setMinimum( p_item->i_min );
}

/********* Integer / choice list **********/
IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *_parent, bool bycat,
               QGridLayout *l, int &line) :
               VIntConfigControl( _p_this, _p_item, _parent )
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
IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool bycat ) : VIntConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;
    finish( bycat );
}

void IntegerListConfigControl::finish( bool bycat )
{
    combo->setEditable( false );

    for( int i_index = 0; i_index < p_item->i_list; i_index++ )
    {
        combo->addItem( qfu(p_item->ppsz_list_text[i_index] ),
                        QVariant( p_item->pi_list[i_index] ) );
        if( p_item->i_value == p_item->pi_list[i_index] )
            combo->setCurrentIndex( combo->count() - 1 );
    }
    combo->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

int IntegerListConfigControl::getValue()
{
    return combo->itemData( combo->currentIndex() ).toInt();
}

/*********** Boolean **************/
BoolConfigControl::BoolConfigControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item,
                                      QWidget *_parent, QGridLayout *l,
                                      int &line ) :
                    VIntConfigControl( _p_this, _p_item, _parent )
{
    checkbox = new QCheckBox( qfu(p_item->psz_text) );
    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( checkbox, 0 );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( checkbox, line, 0 );
    }
}
BoolConfigControl::BoolConfigControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item,
                                      QLabel *_label,
                                      QCheckBox *_checkbox,
                                      bool bycat ) :
                   VIntConfigControl( _p_this, _p_item )
{
    checkbox = _checkbox;
    finish();
}

void BoolConfigControl::finish()
{
    checkbox->setCheckState( p_item->i_value == VLC_TRUE ? Qt::Checked
                                                        : Qt::Unchecked );
    checkbox->setToolTip( qfu(p_item->psz_longtext) );
}

int BoolConfigControl::getValue()
{
    return checkbox->checkState() == Qt::Checked ? VLC_TRUE : VLC_FALSE;
}

/**************************************************************************
 * Float-based controls
 *************************************************************************/

/*********** Float **************/
FloatConfigControl::FloatConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QWidget *_parent, QGridLayout *l,
                                        int &line ) :
                    VFloatConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qfu(p_item->psz_text) );
    spin = new QDoubleSpinBox; spin->setMinimumWidth( 80 );
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

FloatConfigControl::FloatConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QLabel *_label,
                                        QDoubleSpinBox *_spin ) :
                    VFloatConfigControl( _p_this, _p_item )
{
    spin = _spin;
    label = _label;
    finish();
}

void FloatConfigControl::finish()
{
    spin->setMaximum( 2000000000. );
    spin->setMinimum( -2000000000. );
    spin->setSingleStep( 0.1 );
    spin->setValue( (double)p_item->f_value );
    spin->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

float FloatConfigControl::getValue()
{
    return (float)spin->value();
}

/*********** Float with range **************/
FloatRangeConfigControl::FloatRangeConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QWidget *_parent, QGridLayout *l,
                                        int &line ) :
                FloatConfigControl( _p_this, _p_item, _parent, l, line )
{
    finish();
}

FloatRangeConfigControl::FloatRangeConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item,
                                        QLabel *_label,
                                        QDoubleSpinBox *_spin ) :
                FloatConfigControl( _p_this, _p_item, _label, _spin )
{
    finish();
}

void FloatRangeConfigControl::finish()
{
    spin->setMaximum( (double)p_item->f_max );
    spin->setMinimum( (double)p_item->f_min );
}
