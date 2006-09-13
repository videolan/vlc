/*****************************************************************************
 * preferences.cpp : "Normal preferences"
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

#include "components/preferences.hpp"
#include "components/preferences_widgets.hpp"
#include "qt4.hpp"

#include <vlc_config_cat.h>
#include <vlc_intf_strings.h>
#include <assert.h>

#include "pixmaps/audio.xpm"
#include "pixmaps/video.xpm"
#include "pixmaps/type_net.xpm"
#include "pixmaps/type_playlist.xpm"
#include "pixmaps/advanced.xpm"
#include "pixmaps/codec.xpm"
#include "pixmaps/intf.xpm"

#define ITEM_HEIGHT 25

/*********************************************************************
 * The Tree
 *********************************************************************/
PrefsTree::PrefsTree( intf_thread_t *_p_intf, QWidget *_parent ) :
                            QTreeWidget( _parent ), p_intf( _p_intf )
{
    module_t *p_module;
    vlc_list_t *p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE,
                                        FIND_ANYWHERE );
    if( !p_list ) return;

    setColumnCount( 1 );
    setIconSize( QSize( ITEM_HEIGHT,ITEM_HEIGHT ) );
    setAlternatingRowColors( true );
    header()->hide();

#define BI( a,b) QIcon a##_icon = QIcon( QPixmap( b##_xpm ))
    BI( audio, audio );
    BI( video, video );
    BI( input, codec );
    BI( sout, type_net );
    BI( advanced, advanced );
    BI( playlist, type_playlist );
    BI( interface, intf );
