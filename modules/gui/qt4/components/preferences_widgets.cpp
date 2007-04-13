/*****************************************************************************
 * preferences_widgets.cpp : Widgets for preferences displays
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
#include "util/customwidgets.hpp"
#include "qt4.hpp"

#include <QLineEdit>
#include <QString>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QVariant>
#include <QComboBox>
#include <QGridLayout>
#include <QPushButton>
#include <QSlider>
#include <QFileDialog>
#include <QFontDialog>

#include <vlc_keys.h>

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
        else if( p_item->min.i || p_item->max.i )
            p_control = new IntegerRangeConfigControl( p_this, p_item, parent,
                                                       l, line );
        else
            p_control = new IntegerConfigControl( p_this, p_item, parent,
                                                  l, line );
        break;
    case CONFIG_ITEM_FILE:
        p_control = new FileConfigControl( p_this, p_item, parent, l,
                                                line, false );
        break;
    case CONFIG_ITEM_DIRECTORY:
        p_control = new DirectoryConfigControl( p_this, p_item, parent, l,
                                                line, false );
        break;
    case CONFIG_ITEM_FONT:
        p_control = new FontConfigControl( p_this, p_item, parent, l,
                                           line, false );
        break;
    case CONFIG_ITEM_KEY:
        p_control = new KeySelectorControl( p_this, p_item, parent, l, line );
        break;
    case CONFIG_ITEM_BOOL:
        p_control = new BoolConfigControl( p_this, p_item, parent, l, line );
        break;
    case CONFIG_ITEM_FLOAT:
        if( p_item->min.f || p_item->max.f )
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
            assert( vicc );
            config_PutInt( p_intf, vicc->getName(), vicc->getValue() );
            break;
        }
        case 2:
        {
            VFloatConfigControl *vfcc =
                                    qobject_cast<VFloatConfigControl *>(this);
            assert( vfcc );
            config_PutFloat( p_intf, vfcc->getName(), vfcc->getValue() );
            break;
        }
        case 3:
        {
            VStringConfigControl *vscc =
                            qobject_cast<VStringConfigControl *>(this);
            assert( vscc );
            config_PutPsz( p_intf, vscc->getName(), qta( vscc->getValue() ) );
            break;
        }
        case 4:
        {
            KeySelectorControl *ksc = qobject_cast<KeySelectorControl *>(this);
            assert( ksc );
            ksc->doApply();
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
    text = new QLineEdit( qfu(p_item->value.psz) );
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
    text->setText( qfu(p_item->value.psz) );
    text->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

/*********** File **************/
FileConfigControl::FileConfigControl( vlc_object_t *_p_this,
                                          module_config_t *_p_item,
                                          QWidget *_parent, QGridLayout *l,
                                          int &line, bool pwd ) :
                           VStringConfigControl( _p_this, _p_item, _parent )
{
    label = new QLabel( qfu(p_item->psz_text) );
    text = new QLineEdit( qfu(p_item->value.psz) );
    browse = new QPushButton( qtr( "Browse..." ) );

    BUTTONACT( browse, updateField() );

    finish();

    if( !l )
    {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->addWidget( label, 0 ); layout->addWidget( text, 1 );
        layout->addWidget( browse, 2 );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0 ); l->addWidget( text, line, 1 );
        l->addWidget( browse, line, 2 );
    }
}


FileConfigControl::FileConfigControl( vlc_object_t *_p_this,
                                   module_config_t *_p_item,
                                   QLabel *_label, QLineEdit *_text,
                                   QPushButton *_button, bool pwd ):
                           VStringConfigControl( _p_this, _p_item )
{
    browse = _button;
    text = _text;
    label = _label;

    BUTTONACT( browse, updateField() );

    finish( );
}

void FileConfigControl::updateField()
{
    QString file = QFileDialog::getOpenFileName( NULL,
                  qtr( "Select File" ), qfu( p_this->p_libvlc->psz_homedir ) );
    if( file.isNull() ) return;
    text->setText( file );
}

void FileConfigControl::finish()
{
    text->setText( qfu(p_item->value.psz) );
    text->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

/********* String / Directory **********/
DirectoryConfigControl::DirectoryConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QWidget *_p_widget,
                        QGridLayout *_p_layout, int& _int, bool _pwd ) :
     FileConfigControl( _p_this, _p_item, _p_widget, _p_layout, _int, _pwd)
{}

DirectoryConfigControl::DirectoryConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QLabel *_p_label,
                        QLineEdit *_p_line, QPushButton *_p_button, bool _pwd ):
     FileConfigControl( _p_this, _p_item, _p_label, _p_line, _p_button, _pwd)
{}

