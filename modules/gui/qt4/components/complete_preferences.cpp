/*****************************************************************************
 * preferences.cpp : "Normal preferences"
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

#include <QApplication>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QString>
#include <QFont>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QPalette>
#include <QColor>

#include "components/complete_preferences.hpp"
#include "components/preferences_widgets.hpp"

#include <vlc_config_cat.h>
#include <vlc_intf_strings.h>
#include <assert.h>

#define ITEM_HEIGHT 25

/*********************************************************************
 * The Tree
 *********************************************************************/
PrefsTree::PrefsTree( intf_thread_t *_p_intf, QWidget *_parent ) :
                            QTreeWidget( _parent ), p_intf( _p_intf )
{
    setColumnCount( 1 );
    setAlternatingRowColors( true );
    header()->hide();
    setIconSize( QSize( ITEM_HEIGHT,ITEM_HEIGHT ) );
    setTextElideMode( Qt::ElideNone );
    setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOn );

#define BI( a,b) QIcon a##_icon = QIcon( QPixmap( b ))
    BI( audio, ":/pixmaps/vlc_advprefs_audio.png" );
    BI( video, ":/pixmaps/vlc_advprefs_video.png" );
    BI( input, ":/pixmaps/vlc_advprefs_codec.png" );
    BI( sout, ":/pixmaps/vlc_advprefs_sout.png" );
    BI( advanced, ":/pixmaps/vlc_advprefs_extended.png" );
    BI( playlist, ":/pixmaps/vlc_advprefs_playlist.png" );
    BI( interface, ":/pixmaps/vlc_advprefs_intf.png" );
