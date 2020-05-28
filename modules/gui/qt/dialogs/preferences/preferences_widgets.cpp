/*****************************************************************************
 * preferences_widgets.cpp : Widgets for preferences displays
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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
 *  - Validator for modulelist
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/preferences/preferences_widgets.hpp"
#include "widgets/native/customwidgets.hpp"
#include "widgets/native/searchlineedit.hpp"
#include "util/qt_dirs.hpp"
#include <vlc_actions.h>
#include <vlc_intf_strings.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

#include <QString>
#include <QVariant>
#include <QGridLayout>
#include <QSlider>
#include <QFileDialog>
#include <QGroupBox>
#include <QTreeWidgetItem>
#include <QSignalMapper>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QColorDialog>
#include <QAction>
#include <QKeySequence>

#define MINWIDTH_BOX 90
#define LAST_COLUMN 10

QString formatTooltip(const QString & tooltip)
{
    QString text = tooltip;
    text.replace("\n", "<br/>");

    QString formatted =
    "<html><head><meta name=\"qrichtext\" content=\"1\" />"
    "<style type=\"text/css\"> p, li { white-space: pre-wrap; } </style></head>"
    "<body style=\" font-family:'Sans Serif'; "
    "font-style:normal; text-decoration:none;\">"
    "<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; "
    "margin-right:0px; -qt-block-indent:0; text-indent:0px;\">" +
    text + "</p></body></html>";
    return formatted;
}

ConfigControl *ConfigControl::createControl( vlc_object_t *p_this,
                                             module_config_t *p_item,
                                             QWidget *parent,
                                             QGridLayout *l, int line )
{
    ConfigControl *p_control = NULL;

    switch( p_item->i_type )
    {
    case CONFIG_ITEM_MODULE:
        p_control = new StringListConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_MODULE_CAT:
        p_control = new ModuleConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_MODULE_LIST:
        p_control = new ModuleListConfigControl( p_this, p_item, parent, false );
        break;
    case CONFIG_ITEM_MODULE_LIST_CAT:
        p_control = new ModuleListConfigControl( p_this, p_item, parent, true );
        break;
    case CONFIG_ITEM_STRING:
        if( p_item->list_count )
            p_control = new StringListConfigControl( p_this, p_item, parent );
        else
            p_control = new StringConfigControl( p_this, p_item, parent, false );
        break;
    case CONFIG_ITEM_PASSWORD:
        p_control = new StringConfigControl( p_this, p_item, parent, true );
        break;
    case CONFIG_ITEM_RGB:
        p_control = new ColorConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_INTEGER:
        if( p_item->list_count )
            p_control = new IntegerListConfigControl( p_this, p_item, parent, false );
        else
            p_control = new IntegerRangeConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_LOADFILE:
    case CONFIG_ITEM_SAVEFILE:
        p_control = new FileConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_DIRECTORY:
        p_control = new DirectoryConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_FONT:
        p_control = new FontConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_KEY:
        p_control = new KeySelectorControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_BOOL:
        p_control = new BoolConfigControl( p_this, p_item, parent );
        break;
    case CONFIG_ITEM_FLOAT:
        p_control = new FloatRangeConfigControl( p_this, p_item, parent );
        break;
    default:
        break;
    }
    if ( p_control ) p_control->insertIntoExistingGrid( l, line );
    return p_control;
}

/* Inserts controls into layouts
   This is sub-optimal in the OO way, as controls's code still
   depends on Layout classes. We should use layout inserters [friend]
   classes, but it's unlikely we had to deal with a different layout.*/
void ConfigControl::insertInto( QBoxLayout *layout )
{
    QGridLayout *sublayout = new QGridLayout();
    fillGrid( sublayout, 0 );
    layout->addLayout( sublayout );
}

void ConfigControl::insertIntoExistingGrid( QGridLayout *l, int line )
{
    fillGrid( l, line );
}

/*******************************************************
 * Simple widgets
 *******************************************************/
InterfacePreviewWidget::InterfacePreviewWidget ( QWidget *parent ) : QLabel( parent )
{
    setGeometry( 0, 0, 128, 100 );
    setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
}

void InterfacePreviewWidget::setNormalPreview( bool b_minimal )
{
    setPreview( ( b_minimal ) ? MINIMAL : COMPLETE );
}

void InterfacePreviewWidget::setPreview( enum_style e_style )
{
    QString pixmapLocationString;

    switch( e_style )
    {
    default:
    case COMPLETE:
        pixmapLocationString = ":/prefsmenu/sample_complete.png";
        break;
    case MINIMAL:
        pixmapLocationString = ":/prefsmenu/sample_minimal.png";
        break;
    case SKINS:
        pixmapLocationString = ":/prefsmenu/sample_skins.png";
        break;
    }

    setPixmap( QPixmap( pixmapLocationString ).
               scaledToWidth( width(), Qt::SmoothTransformation ) );
    update();
}


/**************************************************************************
 * String-based controls
 *************************************************************************/

void
VStringConfigControl::doApply()
{
    config_PutPsz( getName(), qtu( getValue() ) );
}