#undef BI

    /* Build the tree for the main module */
    int i_index;
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;
        if( !strcmp( p_module->psz_object_name, "main" ) )
             break;
    }
    if( i_index < p_list->i_count )
    {
        module_config_t *p_item = p_module->p_config;
        PrefsItemData *data = NULL;
        QTreeWidgetItem *current_item = NULL;
        if( p_item ) do
        {
            char *psz_help;
            QIcon icon;
            switch( p_item->i_type )
            {
            case CONFIG_CATEGORY:
                if( p_item->i_value == -1 ) break;
                data = new PrefsItemData();
                data->name = QString( qfu( config_CategoryNameGet
                                               ( p_item->i_value ) ) );
                psz_help = config_CategoryHelpGet( p_item->i_value );
                if( psz_help )
                    data->help = QString( qfu(psz_help) );
                else
                    data->help.clear();
                data->i_type = TYPE_CATEGORY;
                data->i_object_id = p_item->i_value;

                switch( p_item->i_value )
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
                current_item->setData( 0, Qt::UserRole,
                                       qVariantFromValue( data ) );
                addTopLevelItem( current_item );
                break;
            case CONFIG_SUBCATEGORY:
                if( p_item->i_value == -1 ) break;
                if( data &&
                    ( p_item->i_value == SUBCAT_VIDEO_GENERAL ||
                      p_item->i_value == SUBCAT_ADVANCED_MISC ||
                      p_item->i_value == SUBCAT_INPUT_GENERAL ||
                      p_item->i_value == SUBCAT_INTERFACE_GENERAL ||
                      p_item->i_value == SUBCAT_SOUT_GENERAL||
                      p_item->i_value == SUBCAT_PLAYLIST_GENERAL||
                      p_item->i_value == SUBCAT_AUDIO_GENERAL ) )
                {
                    // Data still contains the correct thing
                    data->i_type = TYPE_CATSUBCAT;
                    data->i_subcat_id = p_item->i_value;
                    data->name = QString( qfu( config_CategoryNameGet(
                                                p_item->i_value )) );
                    psz_help = config_CategoryHelpGet( p_item->i_value );
                    if( psz_help )
                        data->help = QString( qfu(psz_help) );
                    else
                        data->help.clear();
                    current_item->setData( 0, Qt::UserRole,
                                           QVariant::fromValue( data ) );
                    continue;
                }
                data = new PrefsItemData();
                data->name = QString( qfu( config_CategoryNameGet( 
                                                            p_item->i_value)) );
                psz_help = config_CategoryHelpGet( p_item->i_value );
                if( psz_help )
                    data->help = QString( qfu(psz_help) );
                else
                    data->help.clear();
                data->i_type = TYPE_SUBCATEGORY;
                data->i_object_id = p_item->i_value;

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
        } while( p_item->i_type != CONFIG_HINT_END && p_item++ );
    }

    /* Build the tree of plugins */
    for( int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        module_config_t *p_item;
        int i_subcategory = -1, i_category = -1, i_options = 0;
        p_module = (module_t *)p_list->p_values[i_index].p_object;

        // Main module excluded
        if( !strcmp( p_module->psz_object_name, "main" ) ) continue;

        /* Exclude empty plugins (submodules don't have config options, they
         * are stored in the parent module) */
        if( p_module->b_submodule ) continue;

        p_item = p_module->p_config;
        if( !p_item ) continue;

        do {
            if( p_item->i_type == CONFIG_CATEGORY )
                i_category = p_item->i_value;
            else if( p_item->i_type == CONFIG_SUBCATEGORY )
                i_subcategory = p_item->i_value;
            if( p_item->i_type & CONFIG_ITEM )
                i_options++;

            if( i_options > 0 && i_category >= 0 && i_subcategory >= 0 )
                break;
        } while( p_item->i_type != CONFIG_HINT_END && p_item++ );

        if( !i_options ) continue; // Nothing to display

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
        module_data->i_object_id = p_module->b_submodule ?
                         ((module_t *)p_module->p_parent)->i_object_id :
                         p_module->i_object_id;
        module_data->help.clear();
        // TODO image
        QTreeWidgetItem *module_item = new QTreeWidgetItem();
        module_item->setText( 0, qfu( p_module->psz_shortname ?
                      p_module->psz_shortname : p_module->psz_object_name) );
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

/// \todo When cleaning, we should remove the panel ?
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
    module_config_t *p_item;

    /* Find our module */
    module_t *p_module = NULL;
    if( data->i_type == TYPE_CATEGORY )
        return;
    else if( data->i_type == TYPE_MODULE )
        p_module = (module_t *) vlc_object_get( p_intf, data->i_object_id );
    else
    {
        /* List the plugins */
        int i_index;
        vlc_bool_t b_found = VLC_FALSE;
        vlc_list_t *p_list = vlc_list_find( p_intf,
                                            VLC_OBJECT_MODULE, FIND_ANYWHERE );
        if( !p_list ) return;

        for( i_index = 0; i_index < p_list->i_count; i_index++ )
        {
            p_module = (module_t *)p_list->p_values[i_index].p_object;
            if( !strcmp( p_module->psz_object_name, "main" ) )
            {
                b_found = VLC_TRUE;
                break;
            }
        }
        if( !p_module && !b_found )
        {
            msg_Warn( p_intf, "unable to create preferences (main not found)");
            return;
        }
        if( p_module ) vlc_object_yield( p_module );
        vlc_list_release( p_list );
    }

    if( p_module->b_submodule )
        p_item = ((module_t *)p_module->p_parent)->p_config;
    else
        p_item = p_module->p_config;

    if( data->i_type == TYPE_SUBCATEGORY || data->i_type ==  TYPE_CATSUBCAT )
    {
        do
        {
            if( p_item->i_type == CONFIG_SUBCATEGORY &&
                            ( data->i_type == TYPE_SUBCATEGORY &&
                              p_item->i_value == data->i_object_id ) ||
                            ( data->i_type == TYPE_CATSUBCAT &&
                              p_item->i_value == data->i_subcat_id ) )
                break;
            if( p_item->i_type == CONFIG_HINT_END ) break;
        } while( p_item++ );
    }

    global_layout = new QVBoxLayout();
    QString head;
    if( data->i_type == TYPE_SUBCATEGORY || data->i_type ==  TYPE_CATSUBCAT )
    {
        head = QString( data->name );
        p_item++; // Why that ?
    }
    else
    {
        head = QString( qfu(p_module->psz_longname) );
        if( p_module->psz_help )
        {
            head.append( "\n" );
            head.append( qfu( p_module->psz_help ) );
        }
    }

    QLabel *label = new QLabel( head );
    global_layout->addWidget( label );
    QFont myFont = QApplication::font(0);
    myFont.setPointSize( myFont.pointSize() + 3 ); myFont.setBold( true );

    label->setFont( myFont );
    QLabel *help = new QLabel( data->help, this );
    help->setWordWrap( true );

    global_layout->addWidget( help );

    QGroupBox *box = NULL;
    QGridLayout *boxlayout = NULL;

    QScrollArea *scroller= new QScrollArea;
    scroller->setFrameStyle( QFrame::NoFrame );
    QWidget *scrolled_area = new QWidget;

    QGridLayout *layout = new QGridLayout();
    int i_line = 0, i_boxline = 0;

    if( p_item ) do
    {
        if( ( ( data->i_type == TYPE_SUBCATEGORY &&
                p_item->i_value != data->i_object_id ) ||
              ( data->i_type == TYPE_CATSUBCAT  &&
                p_item->i_value != data->i_subcat_id ) ) &&
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
            box = new QGroupBox( qfu(p_item->psz_text) );
            boxlayout = new QGridLayout();
        }

        ConfigControl *control;
        if( ! box )
            control = ConfigControl::createControl( VLC_OBJECT( p_intf ),
                                        p_item, NULL, layout, i_line );
        else
            control = ConfigControl::createControl( VLC_OBJECT( p_intf ),
                                    p_item, NULL, boxlayout, i_boxline );
        if( !control )
            continue;

        if( box ) i_boxline++;
        else i_line++;
        controls.append( control );
    }
    while( !(p_item->i_type == CONFIG_HINT_END ||
           ( ( data->i_type == TYPE_SUBCATEGORY ||
               data->i_type == TYPE_CATSUBCAT ) &&
             ( p_item->i_type == CONFIG_CATEGORY ||
               p_item->i_type == CONFIG_SUBCATEGORY ) ) ) && p_item++ );

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
