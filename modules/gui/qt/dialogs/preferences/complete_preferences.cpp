/*****************************************************************************
 * complete_preferences.cpp : "Normal preferences"
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QApplication>
#include <QLabel>
#include <QTreeWidget>
#include <QString>
#include <QFont>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>

#include "dialogs/preferences/complete_preferences.hpp"
#include "dialogs/preferences/preferences_widgets.hpp"

#include <vlc_modules.h>
#include <cassert>

#define ITEM_HEIGHT 25

/*********************************************************************
 * The Tree
 *********************************************************************/
PrefsTree::PrefsTree( qt_intf_t *_p_intf, QWidget *_parent,
                      module_t **p_list, size_t count ) :
                            QTreeWidget( _parent ), p_intf( _p_intf )
{
    b_show_only_loaded = false;
    /* General Qt options */
    setAlternatingRowColors( true );
    setHeaderHidden( true );

    setIconSize( QSize( ITEM_HEIGHT, ITEM_HEIGHT ) );
    setTextElideMode( Qt::ElideNone );

    setUniformRowHeights( true );
    connect( this, &PrefsTree::itemExpanded, this, &PrefsTree::resizeColumns );

    main_module = module_get_main();
    assert( main_module );

    enum vlc_config_cat cat = CAT_UNKNOWN;
    enum vlc_config_subcat subcat = SUBCAT_UNKNOWN;
    QTreeWidgetItem *cat_item = nullptr;

    /* Create base cat/subcat tree from core config set */
    unsigned confsize;
    module_t *p_module = main_module;
    module_config_t *const p_config = module_config_get (p_module, &confsize);
    for( size_t i = 0; i < confsize; i++ )
    {
        module_config_t *p_item = p_config + i;

        if( p_item->i_type != CONFIG_SUBCATEGORY )
            continue;

        subcat = (enum vlc_config_subcat) p_item->value.i;
        cat = vlc_config_cat_FromSubcat(subcat);

        if( cat == CAT_UNKNOWN || cat == CAT_HIDDEN )
            continue;

        /* Create parent cat node? */
        if( findCatItem(cat) == nullptr )
            cat_item = createCatNode( cat );

        // Create subcat node
        // Note that this is done conditionally, primarily because we must take
        // no action here for 'general' subcats (which are merged into the
        // parent category node), but also just out of caution should a mistake
        // result in more than one instance of a subcat to be present within the
        // core option set.
        if( findSubcatItem( subcat ) == nullptr )
            createSubcatNode( cat_item, subcat );
    }
    module_config_free( p_config );

    /* Add nodes for plugins */
    /* A plugin gets one node for its entire set, located based on the first used subcat */
    for( size_t i = 0; i < count; i++ )
    {
        p_module = p_list[i];

        /* Main module is already covered above */
        if( module_is_main( p_module) )
            continue;

        unsigned confsize;
        module_config_t *const p_config = module_config_get (p_module, &confsize);

        /* Get the first (used) subcat */
        cat = CAT_UNKNOWN;
        subcat = SUBCAT_UNKNOWN;
        bool has_options = false;
        for (size_t i = 0; i < confsize; i++)
        {
            const module_config_t *p_item = p_config + i;

            if( p_item->i_type == CONFIG_SUBCATEGORY )
            {
                subcat = (enum vlc_config_subcat) p_item->value.i;
                cat = vlc_config_cat_FromSubcat( subcat );
                continue;
            }

            if( CONFIG_ITEM(p_item->i_type) )
            {
                has_options = true;
                /* If cat lookup worked, we can stop now, else keep looking */
                if( cat != CAT_UNKNOWN )
                    break;
            }
        }
        module_config_free (p_config);

        /* No options, or definitely no place to place it in the tree - ignore */
        if( !has_options || cat == CAT_UNKNOWN || cat == CAT_HIDDEN )
            continue;

        /* Locate the category item */
        /* If not found (very unlikely), we will create it */
        cat_item = findCatItem( cat );
        if ( !cat_item )
            cat_item = createCatNode( cat );

        /* Locate the subcategory item */
        /* If not found (was not used in the core option set - quite possible), we will create it */
        QTreeWidgetItem *subcat_item = findSubcatItem( subcat );
        if( !subcat_item )
            subcat_item = createSubcatNode( cat_item, subcat );

        createPluginNode( subcat_item, p_module );
    }

    // We got everything, just sort a bit.
    // We allow the subcat and plugin nodes to be alphabetical, but we force
    // the top-level cat nodes into a preferred order.
    sortItems( 0, Qt::AscendingOrder );
    unsigned index = 0;
    for (unsigned i = 0; i < ARRAY_SIZE(categories_array); i++)
    {
        cat_item = findCatItem( categories_array[i].id );
        if ( cat_item == NULL )
            continue;
        unsigned cur_index = (unsigned)indexOfTopLevelItem( cat_item );
        if (cur_index != index)
        {
            insertTopLevelItem( index, takeTopLevelItem( cur_index ) );
            expandItem( cat_item );
        }
        ++index;
    }

    resizeColumnToContents( 0 );
}