/*********** String **************/
StringConfigControl::StringConfigControl( vlc_object_t *_p_this,
                                          module_config_t *_p_item,
                                          QWidget *_parent,
                                          bool pwd ) :
                           VStringConfigControl( _p_this, _p_item )
{
    label = new QLabel( p_item->psz_text ? qtr(p_item->psz_text) : "", _parent );
    text = new QLineEdit( p_item->value.psz ? qfu(p_item->value.psz) : "", _parent );
    if( pwd ) text->setEchoMode( QLineEdit::Password );
    finish();
}

void StringConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->setColumnMinimumWidth( 1, 10 );
    l->addWidget( text, line, LAST_COLUMN, Qt::AlignRight );
}

StringConfigControl::StringConfigControl( vlc_object_t *_p_this,
                                   module_config_t *_p_item,
                                   QLabel *_label, QLineEdit *_text, bool pwd ):
                           VStringConfigControl( _p_this, _p_item )
{
    text = _text;
    if( pwd ) text->setEchoMode( QLineEdit::Password );
    label = _label;
    finish( );
}

int VStringConfigControl::getType() const { return CONFIG_ITEM_STRING; }

void StringConfigControl::finish()
{
    text->setText( qfu(p_item->value.psz) );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        text->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( text );
}

/*********** File **************/
FileConfigControl::FileConfigControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item, QWidget *p ) :
                           VStringConfigControl( _p_this, _p_item )
{
    label = new QLabel( qtr(p_item->psz_text), p );
    text = new QLineEdit( qfu(p_item->value.psz), p );
    browse = new QPushButton( qtr( "Browse..." ), p );

    BUTTONACT( browse, updateField() );

    finish();
}

void FileConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->setColumnMinimumWidth( 1, 10 );
    QHBoxLayout *textAndButton = new QHBoxLayout();
    textAndButton->setMargin( 0 );
    textAndButton->addWidget( text, 2 );
    textAndButton->addWidget( browse, 0 );
    l->addLayout( textAndButton, line, LAST_COLUMN );
}

FileConfigControl::FileConfigControl( vlc_object_t *_p_this,
                                   module_config_t *_p_item,
                                   QLabel *_label, QLineEdit *_text,
                                   QPushButton *_button ):
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
    QString file;

    if (p_item->i_type == CONFIG_ITEM_SAVEFILE)
        file = QFileDialog::getSaveFileName( NULL, qtr( "Save File" ),
                                             QVLCUserDir( VLC_HOME_DIR ) );
    else
        file = QFileDialog::getOpenFileName( NULL, qtr( "Select File" ),
                                             QVLCUserDir( VLC_HOME_DIR ) );

    if( file.isNull() ) return;
    text->setText( toNativeSeparators( file ) );
}

void FileConfigControl::finish()
{
    text->setText( qfu(p_item->value.psz) );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        text->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( text );
}

/********* String / Directory **********/
DirectoryConfigControl::DirectoryConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QWidget *p ) :
     FileConfigControl( _p_this, _p_item, p )
{}

DirectoryConfigControl::DirectoryConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QLabel *_p_label,
                        QLineEdit *_p_line, QPushButton *_p_button ):
     FileConfigControl( _p_this, _p_item, _p_label, _p_line, _p_button)
{}

void DirectoryConfigControl::updateField()
{
    QString dir = QFileDialog::getExistingDirectory( NULL,
                      qtr( I_OP_SEL_DIR ),
                      text->text().isEmpty() ?
                        QVLCUserDir( VLC_HOME_DIR ) : text->text(),
                  QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );

    if( dir.isNull() ) return;
    text->setText( toNativeSepNoSlash( dir ) );
}

/********* String / Font **********/
FontConfigControl::FontConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QWidget *p ) :
     VStringConfigControl( _p_this, _p_item )
{
    label = new QLabel( qtr(p_item->psz_text), p );
    font = new QFontComboBox( p );
    font->setCurrentFont( QFont( qfu( p_item->value.psz) ) );

    if( p_item->psz_longtext )
    {
        label->setToolTip( formatTooltip( qtr(p_item->psz_longtext) ) );
    }
}

void FontConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->addWidget( font, line, 1, 1, -1 );
}

FontConfigControl::FontConfigControl( vlc_object_t *_p_this,
                        module_config_t *_p_item, QLabel *_p_label,
                        QFontComboBox *_p_font):
     VStringConfigControl( _p_this, _p_item)
{
    label = _p_label;
    font = _p_font;
    font->setCurrentFont( QFont( qfu( p_item->value.psz) ) );

    if( p_item->psz_longtext )
    {
        label->setToolTip( formatTooltip( qtr(p_item->psz_longtext) ) );
    }
}

/********* String / choice list **********/
StringListConfigControl::StringListConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *p ) :
               VStringConfigControl( _p_this, _p_item )
{
    label = new QLabel( qtr(p_item->psz_text), p );
    combo = new QComboBox( p );
    combo->setMinimumWidth( MINWIDTH_BOX );
    combo->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Preferred );

    module_config_t *p_module_config = config_FindConfig( p_item->psz_name );

    finish( p_module_config );
}

void StringListConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->addWidget( combo, line, LAST_COLUMN, Qt::AlignRight );
    int i = 0;
    foreach( QPushButton *button, buttons )
        l->addWidget( button, line, LAST_COLUMN + i++,
                      Qt::AlignRight );
}

void StringListConfigControl::comboIndexChanged( int i_index )
{
    Q_UNUSED( i_index );
    emit changed();
}