#undef BI

    /* Build the tree for the main module */
    const module_t *p_module = NULL;
    vlc_list_t *p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE,
                                        FIND_ANYWHERE );
    if( !p_list ) return;
    for( unsigned i = 0; p_module == NULL; i++ )
    {
        assert (i < (unsigned)p_list->i_count);

        const module_t *p_main = (module_t *)p_list->p_values[i].p_object;
        if( strcmp( module_GetObjName( p_main ), "main" ) == 0 )
            p_module = p_main;
    }

    PrefsItemData *data = NULL;
    QTreeWidgetItem *current_item = NULL;
    for (size_t i = 0; i < p_module->confsize; i++)
    {
        const module_config_t *p_item = p_module->p_config + i;
        const char *psz_help;
        QIcon icon;
        switch( p_item->i_type )
        {
        case CONFIG_CATEGORY:
            if( p_item->value.i == -1 ) break;
            data = new PrefsItemData();
            data->name = QString( qtr( config_CategoryNameGet
                                           ( p_item->value.i ) ) );
            psz_help = config_CategoryHelpGet( p_item->value.i );
            if( psz_help )
                data->help = QString( qtr(psz_help) );
            else
                data->help.clear();
            data->i_type = TYPE_CATEGORY;
            data->i_object_id = p_item->value.i;

            switch( p_item->value.i )
            {
#define CI(a,b) case a: icon = b##_icon;break
            CI( CAT_AUDIO, audio );
            CI( CAT_VIDEO, video );
            CI( CAT_INPUT, input );
            CI( CAT_SOUT, sout );
            CI( CAT_ADVANCED, advanced );
            CI( CAT_PLAYLIST, playlist );
            CI( CAT_INTERFACE, interface );
#undef CI
            }

            current_item = new QTreeWidgetItem();
            current_item->setText( 0, data->name );
            current_item->setIcon( 0 , icon );
            current_item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );
            current_item->setData( 0, Qt::UserRole,
                                   qVariantFromValue( data ) );
            addTopLevelItem( current_item );
            break;
        case CONFIG_SUBCATEGORY:
            if( p_item->value.i == -1 ) break;
            if( data &&
                ( p_item->value.i == SUBCAT_VIDEO_GENERAL ||
                  p_item->value.i == SUBCAT_ADVANCED_MISC ||
                  p_item->value.i == SUBCAT_INPUT_GENERAL ||
                  p_item->value.i == SUBCAT_INTERFACE_GENERAL ||
                  p_item->value.i == SUBCAT_SOUT_GENERAL||
                  p_item->value.i == SUBCAT_PLAYLIST_GENERAL||
                  p_item->value.i == SUBCAT_AUDIO_GENERAL ) )
            {
                // Data still contains the correct thing
                data->i_type = TYPE_CATSUBCAT;
                data->i_subcat_id = p_item->value.i;
                data->name = QString( qtr( config_CategoryNameGet(
                                            p_item->value.i )) );
                psz_help = config_CategoryHelpGet( p_item->value.i );
                if( psz_help )
                    data->help = QString( qtr(psz_help) );
                else
                    data->help.clear();
                current_item->setData( 0, Qt::UserRole,
                                       QVariant::fromValue( data ) );
                continue;
            }
            data = new PrefsItemData();
            data->name = QString( qtr( config_CategoryNameGet(
                                                        p_item->value.i)) );
            psz_help = config_CategoryHelpGet( p_item->value.i );
            if( psz_help )
                data->help = QString( qtr(psz_help) );
            else
                data->help.clear();
            data->i_type = TYPE_SUBCATEGORY;
            data->i_object_id = p_item->value.i;

            assert( current_item );

            /* TODO : Choose the image */
            QTreeWidgetItem *subcat_item = new QTreeWidgetItem();
            subcat_item->setText( 0, data->name );
            //item->setIcon( 0 , XXX );
            subcat_item->setData( 0, Qt::UserRole,
                                  qVariantFromValue(data) );
            subcat_item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );
            current_item->addChild( subcat_item );
            break;
        }
    }

    /* Build the tree of plugins */
    for( int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;

        // Main module excluded
        if( !strcmp( module_GetObjName( p_module ), "main" ) ) continue;

        /* Exclude submodules; they have no config options of their own */
        if( p_module->b_submodule ) continue;

        unsigned i_subcategory = 0, i_category = 0;
        bool b_options = false;

        for (size_t i = 0; i < p_module->confsize; i++)
        {
            const module_config_t *p_item = p_module->p_config + i;

            if( p_item->i_type == CONFIG_CATEGORY )
                i_category = p_item->value.i;
            else if( p_item->i_type == CONFIG_SUBCATEGORY )
                i_subcategory = p_item->value.i;

            if( p_item->i_type & CONFIG_ITEM )
                b_options = true;

            if( b_options && i_category && i_subcategory )
                break;
        }
        if( !b_options || i_category == 0 || i_subcategory == 0 ) continue;

        // Locate the category item;
        QTreeWidgetItem *subcat_item = NULL;
        bool b_found = false;
        for( int i_cat_index = 0 ; i_cat_index < topLevelItemCount();
                                   i_cat_index++ )
        {
            QTreeWidgetItem *cat_item = topLevelItem( i_cat_index );
            PrefsItemData *data = cat_item->data( 0, Qt::UserRole ).
                                             value<PrefsItemData *>();
            if( data->i_object_id == i_category )
            {
                for( int i_sc_index = 0; i_sc_index < cat_item->childCount();
                         i_sc_index++ )
                {
                    subcat_item = cat_item->child( i_sc_index );
                    PrefsItemData *sc_data = subcat_item->data(0, Qt::UserRole).
                                                value<PrefsItemData *>();
                    if( sc_data && sc_data->i_object_id == i_subcategory )
                    {
                        b_found = true;
                        break;
                    }
                }
                if( !b_found )
                {
                    subcat_item = cat_item;
                    b_found = true;
                }
                break;
            }
        }
        if( !b_found ) continue;

        PrefsItemData *module_data = new PrefsItemData();
        module_data->b_submodule = p_module->b_submodule;
        module_data->i_type = TYPE_MODULE;
        module_data->psz_name = strdup( module_GetObjName( p_module ) );
        module_data->i_object_id = p_module->b_submodule ?
                         ((module_t *)p_module->p_parent)->i_object_id :
                         p_module->i_object_id;
        module_data->help.clear();
        // TODO image
        QTreeWidgetItem *module_item = new QTreeWidgetItem();
        module_item->setText( 0, qtr( module_GetName( p_module, VLC_FALSE ) ) );
        //item->setIcon( 0 , XXX );
        module_item->setData( 0, Qt::UserRole,
                              QVariant::fromValue( module_data) );
        module_item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );
        subcat_item->addChild( module_item );
    }

    /* We got everything, just sort a bit */
    sortItems( 0, Qt::AscendingOrder );

    vlc_list_release( p_list );
}

