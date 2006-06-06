/*****************************************************************************
 * preferences_tree.cpp : Tree of modules for preferences
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

#include "components/preferences.hpp"
#include "components/preferences_widgets.hpp"
#include <vlc_config_cat.h>
#include <assert.h>

#include "pixmaps/audio.xpm"
#include "pixmaps/video.xpm"
#include "pixmaps/type_net.xpm"
#include "pixmaps/type_playlist.xpm"
#include "pixmaps/advanced.xpm"
#include "pixmaps/codec.xpm"
#include "pixmaps/intf.xpm"

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

    QFont f = font();
    f.setPointSize( f.pointSize() + 1 );
    setFont( f );

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
                data->name = QString( config_CategoryNameGet
                                               ( p_item->i_value ) );
                psz_help = config_CategoryHelpGet( p_item->i_value );
                if( psz_help )
                    data->help = QString( psz_help );
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
                //fprintf( stderr, "Adding %s\n", current_item->text(0).toLatin1().data() );
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
                    data->name = QString( config_CategoryNameGet(
                                                p_item->i_value ) );
                    psz_help = config_CategoryHelpGet( p_item->i_value );
                    if( psz_help )
                        data->help = QString( psz_help );
                    else
                        data->help.clear();
                    current_item->setData( 0, Qt::UserRole,
                                           QVariant::fromValue( data ) );
                    continue;
                }
                data = new PrefsItemData();
                data->name = QString( config_CategoryNameGet( p_item->i_value));
                psz_help = config_CategoryHelpGet( p_item->i_value );
                if( psz_help )
                    data->help = QString( psz_help );
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
        module_item->setText( 0, p_module->psz_shortname ?
                      p_module->psz_shortname : p_module->psz_object_name );
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

void PrefsTree::ApplyAll()
{
    DoAll( false );
}

void PrefsTree::CleanAll()
{
    DoAll( true );
}

/// \todo When cleaning, we should remove the panel ?
void PrefsTree::DoAll( bool doclean )
{
    for( int i_cat_index = 0 ; i_cat_index < topLevelItemCount();
             i_cat_index++ )
    {
        QTreeWidgetItem *cat_item = topLevelItem( i_cat_index );
        for( int i_sc_index = 0; i_sc_index <= cat_item->childCount();
                 i_sc_index++ )
        {
            QTreeWidgetItem *sc_item = cat_item->child( i_sc_index );
            for( int i_module = 0 ; i_module <= sc_item->childCount();
                     i_module++ )
            {
                PrefsItemData *data = sc_item->child( i_sc_index )->
                                                 data( 0, Qt::UserRole ).
                                                 value<PrefsItemData *>();
                if( data->panel && doclean )
                    data->panel->Clean();
                else if( data->panel )
                    data->panel->Apply();
            }
        }
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
    module_t *p_module;
    vlc_list_t *p_list = NULL;
    global_layout = new QVBoxLayout();

    if( data->i_type == TYPE_CATEGORY )
    {
        /* TODO */
            return;
    }
    else if( data->i_type == TYPE_MODULE )
    {
        p_module = (module_t *) vlc_object_get( p_intf, data->i_object_id );
    }
    else
    {
        /* List the plugins */
        int i_index;
        vlc_bool_t b_found = VLC_FALSE;
        p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
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
            {
                break;
            }
            if( p_item->i_type == CONFIG_HINT_END ) break;
        } while( p_item++ );
    }

    QString head;
    if( data->i_type == TYPE_SUBCATEGORY || data->i_type ==  TYPE_CATSUBCAT )
    {
        head = QString( data->name );
        p_item++; // Why that ?
    }
    else
        head = QString( p_module->psz_longname );

    QLabel *label = new QLabel( head, this );
    QFont font = label->font();
    font.setPointSize( font.pointSize() + 2 ); font.setBold( true );
    label->setFont( font );
    QLabel *help = new QLabel( data->help, this );
    help->setWordWrap( true );

    global_layout->addWidget( label );
    global_layout->addWidget( help );

    QGroupBox *box = NULL;
    QVBoxLayout *boxlayout = NULL;

    QScrollArea *scroller= new QScrollArea;
    QWidget *scrolled_area = new QWidget;

    QVBoxLayout *layout = new QVBoxLayout();

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
                layout->addWidget( box, 1 );
            }
            box = new QGroupBox( p_item->psz_text );
            boxlayout = new QVBoxLayout();
        }

        ConfigControl *control = ConfigControl::createControl(
                                    VLC_OBJECT( p_intf ), p_item,
                                    NULL );
        if( !control )
        {
            continue;
        }
        if( !box )
            layout->addWidget( control );
        else
            boxlayout->addWidget( control );

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
        layout->addWidget( box, 1 );
    }

    scrolled_area->setSizePolicy( QSizePolicy::Preferred,QSizePolicy::Fixed );
    scrolled_area->setLayout( layout );
    scroller->setWidget( scrolled_area );
    scroller->setWidgetResizable( true );
    global_layout->addWidget( scroller );

    some_hidden_text = new QLabel( "Some options are available but hidden. "\
                                  "Check \"Advanced options\" to see them." );
    some_hidden_text->setWordWrap( true );

    setLayout( global_layout );
    setAdvanced( false );
}

void PrefsPanel::Apply()
{
    /* todo */
    QList<ConfigControl *>::Iterator i;
    for( i = controls.begin() ; i != controls.end() ; i++ )
    {
        VIntConfigControl *vicc = qobject_cast<VIntConfigControl *>(*i);
        if( !vicc )
        {
            VFloatConfigControl *vfcc = qobject_cast<VFloatConfigControl *>(*i);
            if( !vfcc)
            {
                VStringConfigControl *vscc =
                               qobject_cast<VStringConfigControl *>(*i);
                assert( vscc );
                config_PutPsz( p_intf, vscc->getName().toAscii().data(),
                                       vscc->getValue().toAscii().data() );
                continue;
            }
            config_PutFloat( p_intf, vfcc->getName().toAscii().data(),
                                     vfcc->getValue() );
            continue;
        }
        config_PutInt( p_intf, vicc->getName().toAscii().data(),
                               vicc->getValue() );
    }
}

void PrefsPanel::Clean()
{}

void PrefsPanel::setAdvanced( bool adv )
{
    bool some_hidden = false;
    if( adv == advanced ) return;

    advanced = adv;
    QList<ConfigControl *>::Iterator i;
    for( i = controls.begin() ; i != controls.end() ; i++ )
    {
        if( (*i)->isAdvanced() )
        {
            fprintf( stderr, "Showing \n" );
            if( !advanced ) some_hidden = true;
            (*i)->setVisible( advanced );
        }
    }
    if( some_hidden_text )
    {
        global_layout->removeWidget( some_hidden_text );
        some_hidden_text->hide();
    }
    if( some_hidden )
    {
        global_layout->addWidget( some_hidden_text );
        some_hidden_text->show();
    }
}