StringListConfigControl::StringListConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool ) : VStringConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;

    module_config_t *p_module_config = config_FindConfig( getName() );

    finish( p_module_config );
}

void StringListConfigControl::finish(module_config_t *p_module_config )
{
    combo->setEditable( false );
    CONNECT( combo, currentIndexChanged ( int ), this, comboIndexChanged( int ) );

    if(!p_module_config) return;

    char **values, **texts;
    ssize_t count = config_GetPszChoices( p_item->psz_name, &values, &texts );
    for( ssize_t i = 0; i < count && texts; i++ )
    {
        if( texts[i] == NULL || values[i] == NULL )
            continue;

        combo->addItem( qfu(texts[i]), QVariant( qfu(values[i])) );
        if( !strcmp( p_item->value.psz ? p_item->value.psz : "", values[i] ) )
            combo->setCurrentIndex( combo->count() - 1 );
        free( texts[i] );
        free( values[i] );
    }
    free( texts );
    free( values );

    if( p_module_config->psz_longtext  )
    {
        QString tipText = qtr(p_module_config->psz_longtext);
        combo->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( combo );
}

QString StringListConfigControl::getValue() const
{
    return combo->itemData( combo->currentIndex() ).toString();
}

void setfillVLCConfigCombo( const char *configname, QComboBox *combo )
{
    module_config_t *p_config = config_FindConfig( configname );
    if( p_config == NULL )
        return;

    if( (p_config->i_type & 0xF0) == CONFIG_ITEM_STRING )
    {
        char **values, **texts;
        ssize_t count = config_GetPszChoices(configname, &values, &texts);
        for( ssize_t i = 0; i < count; i++ )
        {
            combo->addItem( qtr(texts[i]), QVariant(qfu(values[i])) );
            if( p_config->value.psz && !strcmp(p_config->value.psz, values[i]) )
                combo->setCurrentIndex( i );
            free( texts[i] );
            free( values[i] );
        }
        free( texts );
        free( values );
    }
    else
    {
        int64_t *values;
        char **texts;
        ssize_t count = config_GetIntChoices(configname, &values, &texts);
        for( ssize_t i = 0; i < count; i++ )
        {
            combo->addItem( qtr(texts[i]), QVariant(qlonglong(values[i])) );
            if( p_config->value.i == values[i] )
                combo->setCurrentIndex( i );
            free( texts[i] );
        }
        free( texts );
        free( values );
    }

    if( p_config->psz_longtext != NULL )
        combo->setToolTip( qfu( p_config->psz_longtext ) );
}

/********* Module **********/
ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *p ) :
               VStringConfigControl( _p_this, _p_item )
{
    label = new QLabel( qtr(p_item->psz_text), p );
    combo = new QComboBox( p );
    combo->setMinimumWidth( MINWIDTH_BOX );
    finish( );
}

void ModuleConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->addWidget( combo, line, LAST_COLUMN );
}

ModuleConfigControl::ModuleConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo )
                : VStringConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;
    finish( );
}

void ModuleConfigControl::finish( )
{
    combo->setEditable( false );

    /* build a list of available modules */
    size_t count;
    module_t **p_list = module_list_get( &count );
    combo->addItem( qtr("Default") );
    for( size_t i = 0; i < count; i++ )
    {
        module_t *p_parser = p_list[i];

        if( !strcmp( module_get_object( p_parser ), "core" ) ) continue;

        unsigned confsize;
        module_config_t *p_config;

        p_config = module_config_get (p_parser, &confsize);
        for (size_t i = 0; i < confsize; i++)
        {
            /* Hack: required subcategory is stored in i_min */
            const module_config_t *p_cfg = p_config + i;
            if( p_cfg->i_type == CONFIG_SUBCATEGORY &&
                p_cfg->value.i == p_item->min.i )
                combo->addItem( qtr( module_GetLongName( p_parser )),
                                QVariant( module_get_object( p_parser ) ) );
            if( p_item->value.psz && !strcmp( p_item->value.psz,
                                              module_get_object( p_parser ) ) )
                combo->setCurrentIndex( combo->count() - 1 );
        }
        module_config_free (p_config);
    }
    module_list_free( p_list );

    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        combo->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( combo );
}

QString ModuleConfigControl::getValue() const
{
    return combo->itemData( combo->currentIndex() ).toString();
}

/********* Module list **********/
ModuleListConfigControl::ModuleListConfigControl( vlc_object_t *_p_this,
        module_config_t *_p_item, QWidget *p, bool bycat ) :
    VStringConfigControl( _p_this, _p_item )
{
    groupBox = NULL;

    /* Special Hack */
    if( !p_item->psz_text ) return;

    groupBox = new QGroupBox ( qtr(p_item->psz_text), p );
    text = new QLineEdit( p );
    QGridLayout *layoutGroupBox = new QGridLayout( groupBox );

    finish( bycat );

    int boxline = 0;
    foreach ( checkBoxListItem *it, modules )
    {
        layoutGroupBox->addWidget( it->checkBox, boxline / 2, boxline % 2 );
        boxline++;
    }

    layoutGroupBox->addWidget( text, boxline, 0, 1, 2 );

    if( p_item->psz_longtext )
        text->setToolTip( formatTooltip( qtr( p_item->psz_longtext) ) );
}

void ModuleListConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( groupBox, line, 0, 1, -1 );
}

ModuleListConfigControl::~ModuleListConfigControl()
{
    foreach ( checkBoxListItem *it, modules )
        free( it->psz_module );
    qDeleteAll( modules );
    modules.clear();
    delete groupBox;
}

void ModuleListConfigControl::checkbox_lists( module_t *p_parser )
{
    const char *help = module_get_help( p_parser );
    checkbox_lists( qtr( module_GetLongName( p_parser ) ),
                    help != NULL ? qtr( help ): "",
                    module_get_object( p_parser ) );
}

void ModuleListConfigControl::checkbox_lists( QString label, QString help, const char* psz_module )
{
    QCheckBox *cb = new QCheckBox( label );
    checkBoxListItem *cbl = new checkBoxListItem;

    CONNECT( cb, stateChanged( int ), this, onUpdate() );
    if( !help.isEmpty() )
        cb->setToolTip( formatTooltip( help ) );
    cbl->checkBox = cb;

    cbl->psz_module = strdup( psz_module );
    modules.append( cbl );

    if( p_item->value.psz && strstr( p_item->value.psz, cbl->psz_module ) )
        cbl->checkBox->setChecked( true );
}

void ModuleListConfigControl::finish( bool bycat )
{
    /* build a list of available modules */
    size_t count;
    module_t **p_list = module_list_get( &count );
    for( size_t i = 0; i < count; i++ )
    {
        module_t *p_parser = p_list[i];

        if( bycat )
        {
            if( !strcmp( module_get_object( p_parser ), "core" ) ) continue;

            unsigned confsize;
            module_config_t *p_config = module_config_get (p_parser, &confsize);

            for (size_t i = 0; i < confsize; i++)
            {
                module_config_t *p_cfg = p_config + i;
                /* Hack: required subcategory is stored in i_min */
                if( p_cfg->i_type == CONFIG_SUBCATEGORY &&
                        p_cfg->value.i == p_item->min.i )
                {
                    checkbox_lists( p_parser );
                }

                /* Parental Advisory HACK:
                 * Selecting HTTP, RC and Telnet interfaces is difficult now
                 * since they are just the lua interface module */
                if( p_cfg->i_type == CONFIG_SUBCATEGORY &&
                    !strcmp( module_get_object( p_parser ), "lua" ) &&
                    !strcmp( p_item->psz_name, "extraintf" ) &&
                    p_cfg->value.i == p_item->min.i )
                {
                    checkbox_lists( "Web", "Lua HTTP", "http" );
                    checkbox_lists( "Telnet", "Lua Telnet", "telnet" );
#ifndef _WIN32
                    checkbox_lists( "Console", "Lua CLI", "cli" );
#endif
                }
            }
            module_config_free (p_config);
        }
        else if( module_provides( p_parser, p_item->psz_type ) )
        {
            checkbox_lists(p_parser);
        }
    }
    module_list_free( p_list );

    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);

        text->setToolTip( formatTooltip(tipText) );
        assert( groupBox );
        groupBox->setToolTip( formatTooltip(tipText) );
   }
}

QString ModuleListConfigControl::getValue() const
{
    assert( text );
    return text->text();
}

void ModuleListConfigControl::changeVisibility( bool b )
{
    foreach ( checkBoxListItem *it, modules )
        it->checkBox->setVisible( b );
    groupBox->setVisible( b );
}

void ModuleListConfigControl::onUpdate()
{
    text->clear();
    bool first = true;

    foreach ( checkBoxListItem *it, modules )
    {
        if( it->checkBox->isChecked() )
        {
            if( first )
            {
                text->setText( text->text() + it->psz_module );
                first = false;
            }
            else
            {
                text->setText( text->text() + ":" + it->psz_module );
            }
        }
    }
}

/**************************************************************************
 * Integer-based controls
 *************************************************************************/

void
VIntConfigControl::doApply()
{
    config_PutInt( getName(), getValue() );
}

/*********** Integer **************/
IntegerConfigControl::IntegerConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item, QWidget *p ) :
                           VIntConfigControl( _p_this, _p_item )
{
    label = new QLabel( qtr(p_item->psz_text), p );
    spin = new QSpinBox( p ); spin->setMinimumWidth( MINWIDTH_BOX );
    spin->setAlignment( Qt::AlignRight );
    spin->setMaximumWidth( MINWIDTH_BOX );
    finish();
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

void IntegerConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->addWidget( spin, line, LAST_COLUMN, Qt::AlignRight );
}

void IntegerConfigControl::finish()
{
    spin->setMaximum( 2000000000 );
    spin->setMinimum( -2000000000 );
    spin->setValue( p_item->value.i );

    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        spin->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( spin );
}

int IntegerConfigControl::getValue() const
{
    return spin->value();
}

int VIntConfigControl::getType() const { return CONFIG_ITEM_INTEGER; }

/********* Integer range **********/
IntegerRangeConfigControl::IntegerRangeConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item, QWidget *p ) :
            IntegerConfigControl( _p_this, _p_item, p )
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
    spin->setMaximum( p_item->max.i > INT_MAX ? INT_MAX : p_item->max.i );
    spin->setMinimum( p_item->min.i < INT_MIN ? INT_MIN : p_item->min.i );
}