QTreeWidgetItem *PrefsTree::createCatNode( enum vlc_config_cat cat )
{
    enum vlc_config_subcat general_subcat = vlc_config_cat_GetGeneralSubcat( cat );
    assert(general_subcat != SUBCAT_UNKNOWN && general_subcat != SUBCAT_HIDDEN);

    PrefsTreeItem *item = new PrefsTreeItem( PrefsTreeItem::CATEGORY_NODE );

    item->cat_id = cat;
    item->subcat_id = general_subcat;
    item->p_module = this->main_module;
    item->name = qfu( vlc_config_subcat_GetName( general_subcat ) );
    item->help = qfu( vlc_config_subcat_GetHelp( general_subcat ) );

    QIcon icon;
    switch( cat )
    {
#define CI(a,b) case a: icon = QIcon( b );break
        CI( CAT_AUDIO,     ":/prefsmenu/advprefs_audio.svg"    );
        CI( CAT_VIDEO,     ":/prefsmenu/advprefs_video.svg"    );
        CI( CAT_INPUT,     ":/prefsmenu/advprefs_codec.svg"    );
        CI( CAT_SOUT,      ":/prefsmenu/advprefs_sout.svg"     );
        CI( CAT_ADVANCED,  ":/prefsmenu/advprefs_extended.svg" );
        CI( CAT_PLAYLIST,  ":/prefsmenu/advprefs_playlist.svg" );
        CI( CAT_INTERFACE, ":/prefsmenu/advprefs_intf.svg"     );
#undef CI
        default: break;
    }

    item->setText( 0, qfu( vlc_config_cat_GetName( cat ) ) );
    item->setIcon( 0, icon );
    //current_item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );

    int cat_index = (int) vlc_config_cat_IndexOf( cat );
    int general_subcat_index = (int) vlc_config_subcat_IndexOf( general_subcat );
    this->catMap[cat_index] = item;
    this->subcatMap[general_subcat_index] = item;

    addTopLevelItem( item );
    expandItem( item );

    return item;
}

QTreeWidgetItem *PrefsTree::createSubcatNode( QTreeWidgetItem * cat, enum vlc_config_subcat subcat )
{
    assert( cat );

    PrefsTreeItem *item = new PrefsTreeItem( PrefsTreeItem::SUBCATEGORY_NODE );

    item->cat_id = CAT_UNKNOWN;
    item->subcat_id = subcat;
    item->p_module = this->main_module;
    item->name = qfu( vlc_config_subcat_GetName( subcat ) );
    item->help = qfu( vlc_config_subcat_GetHelp( subcat ) );

    item->setText( 0, item->name );
    //item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );

    int subcat_index = (int) vlc_config_subcat_IndexOf( subcat );
    this->subcatMap[subcat_index] = item;

    cat->addChild( item );

    return item;
}