PrefsTree::~PrefsTree() {}

void PrefsTree::applyAll()
{
    doAll( false );
}

void PrefsTree::cleanAll()
{
    doAll( true );
}

void PrefsTree::doAll( bool doclean )
{
    for( int i_cat_index = 0 ; i_cat_index < topLevelItemCount();
             i_cat_index++ )
    {
        QTreeWidgetItem *cat_item = topLevelItem( i_cat_index );
        for( int i_sc_index = 0; i_sc_index < cat_item->childCount();
                 i_sc_index++ )
        {
            QTreeWidgetItem *sc_item = cat_item->child( i_sc_index );
            for( int i_module = 0 ; i_module < sc_item->childCount();
                     i_module++ )
            {
                PrefsItemData *data = sc_item->child( i_module )->
                               data( 0, Qt::UserRole).value<PrefsItemData *>();
                if( data->panel && doclean )
                {
                    delete data->panel;
                    data->panel = NULL;
                }
                else if( data->panel )
                    data->panel->apply();
            }
            PrefsItemData *data = sc_item->data( 0, Qt::UserRole).
                                            value<PrefsItemData *>();
            if( data->panel && doclean )
            {
                delete data->panel;
                data->panel = NULL;
            }
            else if( data->panel )
                data->panel->apply();
        }
        PrefsItemData *data = cat_item->data( 0, Qt::UserRole).
                                            value<PrefsItemData *>();
        if( data->panel && doclean )
        {
            delete data->panel;
            data->panel = NULL;
        }
        else if( data->panel )
            data->panel->apply();
    }
}

/*********************************************************************
 * The Panel
 *********************************************************************/
PrefsPanel::PrefsPanel( QWidget *_parent ) : QWidget( _parent )
{}