IntegerRangeSliderConfigControl::IntegerRangeSliderConfigControl(
                                            vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QSlider *_slider ):
                    VIntConfigControl( _p_this, _p_item )
{
    slider = _slider;
    label = _label;
    slider->setMaximum( p_item->max.i > INT_MAX ? INT_MAX : p_item->max.i );
    slider->setMinimum( p_item->min.i < INT_MIN ? INT_MIN : p_item->min.i );
    slider->setValue( p_item->value.i );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        slider->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( slider );
}

int IntegerRangeSliderConfigControl::getValue() const
{
        return slider->value();
}


/********* Integer / choice list **********/
IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *_p_this,
               module_config_t *_p_item, QWidget *p, bool ) :
               VIntConfigControl( _p_this, _p_item )
{
    label = new QLabel( qtr(p_item->psz_text), p );
    combo = new QComboBox( p );
    combo->setMinimumWidth( MINWIDTH_BOX );

    module_config_t *p_module_config = config_FindConfig( p_item->psz_name );

    finish( p_module_config );
}

void IntegerListConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->addWidget( combo, line, LAST_COLUMN, Qt::AlignRight );
    int i = 0;
    foreach( QPushButton *button, buttons )
        l->addWidget( button, line, LAST_COLUMN + i++,
                      Qt::AlignRight );
}

IntegerListConfigControl::IntegerListConfigControl( vlc_object_t *_p_this,
                module_config_t *_p_item, QLabel *_label, QComboBox *_combo,
                bool ) : VIntConfigControl( _p_this, _p_item )
{
    combo = _combo;
    label = _label;

    module_config_t *p_module_config = config_FindConfig( getName() );

    finish( p_module_config );
}

void IntegerListConfigControl::finish(module_config_t *p_module_config )
{
    combo->setEditable( false );

    if(!p_module_config) return;

    int64_t *values;
    char **texts;
    ssize_t count = config_GetIntChoices( p_module_config->psz_name,
                                          &values, &texts );
    for( ssize_t i = 0; i < count; i++ )
    {
        combo->addItem( qtr(texts[i]), qlonglong(values[i]) );
        if( p_module_config->value.i == values[i] )
            combo->setCurrentIndex( combo->count() - 1 );
        free( texts[i] );
    }
    free( texts );
    free( values );
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext );
        combo->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( combo );
}

int IntegerListConfigControl::getValue() const
{
    return combo->itemData( combo->currentIndex() ).toInt();
}

/*********** Boolean **************/
BoolConfigControl::BoolConfigControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item, QWidget *p ) :
                    VIntConfigControl( _p_this, _p_item )
{
    checkbox = new QCheckBox( qtr(p_item->psz_text), p );
    finish();
}

void BoolConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( checkbox, line, 0, 1, -1 );
}

BoolConfigControl::BoolConfigControl( vlc_object_t *_p_this,
                                      module_config_t *_p_item,
                                      QLabel *_label,
                                      QAbstractButton *_checkbox ) :
                   VIntConfigControl( _p_this, _p_item )
{
    checkbox = _checkbox;
    VLC_UNUSED( _label );
    finish();
}

int BoolConfigControl::getType() const { return CONFIG_ITEM_BOOL; }

void BoolConfigControl::finish()
{
    checkbox->setChecked( p_item->value.i );
    if( p_item->psz_longtext )
        checkbox->setToolTip( formatTooltip(qtr(p_item->psz_longtext)) );
}

int BoolConfigControl::getValue() const
{
    return checkbox->isChecked();
}

/************* Color *************/
ColorConfigControl::ColorConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item, QWidget *p ) :
                           VIntConfigControl( _p_this, _p_item )
{
    label = new QLabel( p );
    color_but = new QToolButton( p );
    finish();
}

void ColorConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->addWidget( color_but, line, LAST_COLUMN, Qt::AlignRight );
}

ColorConfigControl::ColorConfigControl( vlc_object_t *_p_this,
                                            module_config_t *_p_item,
                                            QLabel *_label, QAbstractButton *_color ):
                                      VIntConfigControl( _p_this, _p_item )
{
    label = _label;
    color_but = _color;
    finish();
}

void ColorConfigControl::finish()
{
    i_color = p_item->value.i;

    color_px = new QPixmap( 34, 20 );
    color_px->fill( QColor( i_color ) );
    color_but->setIcon( QIcon( *color_px ) );
    color_but->setMinimumWidth( 40 );

    label->setText( qtr(p_item->psz_text) );
    if( p_item->psz_longtext )
    {
        label->setToolTip( formatTooltip(qtr(p_item->psz_longtext)) );
        color_but->setToolTip( formatTooltip(qtr(p_item->psz_longtext)) );
    }

    BUTTONACT( color_but, selectColor() );
}

int ColorConfigControl::getValue() const
{
    return i_color;
}

void ColorConfigControl::selectColor()
{
    QColor color = QColorDialog::getColor( QColor( i_color ) );
    if( color.isValid() )
    {
        i_color = (color.red() << 16) + (color.green() << 8) + color.blue();

        color_px->fill( QColor( i_color ) );
        color_but->setIcon( QIcon( *color_px ) );
    }
}


/**************************************************************************
 * Float-based controls
 *************************************************************************/

void
VFloatConfigControl::doApply()
{
    config_PutFloat( getName(), getValue() );
}