void PrefsTree::createPluginNode( QTreeWidgetItem * parent, module_t *mod )
{
    assert( parent );

    PrefsTreeItem *item = new PrefsTreeItem( PrefsTreeItem::PLUGIN_NODE );

    item->cat_id = CAT_UNKNOWN;
    item->subcat_id = SUBCAT_UNKNOWN;
    item->p_module = mod;
    item->module_name = strdup( module_get_object( mod ) );
    item->name = qfut( module_get_name( mod, false ) );
    const char *help = module_get_help( mod );
    if( help )
        item->help = qfut( help );
    const char *help_html = module_get_help_html( mod );
    if( help_html )
        item->help_html = qfut( help_html );

    item->setText( 0, item->name );
    //item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );

    parent->addChild( item );
}

QTreeWidgetItem *PrefsTree::findCatItem( enum vlc_config_cat cat )
{
    int cat_index = vlc_config_cat_IndexOf( cat );
    return this->catMap[cat_index];
}

QTreeWidgetItem *PrefsTree::findSubcatItem( enum vlc_config_subcat subcat )
{
    int subcat_index = vlc_config_subcat_IndexOf( subcat );
    return this->subcatMap[subcat_index];
}

void PrefsTree::applyAll()
{
    for( int i_cat_index = 0 ; i_cat_index < topLevelItemCount();
             i_cat_index++ )
    {
        PrefsTreeItem *cat_item = topLevelItem( i_cat_index );
        for( int i_sc_index = 0; i_sc_index < cat_item->childCount();
                 i_sc_index++ )
        {
            PrefsTreeItem *subcat_item = cat_item->child( i_sc_index );
            for( int i_module = 0 ; i_module < subcat_item->childCount();
                     i_module++ )
            {
                PrefsTreeItem *mod_item = subcat_item->child( i_module );
                if( mod_item->panel )
                    mod_item->panel->apply();
            }
            if( subcat_item->panel )
                subcat_item->panel->apply();
        }
        if( cat_item->panel )
            cat_item->panel->apply();
    }
}

/* apply filter on tree item and recursively on its sub items
 * returns whether the item was filtered */
bool PrefsTree::filterItems( PrefsTreeItem *item, const QString &text,
                           Qt::CaseSensitivity cs )
{
    bool sub_filtered = true;

    for( int i = 0; i < item->childCount(); i++ )
    {
        PrefsTreeItem *sub_item = item->child( i );
        if ( !filterItems( sub_item, text, cs ) )
        {
            /* not all the sub items were filtered */
            sub_filtered = false;
        }
    }

    bool filtered = sub_filtered && !item->contains( text, cs );
    if ( b_show_only_loaded && sub_filtered && !item->module_is_loaded )
        filtered = true;
    item->setHidden( filtered );

    return filtered;
}

void PrefsTree::unfilterItems( PrefsTreeItem *item )
{
    item->setHidden( false );
    for( int i = 0; i < item->childCount(); i++ )
        unfilterItems( item->child( i ) );
}

static void populateLoadedSet( QSet<QString> *loaded, vlc_object_t *p_node )
{
    Q_ASSERT( loaded );
    char *name = vlc_object_get_name( p_node );
    if ( !EMPTY_STR( name ) ) loaded->insert( QString( name ) );
    free( name );

    size_t count = 0, size;
    vlc_object_t **tab = NULL;

    do
    {
        delete[] tab;
        size = count;
        tab = new vlc_object_t *[size];
        count = vlc_list_children(p_node, tab, size);
    }
    while (size < count);

    for (size_t i = 0; i < count ; i++)
    {
        populateLoadedSet( loaded, tab[i] );
        vlc_object_release(tab[i]);
    }

    delete[] tab;
}

/* Updates the plugin node 'loaded' status to reflect currently
 * running modules */