PrefsPanel::PrefsPanel( intf_thread_t *_p_intf, QWidget *_parent,
                        PrefsItemData * data ) :
                        QWidget( _parent ), p_intf( _p_intf )
{
    /* Find our module */
    module_t *p_module = NULL;
    if( data->i_type == TYPE_CATEGORY )
        return;
    else if( data->i_type == TYPE_MODULE )
        p_module = (module_t *) vlc_object_get( p_intf, data->i_object_id );
    else
    {
        p_module = config_FindModule( VLC_OBJECT(p_intf), "main" );
        assert( p_module );
        vlc_object_yield( p_module );
    }

    module_t *p_realmodule = p_module->b_submodule
            ? (module_t *)(p_module->p_parent)
            : p_module;

    module_config_t *p_item = p_realmodule->p_config;
    module_config_t *p_end = p_item + p_realmodule->confsize;

    if( data->i_type == TYPE_SUBCATEGORY || data->i_type ==  TYPE_CATSUBCAT )
    {
        while (p_item < p_end)
        {
            if( p_item->i_type == CONFIG_SUBCATEGORY &&
                            ( data->i_type == TYPE_SUBCATEGORY &&
                              p_item->value.i == data->i_object_id ) ||
                            ( data->i_type == TYPE_CATSUBCAT &&
                              p_item->value.i == data->i_subcat_id ) )
                break;
            p_item++;
        }
    }

    /* Widgets now */
    global_layout = new QVBoxLayout();
    global_layout->setMargin( 2 );
    QString head;
    QString help;

    help = QString( data->help );

    if( data->i_type == TYPE_SUBCATEGORY || data->i_type ==  TYPE_CATSUBCAT )
    {
        head = QString( data->name );
        p_item++; // Why that ?
    }
    else
    {
        head = QString( qtr( module_GetLongName( p_module ) ) );
        if( p_module->psz_help )
        {
            help.append( "\n" );
            help.append( qtr( module_GetHelp( p_module ) ) );
        }
    }

    QLabel *titleLabel = new QLabel( head );
    QFont titleFont = QApplication::font( static_cast<QWidget*>(0) );
    titleFont.setPointSize( titleFont.pointSize() + 6 );
    titleFont.setFamily( "Verdana" );
    titleLabel->setFont( titleFont );

    // Title <hr>
    QFrame *title_line = new QFrame;
    title_line->setFrameShape(QFrame::HLine);
    title_line->setFrameShadow(QFrame::Sunken);

    QLabel *helpLabel = new QLabel( help, this );
    helpLabel->setWordWrap( true );

    global_layout->addWidget( titleLabel );
    global_layout->addWidget( title_line );
    global_layout->addWidget( helpLabel );

    QGroupBox *box = NULL;
    QGridLayout *boxlayout = NULL;

    QScrollArea *scroller= new QScrollArea;
    scroller->setFrameStyle( QFrame::NoFrame );
    QWidget *scrolled_area = new QWidget;

    QGridLayout *layout = new QGridLayout();
    int i_line = 0, i_boxline = 0;
    bool has_hotkey = false;

    if( p_item ) do
    {
        if( ( ( data->i_type == TYPE_SUBCATEGORY &&
                p_item->value.i != data->i_object_id ) ||
              ( data->i_type == TYPE_CATSUBCAT  &&
                p_item->value.i != data->i_subcat_id ) ) &&
            ( p_item->i_type == CONFIG_CATEGORY ||
              p_item->i_type == CONFIG_SUBCATEGORY ) )
            break;
        if( p_item->b_internal == VLC_TRUE ) continue;

        if( p_item->i_type == CONFIG_SECTION )
        {
            if( box )
            {
                box->setLayout( boxlayout );
                layout->addWidget( box, i_line, 0, 1, 2 );
                i_line++;
            }
            box = new QGroupBox( qtr(p_item->psz_text) );
            boxlayout = new QGridLayout();
        }
        /* Only one hotkey control */
        if( has_hotkey && p_item->i_type & CONFIG_ITEM && p_item->psz_name &&
                                         strstr( p_item->psz_name, "key-" ) )
            continue;
        if( p_item->i_type & CONFIG_ITEM && p_item->psz_name &&
                                            strstr( p_item->psz_name, "key-" ) )
            has_hotkey = true;

        ConfigControl *control;
        if( ! box )
            control = ConfigControl::createControl( VLC_OBJECT( p_intf ),
                                        p_item, NULL, layout, i_line );
        else
            control = ConfigControl::createControl( VLC_OBJECT( p_intf ),
                                    p_item, NULL, boxlayout, i_boxline );
        if( !control )
            continue;

        if( has_hotkey )
        {
            /* A hotkey widget takes 2 lines */
            if( box ) i_boxline ++;
            else i_line++;
        }

        if( box ) i_boxline++;
        else i_line++;
        controls.append( control );
    }
    while( !( ( data->i_type == TYPE_SUBCATEGORY ||
               data->i_type == TYPE_CATSUBCAT ) &&
             ( p_item->i_type == CONFIG_CATEGORY ||
               p_item->i_type == CONFIG_SUBCATEGORY ) )
        && ( ++p_item < p_end ) );

    if( box )
    {
        box->setLayout( boxlayout );
        layout->addWidget( box, i_line, 0, 1, 2 );
    }

    vlc_object_release( p_module );

    scrolled_area->setSizePolicy( QSizePolicy::Preferred,QSizePolicy::Fixed );
    scrolled_area->setLayout( layout );
    scroller->setWidget( scrolled_area );
    scroller->setWidgetResizable( true );
    global_layout->addWidget( scroller );
    setLayout( global_layout );
}

void PrefsPanel::apply()
{
    QList<ConfigControl *>::Iterator i;
    for( i = controls.begin() ; i != controls.end() ; i++ )
    {
        ConfigControl *c = qobject_cast<ConfigControl *>(*i);
        c->doApply( p_intf );
    }
}
void PrefsPanel::clean()
{}