/*********** Float **************/
FloatConfigControl::FloatConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item, QWidget *p ) :
                    VFloatConfigControl( _p_this, _p_item )
{
    label = new QLabel( qtr(p_item->psz_text), p );
    spin = new QDoubleSpinBox( p );
    spin->setMinimumWidth( MINWIDTH_BOX );
    spin->setMaximumWidth( MINWIDTH_BOX );
    spin->setAlignment( Qt::AlignRight );
    finish();
}

void FloatConfigControl::fillGrid( QGridLayout *l, int line )
{
    l->addWidget( label, line, 0 );
    l->addWidget( spin, line, LAST_COLUMN, Qt::AlignRight );
}

int VFloatConfigControl::getType() const { return CONFIG_ITEM_FLOAT; }

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
    if( p_item->psz_longtext )
    {
        QString tipText = qtr(p_item->psz_longtext);
        spin->setToolTip( formatTooltip(tipText) );
        if( label )
            label->setToolTip( formatTooltip(tipText) );
    }
    if( label )
        label->setBuddy( spin );
}

float FloatConfigControl::getValue() const
{
    return (float)spin->value();
}

/*********** Float with range **************/
FloatRangeConfigControl::FloatRangeConfigControl( vlc_object_t *_p_this,
                                        module_config_t *_p_item, QWidget *p ) :
                FloatConfigControl( _p_this, _p_item, p )
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
                                      module_config_t *_p_item, QWidget *p ) :
                                ConfigControl( _p_this, _p_item )

{
    label = new QLabel(
        qtr( "Select or double click an action to change the associated "
             "hotkey. Use delete key to remove hotkeys."), p );

    label->setWordWrap( true );
    searchLabel = new QLabel( qtr( "Search" ), p );
    actionSearch = new SearchLineEdit();

    searchOptionLabel = new QLabel( qtr("in") );
    searchOption = new QComboBox();
    searchOption->addItem( qtr("Any field"), ANY_COL );
    searchOption->addItem( qtr("Actions"), ACTION_COL );
    searchOption->addItem( qtr("Hotkeys"), HOTKEY_COL );
    searchOption->addItem( qtr("Global Hotkeys"), GLOBAL_HOTKEY_COL );

    table = new QTreeWidget( p );
    table->setColumnCount( ANY_COL );
    table->headerItem()->setText( ACTION_COL, qtr( "Action" ) );
    table->headerItem()->setText( HOTKEY_COL, qtr( "Hotkey" ) );
    table->headerItem()->setToolTip( HOTKEY_COL, qtr( "Application level hotkey" ) );
    table->headerItem()->setText( GLOBAL_HOTKEY_COL, qtr( "Global" ) );
    table->headerItem()->setToolTip( GLOBAL_HOTKEY_COL, qtr( "Desktop level hotkey" ) );
    table->setAlternatingRowColors( true );
    table->setSelectionBehavior( QAbstractItemView::SelectItems );

    table->installEventFilter( this );

    /* Find the top most widget */
    QWidget *parent, *rootWidget = p;
    while( (parent = rootWidget->parentWidget()) != NULL )
        rootWidget = parent;
    buildAppHotkeysList( rootWidget );

    finish();

    CONNECT( actionSearch, textChanged( const QString& ),
             this, filter( const QString& ) );
}

void KeySelectorControl::fillGrid( QGridLayout *l, int line )
{
    QGridLayout *gLayout = new QGridLayout();
    gLayout->addWidget( label, 0, 0, 1, 5 );
    gLayout->addWidget( searchLabel, 1, 0, 1, 2 );
    gLayout->addWidget( actionSearch, 1, 2, 1, 1 );
    gLayout->addWidget( searchOptionLabel, 1, 3, 1, 1 );
    gLayout->addWidget( searchOption, 1, 4, 1, 1 );
    gLayout->addWidget( table, 2, 0, 1, 5 );
    l->addLayout( gLayout, line, 0, 1, -1 );
}

int KeySelectorControl::getType() const { return CONFIG_ITEM_KEY; }

void KeySelectorControl::buildAppHotkeysList( QWidget *rootWidget )
{
    QList<QAction *> actionsList = rootWidget->findChildren<QAction *>();
    foreach( const QAction *action, actionsList )
    {
        const QList<QKeySequence> shortcuts = action->shortcuts();
        foreach( const QKeySequence &keySequence, shortcuts )
            existingkeys << keySequence.toString();
    }
}