void PrefsTree::updateLoadedStatus( PrefsTreeItem *item = NULL,
                                    QSet<QString> *loaded = NULL )
{
    bool b_release = false;

    if( loaded == NULL )
    {
        vlc_object_t *p_root = VLC_OBJECT( vlc_object_instance(p_intf) );
        loaded = new QSet<QString>();
        populateLoadedSet( loaded, p_root );
        b_release = true;
    }

    if ( item == NULL )
    {
        for( int i = 0 ; i < topLevelItemCount(); i++ )
            updateLoadedStatus( topLevelItem( i ), loaded );
    }
    else
    {
        if( item->node_type == PrefsTreeItem::PLUGIN_NODE )
            item->module_is_loaded = loaded->contains( QString( item->module_name ) );

        for( int i = 0; i < item->childCount(); i++ )
            updateLoadedStatus( item->child( i ), loaded );
    }

    if ( b_release )
        delete loaded;
}

/* apply filter on tree */
void PrefsTree::filter( const QString &text )
{
    bool clear_filter = text.isEmpty() && ! b_show_only_loaded;

    updateLoadedStatus();

    for( int i = 0 ; i < topLevelItemCount(); i++ )
    {
        PrefsTreeItem *cat_item = topLevelItem( i );
        if ( clear_filter )
            unfilterItems( cat_item );
        else
            filterItems( cat_item, text, Qt::CaseInsensitive );
    }
}

void PrefsTree::setLoadedOnly( bool b_only )
{
    b_show_only_loaded = b_only;
    filter( "" );
}

void PrefsTree::resizeColumns()
{
    resizeColumnToContents( 0 );
}

PrefsTreeItem::PrefsTreeItem( PrefsTreeItemType ty ) : QTreeWidgetItem(ty)
{
    node_type = ty;
    panel = nullptr;
    cat_id = CAT_UNKNOWN;
    subcat_id = SUBCAT_UNKNOWN;
    p_module = nullptr;
    module_name = nullptr;
    module_is_loaded = false;
}

/* go over the config items and search text in psz_text
 * also search the node name and head */
bool PrefsTreeItem::contains( const QString &text, Qt::CaseSensitivity cs )
{
    bool is_core = this->node_type != PrefsTreeItem::PLUGIN_NODE;
    enum vlc_config_subcat id = this->subcat_id;
    module_t *p_module = this->p_module;

    /* check the node itself (its name/longname/helptext) */

    QString head;
    if( !is_core )
        head = QString( qfut( module_GetLongName( p_module ) ) );

    if ( name.contains( text, cs )
         || (!is_core && head.contains( text, cs ))
         || (!help_html.isEmpty() ? help_html.contains( text, cs ) : help.contains( text, cs ))
       )
    {
        return true;
    }

    /* check options belonging to this core subcat or plugin */

    unsigned confsize;
    module_config_t *const p_config = module_config_get (p_module, &confsize),
                    *p_item = p_config,
                    *p_end = p_config + confsize;

    if( !p_config )
        return false;

    bool ret = false;

    if( is_core )
    {
        /* find start of relevant option block */
        while ( p_item < p_end )
        {
            if( p_item->i_type == CONFIG_SUBCATEGORY &&
                p_item->value.i == id
              )
                break;
            p_item++;
        }
        if( ++p_item >= p_end )
        {
            ret = false;
            goto end;
        }
    }

    do
    {
        if ( p_item->i_type == CONFIG_SUBCATEGORY )
        {
            /* for core, if we hit a subcat, stop */
            if ( is_core )
                break;
            /* a plugin's options are grouped under one node; we can/should
               ignore all other subcat entries. */
            continue;
        }

        /* cat-hint items are not relevant, they are an alternate set of headings for help output */
        if( p_item->i_type == CONFIG_HINT_CATEGORY ) continue;

        if ( p_item->psz_text && qfut( p_item->psz_text ).contains( text, cs ) )
        {
            ret = true;
            goto end;
        }
    }
    while ( ++p_item < p_end );

end:
    module_config_free( p_config );
    return ret;
}