void DirectoryConfigControl::updateField()
{
    QString dir = QFileDialog::getExistingDirectory( NULL,
                      qtr( "Select Directory" ),
                      text->text().isEmpty() ?
                        qfu( p_this->p_libvlc->psz_homedir ) : text->text(),
                      QFileDialog::ShowDirsOnly |
                        QFileDialog::DontResolveSymlinks );
    if( dir.isNull() ) return;
    text->setText( dir );
}

/********* String / Font **********/
FontConfigControl::FontConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QWidget *_p_widget,
                        QGridLayout *_p_layout, int& _int, bool _pwd ) :
     FileConfigControl( _p_this, _p_item, _p_widget, _p_layout, _int, _pwd)
{}

FontConfigControl::FontConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QLabel *_p_label,
                        QLineEdit *_p_line, QPushButton *_p_button, bool _pwd ):
     FileConfigControl( _p_this, _p_item, _p_label, _p_line, _p_button, _pwd)
{}

void FontConfigControl::updateField()
{
    bool ok;
    QFont font = QFontDialog::getFont( &ok, QFont( text->text() ), NULL );
    if( !ok ) return;
    text->setText( font.family() );
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
        if( p_item->value.psz && !strcmp( p_item->value.psz,
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

            for (size_t i = 0; i < p_parser->confsize; i++)
            {
                module_config_t *p_config = p_parser->p_config + i;
                /* Hack: required subcategory is stored in i_min */
                if( p_config->i_type == CONFIG_SUBCATEGORY &&
                    p_config->value.i == p_item->min.i )
                    combo->addItem( qfu(p_parser->psz_longname),
                                    QVariant( p_parser->psz_object_name ) );
                if( p_item->value.psz && !strcmp( p_item->value.psz,
                                                  p_parser->psz_object_name) )
                    combo->setCurrentIndex( combo->count() - 1 );
            }
        }
        else if( !strcmp( p_parser->psz_capability, p_item->psz_type ) )
        {
            combo->addItem( qfu(p_parser->psz_longname),
                            QVariant( p_parser->psz_object_name ) );
            if( p_item->value.psz && !strcmp( p_item->value.psz,
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

            for (size_t i = 0; i < p_parser->confsize; i++)
            {
                module_config_t *p_config = p_parser->p_config + i;
                /* Hack: required subcategory is stored in i_min */
                if( p_config->i_type == CONFIG_SUBCATEGORY &&
                    p_config->value.i == p_item->min.i )
                {
                    QCheckBox *cb =
                        new QCheckBox( qfu( p_parser->psz_object_name ) );
                    cb->setToolTip( qfu(p_parser->psz_longname) );
                    modules.push_back( cb );
                }
            }
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
    spin->setValue( p_item->value.i );
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
    spin->setMaximum( p_item->max.i );
    spin->setMinimum( p_item->min.i );
}

IntegerRangeSliderConfigControl::IntegerRangeSliderConfigControl(
                                            vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QSlider *_slider ):
                    VIntConfigControl( _p_this, _p_item )
{
    slider = _slider;
    label = _label;
    slider->setMaximum( p_item->max.i );
    slider->setMinimum( p_item->min.i );
    slider->setValue( p_item->value.i );
    slider->setToolTip( qfu(p_item->psz_longtext) );
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );
}

int IntegerRangeSliderConfigControl::getValue()
{
        return slider->value();
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
        if( p_item->value.i == p_item->pi_list[i_index] )
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
    checkbox->setCheckState( p_item->value.i == VLC_TRUE ? Qt::Checked
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
    spin->setValue( (double)p_item->value.f );
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
    spin->setMaximum( (double)p_item->max.f );
    spin->setMinimum( (double)p_item->min.f );
}


/**********************************************************************
 * Key selector widget
 **********************************************************************/
KeySelectorControl::KeySelectorControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item,
                                      QWidget *_parent, QGridLayout *l,
                                      int &line ) :
                                ConfigControl( _p_this, _p_item, _parent )

{
    label = new QLabel( qtr("Select an action to change the associated hotkey") );
    table = new QTreeWidget( 0 );
    finish();

    if( !l )
    {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget( label, 0 ); layout->addWidget( table, 1 );
        widget->setLayout( layout );
    }
    else
    {
        l->addWidget( label, line, 0, 1, 2 );
        l->addWidget( table, line+1, 0, 1,2 );
    }
}

void KeySelectorControl::finish()
{
    if( label )
        label->setToolTip( qfu(p_item->psz_longtext) );

    /* Fill the table */
    table->setColumnCount( 2 );
    table->setAlternatingRowColors( true );

    module_t *p_main = config_FindModule( p_this, "main" );
    assert( p_main );

    for (size_t i = 0; i < p_main->confsize; i++)
    {
        module_config_t *p_item = p_main->p_config + i;

        if( p_item->i_type & CONFIG_ITEM && p_item->psz_name &&
            strstr( p_item->psz_name , "key-" ) )
        {
            QTreeWidgetItem *treeItem = new QTreeWidgetItem();
            treeItem->setText( 0, qfu( p_item->psz_text ) );
            treeItem->setText( 1, VLCKeyToString( p_item->value.i ) );
            treeItem->setData( 0, Qt::UserRole,
                                  QVariant::fromValue( (void*)p_item ) );
            values += p_item;
            table->addTopLevelItem( treeItem );
        }
    }
    table->resizeColumnToContents( 0 );

    CONNECT( table, itemDoubleClicked( QTreeWidgetItem *, int ),
             this, selectKey( QTreeWidgetItem * ) );
}

void KeySelectorControl::selectKey( QTreeWidgetItem *keyItem )
{
   module_config_t *p_keyItem = static_cast<module_config_t*>
                          (keyItem->data( 0, Qt::UserRole ).value<void*>());

    KeyInputDialog *d = new KeyInputDialog( values, p_keyItem->psz_text );
    d->exec();
    if( d->result() == QDialog::Accepted )
    {
        p_keyItem->value.i = d->keyValue;
        if( d->conflicts )
        {
            for( int i = 0; i < table->topLevelItemCount() ; i++ )
            {
                QTreeWidgetItem *it = table->topLevelItem(i);
                module_config_t *p_item = static_cast<module_config_t*>
                              (it->data( 0, Qt::UserRole ).value<void*>());
                it->setText( 1, VLCKeyToString( p_item->value.i ) );
            }
        }
        else
            keyItem->setText( 1, VLCKeyToString( p_keyItem->value.i ) );
    }
    delete d;
}

void KeySelectorControl::doApply()
{
    foreach( module_config_t *p_current, values )
    {
        config_PutInt( p_this, p_current->psz_name, p_current->value.i );
    }
}

KeyInputDialog::KeyInputDialog( QList<module_config_t*>& _values,
                                const char * _keyToChange ) :
                                                QDialog(0), keyValue(0)
{
    setModal( true );
    values = _values;
    conflicts = false;
    keyToChange = _keyToChange;
    setWindowTitle( qtr( "Hotkey for " ) + qfu( keyToChange)  );

    QVBoxLayout *l = new QVBoxLayout( this );
    selected = new QLabel( qtr("Press the new keys for ")  + qfu(keyToChange) );
    warning = new QLabel();
    l->addWidget( selected , Qt::AlignCenter );
    l->addWidget( warning, Qt::AlignCenter );

    QHBoxLayout *l2 = new QHBoxLayout();
    QPushButton *ok = new QPushButton( qtr("OK") );
    l2->addWidget( ok );
    QPushButton *cancel = new QPushButton( qtr("Cancel") );
    l2->addWidget( cancel );

    BUTTONACT( ok, accept() );
    BUTTONACT( cancel, reject() );

    l->addLayout( l2 );
}

void KeyInputDialog::checkForConflicts( int i_vlckey )
{
    conflicts = false;
    module_config_t *p_current = NULL;
    foreach( p_current, values )
    {
        if( p_current->value.i == i_vlckey && strcmp( p_current->psz_text,
                                                    keyToChange ) )
        {
            p_current->value.i = 0;
            conflicts = true;
            break;
        }
    }
    if( conflicts )
    {
        warning->setText(
          qtr("Warning: the  key is already assigned to \"") +
          QString( p_current->psz_text ) + "\"" );
    }
    else warning->setText( "" );
}

void KeyInputDialog::keyPressEvent( QKeyEvent *e )
{
    if( e->key() == Qt::Key_Tab ) return;
    int i_vlck = qtEventToVLCKey( e );
    selected->setText( VLCKeyToString( i_vlck ) );
    checkForConflicts( i_vlck );
    keyValue = i_vlck;
}

void KeyInputDialog::wheelEvent( QWheelEvent *e )
{
    int i_vlck = qtWheelEventToVLCKey( e );
    selected->setText( VLCKeyToString( i_vlck ) );
    checkForConflicts( i_vlck );
    keyValue = i_vlck;
}