void KeySelectorControl::finish()
{
    if( label && p_item->psz_longtext )
        label->setToolTip( formatTooltip( qtr( p_item->psz_longtext ) ) );

    /* Fill the table */

    /* Get the main Module */
    module_t *p_main = module_get_main();
    assert( p_main );

    /* Access to the module_config_t */
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    QMultiMap<QString, QString> global_keys;
    for (size_t i = 0; i < confsize; i++)
    {
        module_config_t *p_config_item = p_config + i;

        /* If we are a (non-global) key option not empty */
        if( CONFIG_ITEM(p_config_item->i_type) && p_config_item->psz_name != NULL
         && !strncmp( p_config_item->psz_name , "key-", 4 )
         && !EMPTY_STR( p_config_item->psz_text ) )
        {
            /*
               Each tree item has:
                - QString text in column 0
                - QString name in data of column 0
                - KeyValue in String in column 1
             */
            QTreeWidgetItem *treeItem = new QTreeWidgetItem();
            treeItem->setText( ACTION_COL, qtr( p_config_item->psz_text ) );
            treeItem->setData( ACTION_COL, Qt::UserRole,
                               QVariant( qfu( p_config_item->psz_name ) ) );

            QString keys = qfu(p_config_item->value.psz ? _(p_config_item->value.psz) : "");
            treeItem->setText( HOTKEY_COL, keys );
            treeItem->setToolTip( HOTKEY_COL, qtr("Double click to change.\nDelete key to remove.") );
            treeItem->setToolTip( GLOBAL_HOTKEY_COL, qtr("Double click to change.\nDelete key to remove.") );
            treeItem->setData( HOTKEY_COL, Qt::UserRole, QVariant( p_config_item->value.psz ) );
            table->addTopLevelItem( treeItem );
            continue;
        }

        if( CONFIG_ITEM(p_config_item->i_type) && p_config_item->psz_name != NULL
         && !strncmp( p_config_item->psz_name , "global-key", 10 )
         && !EMPTY_STR( p_config_item->psz_text )
         && !EMPTY_STR( p_config_item->value.psz ) )
        {
            global_keys.insert( qtr( p_config_item->psz_text ), qfu( p_config_item->value.psz ) );
        }
    }

    QMap<QString, QString>::const_iterator i = global_keys.constBegin();
    while (i != global_keys.constEnd())
    {
        QList<QTreeWidgetItem *> list =
            table->findItems( i.key(), Qt::MatchExactly|Qt::MatchWrap, ACTION_COL );
        if( list.count() >= 1 )
        {
            QString keys = i.value();
            list[0]->setText( GLOBAL_HOTKEY_COL, keys );
            list[0]->setData( GLOBAL_HOTKEY_COL, Qt::UserRole, keys );
        }
        if( list.count() >= 2 )
            msg_Dbg( p_this, "This is probably wrong, %s", qtu(i.key()) );

        ++i;
    }

    module_config_free (p_config);

    table->resizeColumnToContents( 0 );

    CONNECT( table, itemActivated( QTreeWidgetItem *, int ),
             this, selectKey( QTreeWidgetItem *, int ) );
}

void KeySelectorControl::filter( const QString &qs_search )
{
    int i_column = searchOption->itemData( searchOption->currentIndex() ).toInt();
    QList<QTreeWidgetItem *> resultList;
    if ( i_column == ANY_COL )
    {
        for( int i = 0; i < ANY_COL; i++ )
            resultList << table->findItems( qs_search, Qt::MatchContains, i );
    }
    else
    {
        resultList = table->findItems( qs_search, Qt::MatchContains, i_column );
    }
    for( int i = 0; i < table->topLevelItemCount(); i++ )
    {
        table->topLevelItem( i )->setHidden(
                !resultList.contains( table->topLevelItem( i ) ) );
    }
}

void KeySelectorControl::selectKey( QTreeWidgetItem *keyItem, int column )
{
    /* This happens when triggered by ClickEater */
    if( keyItem == NULL ) keyItem = table->currentItem();

    /* This can happen when nothing is selected on the treeView
       and the shortcutValue is clicked */
    if( !keyItem ) return;

    /* If clicked on the first column, assuming user wants the normal hotkey */
    if( column == ACTION_COL ) column = HOTKEY_COL;

    bool b_global = ( column == GLOBAL_HOTKEY_COL );

    /* Launch a small dialog to ask for a new key */
    KeyInputDialog *d = new KeyInputDialog( table, keyItem->text( ACTION_COL ),
                                            table, b_global );
    d->setExistingkeysSet( &existingkeys );
    d->exec();

    if( d->result() == QDialog::Accepted )
    {
        QString newKey = VLCKeyToString( d->keyValue, false );

        /* In case of conflict, reset other keys*/
        if( d->conflicts )
        {
            QTreeWidgetItem *it;
            for( int i = 0; i < table->topLevelItemCount() ; i++ )
            {
                it = table->topLevelItem(i);
                if( ( keyItem != it ) &&
                    ( it->data( column, Qt::UserRole ).toString() == newKey ) )
                {
                    it->setText( column, NULL );
                    it->setData( column, Qt::UserRole, QVariant() );
                }
            }
        }

        keyItem->setText( column, VLCKeyToString( d->keyValue, true ) );
        keyItem->setData( column, Qt::UserRole, newKey );
    }
    else if( d->result() == 2 )
    {
        keyItem->setText( column, NULL );
        keyItem->setData( column, Qt::UserRole, QVariant() );
    }

    delete d;
}

void KeySelectorControl::doApply()
{
    QTreeWidgetItem *it;
    for( int i = 0; i < table->topLevelItemCount() ; i++ )
    {
        it = table->topLevelItem(i);
        if( it->data( HOTKEY_COL, Qt::UserRole ).toInt() >= 0 )
            config_PutPsz( qtu( it->data( ACTION_COL, Qt::UserRole ).toString() ),
                           qtu( it->data( HOTKEY_COL, Qt::UserRole ).toString() ) );

        config_PutPsz( qtu( "global-" + it->data( ACTION_COL, Qt::UserRole ).toString() ),
                       qtu( it->data( GLOBAL_HOTKEY_COL, Qt::UserRole ).toString() ) );
    }
}