/*********************************************************************
 * The Panel
 *********************************************************************/

AdvPrefsPanel::AdvPrefsPanel( qt_intf_t *_p_intf, QWidget *_parent,
                        PrefsTreeItem * node ) :
                        QWidget( _parent ), p_intf( _p_intf )
{
    module_t *p_module = node->p_module;

    unsigned confsize;
    p_config = module_config_get( p_module, &confsize );
    module_config_t *p_item = p_config,
                    *p_end = p_config + confsize;

    QString heading;

    if( node->node_type != PrefsTreeItem::PLUGIN_NODE  )
    {
        heading = QString( node->name );
        /* Skip to the relevant part of the core config */
        while (p_item < p_end)
        {
            if(  p_item->i_type == CONFIG_SUBCATEGORY &&
                 p_item->value.i == node->subcat_id )
                break;
            p_item++;
        }
        p_item++; // Skip past the subcat entry itself
    }
    else
    {
        heading = QString( qfut( module_GetLongName( p_module ) ) );
    }

    /* Widgets now */
    global_layout = new QVBoxLayout();
    global_layout->setContentsMargins(2, 2, 2, 2);

    QLabel *titleLabel = new QLabel( heading );
    QFont titleFont = QApplication::font();
    titleFont.setPointSize( titleFont.pointSize() + 6 );
    titleLabel->setFont( titleFont );

    // Title <hr>
    QFrame *title_line = new QFrame;
    title_line->setFrameShape(QFrame::HLine);
    title_line->setFrameShadow(QFrame::Sunken);

    QLabel *helpLabel = new QLabel( this );
    helpLabel->setWordWrap( true );
    if( node->help_html.isEmpty() )
    {
        helpLabel->setText( node->help );
        helpLabel->setTextFormat( Qt::PlainText );
    }
    else
    {
        helpLabel->setText( node->help_html );
        helpLabel->setTextFormat( Qt::RichText );
        helpLabel->setOpenExternalLinks( true );
    }

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

    if( p_item ) for ( ; p_item < p_end; ++p_item)
    {
        if( p_item->i_type == CONFIG_SUBCATEGORY )
        {
            if( node->node_type == PrefsTreeItem::PLUGIN_NODE )
                continue;
            else
                break;
        }

        if( p_item->i_type == CONFIG_SECTION )
        {
            if( box && i_boxline > 0 )
            {
                box->setLayout( boxlayout );
                box->show();
                layout->addWidget( box, i_line, 0, 1, -1 );
                i_line++;
            }
            i_boxline = 0;
            box = new QGroupBox( qfut( p_item->psz_text ), this );
            box->hide();
            boxlayout = new QGridLayout();
        }

        /* Only one hotkey control */
        if( p_item->i_type == CONFIG_ITEM_KEY )
        {
            if( has_hotkey )
                continue;
            has_hotkey = true;
        }

        ConfigControl *control;
        if( ! box )
            control = ConfigControl::createControl( p_item, this,
                                                    layout, i_line );
        else
            control = ConfigControl::createControl( p_item, this,
                                                    boxlayout, i_boxline );
        if( !control )
            continue;

        if( box ) i_boxline++;
        else i_line++;
        controls.append( control );
    }

    if( box && i_boxline > 0 )
    {
        box->setLayout( boxlayout );
        box->show();
        layout->addWidget( box, i_line, 0, 1, -1 );
    }

    scrolled_area->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
    scrolled_area->setLayout( layout );
    scroller->setWidget( scrolled_area );
    scroller->setWidgetResizable( true );
    global_layout->addWidget( scroller );
    setLayout( global_layout );
}

void AdvPrefsPanel::apply()
{
    foreach ( ConfigControl *cfg, controls )
        cfg->doApply();
}

void AdvPrefsPanel::clean()
{}

AdvPrefsPanel::~AdvPrefsPanel()
{
    qDeleteAll( controls );
    controls.clear();
    module_config_free( p_config );
}