bool KeySelectorControl::eventFilter( QObject *obj, QEvent *e )
{
    if( obj != table || e->type() != QEvent::KeyPress )
        return ConfigControl::eventFilter(obj, e);

    QKeyEvent *keyEv = static_cast<QKeyEvent*>(e);
    QTreeWidget *aTable = static_cast<QTreeWidget *>(obj);
    if( keyEv->key() == Qt::Key_Escape )
    {
        aTable->clearFocus();
        return true;
    }
    else if( keyEv->key() == Qt::Key_Return ||
             keyEv->key() == Qt::Key_Enter )
    {
        selectKey( aTable->currentItem(), aTable->currentColumn() );
        return true;
    }
    else if( keyEv->key() == Qt::Key_Delete )
    {
        if( aTable->currentColumn() != ACTION_COL )
        {
            aTable->currentItem()->setText( aTable->currentColumn(), NULL );
            aTable->currentItem()->setData( aTable->currentColumn(), Qt::UserRole, QVariant() );
        }
        return true;
    }
    else
        return false;
}


/**
 * Class KeyInputDialog
 **/
KeyInputDialog::KeyInputDialog( QTreeWidget *_table,
                                const QString& keyToChange,
                                QWidget *_parent,
                                bool _b_global ) :
                                QDialog( _parent ), keyValue(0), b_global( _b_global )
{
    setModal( true );
    conflicts = false;
    existingkeys = NULL;

    table = _table;
    setWindowTitle( ( b_global ? qtr( "Global" ) + QString(" ") : "" )
                    + qtr( "Hotkey change" ) );
    setWindowRole( "vlc-key-input" );

    QVBoxLayout *vLayout = new QVBoxLayout( this );
    selected = new QLabel( qtr( "Press the new key or combination for " )
                           + QString("<b>%1</b>").arg( keyToChange ) );
    vLayout->addWidget( selected , Qt::AlignCenter );

    warning = new QLabel;
    warning->hide();
    vLayout->insertWidget( 1, warning );

    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    ok = new QPushButton( qtr("Assign") );
    QPushButton *cancel = new QPushButton( qtr("Cancel") );
    unset = new QPushButton( qtr("Unset") );
    buttonBox->addButton( ok, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( unset, QDialogButtonBox::ActionRole );
    buttonBox->addButton( cancel, QDialogButtonBox::RejectRole );
    ok->setDefault( true );

    ok->setFocusPolicy(Qt::NoFocus);
    unset->setFocusPolicy(Qt::NoFocus);
    cancel->setFocusPolicy(Qt::NoFocus);

    vLayout->addWidget( buttonBox );
    ok->hide();

    CONNECT( buttonBox, accepted(), this, accept() );
    CONNECT( buttonBox, rejected(), this, reject() );
    BUTTONACT( unset, unsetAction() );
}

void KeyInputDialog::setExistingkeysSet( const QSet<QString> *keyset )
{
    existingkeys = keyset;
}

void KeyInputDialog::checkForConflicts( int i_vlckey, const QString &sequence )
{
    QList<QTreeWidgetItem *> conflictList =
        table->findItems( VLCKeyToString( i_vlckey, true ), Qt::MatchExactly,
                          b_global ? 2 : 1 );

    if( conflictList.count() &&
        !conflictList[0]->data( b_global ? 2 : 1, Qt::UserRole ).toString().isEmpty() &&
         conflictList[0]->data( b_global ? 2 : 1, Qt::UserRole ).toString() != "Unset" )
    {
        warning->setText( qtr("Warning: this key or combination is already assigned to ") +
                QString( "\"<b>%1</b>\"" ).arg( conflictList[0]->text( 0 ) ) );
        warning->show();
        ok->show();
        unset->hide();

        conflicts = true;
    }
    else if( existingkeys && !sequence.isEmpty()
             && existingkeys->contains( sequence ) )
    {
        warning->setText(
            qtr( "Warning: <b>%1</b> is already an application menu shortcut" )
                    .arg( sequence )
        );
        warning->show();
        ok->show();
        unset->hide();

        conflicts = true;
    }
    else accept();
}

void KeyInputDialog::keyPressEvent( QKeyEvent *e )
{
    if( e->key() == Qt::Key_Tab ||
        e->key() == Qt::Key_Shift ||
        e->key() == Qt::Key_Control ||
        e->key() == Qt::Key_Meta ||
        e->key() == Qt::Key_Alt ||
        e->key() == Qt::Key_AltGr )
        return;
    int i_vlck = qtEventToVLCKey( e );
    QKeySequence sequence( e->key() | e->modifiers() );
    selected->setText( qtr( "Key or combination: " )
                + QString("<b>%1</b>").arg( VLCKeyToString( i_vlck, true ) ) );
    checkForConflicts( i_vlck, sequence.toString() );
    keyValue = i_vlck;
}

void KeyInputDialog::wheelEvent( QWheelEvent *e )
{
    int i_vlck = qtWheelEventToVLCKey( e );
    selected->setText( qtr( "Key: " ) + VLCKeyToString( i_vlck, true ) );
    checkForConflicts( i_vlck, QString() );
    keyValue = i_vlck;
}

void KeyInputDialog::unsetAction() { done( 2 ); };
