/*****************************************************************************
 * PreferencesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.cpp,v 1.22 2003/05/17 18:30:41 titer Exp $
 *
 * Authors: Eric Petit <titer@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdlib.h> /* atoi(), strtod() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "PreferencesWindow.h"

/* TODO:
    - handle CONFIG_HINT_SUBCATEGORY
    - use BSliders for integer_with_range and float_with_range
    - add the needed LockLooper()s
    - fix horizontal window resizing */

/* We use this function to order the items of the BOutlineView */
int compare_func( const BListItem * _first, const BListItem * _second )
{
    StringItemWithView * first = (StringItemWithView*) _first;
    StringItemWithView * second = (StringItemWithView*) _second;

    /* The Modules tree at last */
    if( !strcmp( first->Text(), _( "Modules" ) ) )
        return 1;
    if( !strcmp( second->Text(), _( "Modules" ) ) )
        return -1;

    /* alphabetic order */
    return( strcmp( first->Text(), second->Text() ) );
}

/*****************************************************************************
 * PreferencesWindow::PreferencesWindow
 *****************************************************************************/
PreferencesWindow::PreferencesWindow( intf_thread_t * p_interface,
                                      BRect frame, const char * name )
    : BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_ZOOMABLE | B_NOT_H_RESIZABLE ),
      fConfigScroll( NULL ),
      p_intf( p_interface )
{
    SetSizeLimits( PREFS_WINDOW_WIDTH, PREFS_WINDOW_WIDTH,
                   200, 2000 );

    BRect rect;

    /* The "background" view */
    fPrefsView = new BView( Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    fPrefsView->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    AddChild( fPrefsView );

    /* Create the preferences tree */
    rect = Bounds();
    rect.InsetBy( 10, 10 );
    rect.right = rect.left + 150;
    fOutline = new BOutlineListView( rect, "preferences tree",
                                     B_SINGLE_SELECTION_LIST,
                                     B_FOLLOW_LEFT | B_FOLLOW_TOP_BOTTOM );
    BScrollView * scrollview = new BScrollView( "scrollview", fOutline,
                                                B_FOLLOW_LEFT | B_FOLLOW_TOP_BOTTOM,
                                                0, false, true );
    fPrefsView->AddChild( scrollview );

    /* We need to be informed if the user selects an item */
    fOutline->SetSelectionMessage( new BMessage( PREFS_ITEM_SELECTED ) );

    /* Create a dummy view so we can correctly place the real config views later */
    rect.bottom -= 40;
    rect.left = rect.right + 15 + B_V_SCROLL_BAR_WIDTH;
    rect.right = Bounds().right - 15;
    fDummyView = new BView( rect, "", B_FOLLOW_ALL_SIDES, B_WILL_DRAW );
    fDummyView->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    fPrefsView->AddChild( fDummyView );

    /* Add a category for modules configuration */
    StringItemWithView * modulesItem;
    modulesItem = new StringItemWithView( _("Modules") );
    fOutline->AddItem( modulesItem );

    /* Fill the tree */
    vlc_list_t * p_list;
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    if( !p_list )
    {
        msg_Warn( p_intf, "couldn't find any module !" );
        return;
    }


    /* First, handle the main module */
    module_t * p_module = NULL;
    module_config_t * p_item;
    for( int i = 0; i < p_list->i_count; i++ )
    {
        p_module = (module_t*) p_list->p_values[i].p_object;

        if( !strcmp( p_module->psz_object_name, "main" ) &&
            ( p_item = p_module->p_config ) )
            break;
        else
            p_module = NULL;
    }

    if( p_module )
    {
        /* We found the main module */
        while( p_item->i_type == CONFIG_HINT_CATEGORY )
        {
            StringItemWithView * stringItem;
            stringItem = new StringItemWithView( p_item->psz_text );
            p_item++;
            stringItem->fConfigView = BuildConfigView( &p_item, true );
            fOutline->AddItem( stringItem );
        }
    }

    for( int i = 0; i < p_list->i_count; i++ )
    {
        p_module = (module_t*) p_list->p_values[i].p_object;

        if( !strcmp( p_module->psz_object_name, "main" ) )
            continue;

        /* If the module has no config option, ignore it */
        p_item = p_module->p_config;
        if( !p_item )
            continue;
        do {
            if( p_item->i_type & CONFIG_ITEM )
                break;
        } while( p_item->i_type != CONFIG_HINT_END && p_item++ );
        if( p_item->i_type == CONFIG_HINT_END )
            continue;

        /* Create the capability tree if it doesn't already exist */
        char * psz_capability;
        psz_capability = p_module->psz_capability;
        if( !psz_capability || !*psz_capability )
        {
            /* Empty capability ? Let's look at the submodules */
            module_t * p_submodule;
            for( int j = 0; j < p_module->i_children; j++ )
            {
                p_submodule = (module_t*)p_module->pp_children[ j ];
                if( p_submodule->psz_capability && *p_submodule->psz_capability )
                {
                    psz_capability = p_submodule->psz_capability;
                    break;
                }
            }
        }

        StringItemWithView * capabilityItem;
        capabilityItem = NULL;
        for( int j = 0; j < fOutline->CountItemsUnder( modulesItem, true ); j++ )
        {
            if( !strcmp( ((StringItemWithView*)
                             fOutline->ItemUnderAt( modulesItem, true, j ))->Text(),
                         psz_capability ) )
            {
                capabilityItem = (StringItemWithView*)
                    fOutline->ItemUnderAt( modulesItem, true, j );
                break;
            }
        }
        if( !capabilityItem )
        {
             capabilityItem = new StringItemWithView( psz_capability );
             fOutline->AddUnder( capabilityItem, modulesItem );
        }

        /* Now add the item ! */
        StringItemWithView * stringItem;
        stringItem = new StringItemWithView( p_module->psz_object_name );
        stringItem->fConfigView = BuildConfigView( &p_item, false );
        fOutline->AddUnder( stringItem, capabilityItem );
    }

    vlc_list_release( p_list );

    /* Set the correct values */
    ApplyChanges( false );

    /* Sort items, collapse the tree */
    fOutline->FullListSortItems( compare_func );
    fOutline->Collapse( modulesItem );
    for( int i = 0; i < fOutline->CountItemsUnder( modulesItem, true ); i++ )
        fOutline->Collapse( fOutline->ItemUnderAt( modulesItem, true, i ) );

    /* Select the first item */
    fOutline->Select( 0 );

    /* Add the buttons */
    BButton * button;
    rect = Bounds();
    rect.InsetBy( 10, 10 );
    rect.left = rect.right - 80;
    rect.top = rect.bottom - 25;
    button = new BButton( rect, "", _("Apply"), new BMessage( PREFS_APPLY ),
                          B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM );
    button->MakeDefault( true );
    fPrefsView->AddChild( button );
    rect.OffsetBy( -90, 0 );
    button = new BButton( rect, "", _("Save"), new BMessage( PREFS_SAVE ),
                          B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM );
    fPrefsView->AddChild( button );
    rect.OffsetBy( -90, 0 );
    button = new BButton( rect, "", _("Defaults"), new BMessage( PREFS_DEFAULTS ),
                          B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM );
    fPrefsView->AddChild( button );

    Hide();
    Show();
}

/*****************************************************************************
 * PreferencesWindow::~PreferencesWindow
 *****************************************************************************/
PreferencesWindow::~PreferencesWindow()
{
}

/*****************************************************************************
 * PreferencesWindow::QuitRequested
 *****************************************************************************/
bool PreferencesWindow::QuitRequested()
{
    if( !IsHidden() )
        Hide();
	return false;
}

/*****************************************************************************
 * PreferencesWindow::MessageReceived
 *****************************************************************************/
void PreferencesWindow::MessageReceived( BMessage * message )
{
    switch( message->what )
    {
        case PREFS_ITEM_SELECTED:
            Update();
            break;

        case PREFS_DEFAULTS:
            config_ResetAll( p_intf );
            ApplyChanges( false );
            break;

        case PREFS_APPLY:
            ApplyChanges( true );
            break;

        case PREFS_SAVE:
            SaveChanges();
            break;

        default:
            BWindow::MessageReceived( message );
    }
}

/*****************************************************************************
 * PreferencesWindow::FrameResized
 *****************************************************************************/
void PreferencesWindow::FrameResized( float width, float height )
{
    BWindow::FrameResized( width, height );

    /* Get the current config BView */
    BView * view;
    view = fConfigScroll->ChildAt( 0 );

    UpdateScrollBar();
}

/*****************************************************************************
 * PreferencesWindow::Update
 *****************************************************************************/
void PreferencesWindow::Update()
{
    /* Get the selected item, if any */
    if( fOutline->CurrentSelection() < 0 )
        return;
    StringItemWithView * selectedItem =
        (StringItemWithView*) fOutline->ItemAt( fOutline->CurrentSelection() );

    if( !selectedItem->fConfigView )
        /* This is a category */
        return;

    if( fConfigScroll )
    {
        /* If we don't do this, the config BView will remember a wrong position */
        BScrollBar * scrollBar = fConfigScroll->ScrollBar( B_VERTICAL );
        scrollBar->SetValue( 0 );

        /* Detach the current config BView, remove the BScrollView */
        BView * view;
        while( ( view = fConfigScroll->ChildAt( 0 ) ) )
            fConfigScroll->RemoveChild( view );
        fDummyView->RemoveChild( fConfigScroll );
        delete fConfigScroll;
    }

    /* Create a BScrollView with the new config BView in it */
    BRect oldBounds = selectedItem->fConfigView->Bounds();
    selectedItem->fConfigView->ResizeTo( fDummyView->Bounds().Width() -
                                             B_V_SCROLL_BAR_WIDTH,
                                         fDummyView->Bounds().Height() );
    fConfigScroll = new BScrollView( "", selectedItem->fConfigView, B_FOLLOW_ALL_SIDES,
                                     0, false, true, B_NO_BORDER );
    fConfigScroll->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    fDummyView->AddChild( fConfigScroll );
    selectedItem->fConfigView->ResizeTo( oldBounds.Width(),
                                         oldBounds.Height() );
    UpdateScrollBar();
}


/*****************************************************************************
 * PreferencesWindow::UpdateScrollBar
 *****************************************************************************/
void PreferencesWindow::UpdateScrollBar()
{
    /* We have to fix the scrollbar manually because it doesn't handle
       correctly simple BViews */

    /* Get the current config view */
    BView * view;
    view = fConfigScroll->ChildAt( 0 );

    /* Get the available BRect for display */
    BRect display = fConfigScroll->Bounds();
    display.right -= B_V_SCROLL_BAR_WIDTH;

    /* Fix the scrollbar */
    BScrollBar * scrollBar;
    long max;
	BRect visible = display & view->Bounds();
	BRect total = display | view->Bounds();
    scrollBar = fConfigScroll->ScrollBar( B_VERTICAL );
    max = (long)( view->Bounds().Height() - visible.Height() );
    if( max < 0 ) max = 0;
    scrollBar->SetRange( 0, max );
    scrollBar->SetProportion( visible.Height() / total.Height() );
    scrollBar->SetSteps( 10, 100 );
}

/*****************************************************************************
 * PreferencesWindow::ApplyChanges
 * Apply changes if doIt is true, revert them otherwise
 *****************************************************************************/
void PreferencesWindow::ApplyChanges( bool doIt )
{
    StringItemWithView * item;
    BView * view;
    BView * child;
    const char * name;
    BString string;
    for( int i = 0; i < fOutline->CountItems(); i++ )
    {
        item = (StringItemWithView*) fOutline->ItemAt( i );
        view = item->fConfigView;

        if( !view )
            /* This is a category */
            continue;

        for( int j = 0; j < view->CountChildren(); j++ )
        {
            child = view->ChildAt( j );
            name = child->Name();
            if( !strcmp( name, "ConfigTextControl" ) )
            {
                ConfigTextControl * textControl;
                textControl = (ConfigTextControl*) child;
                switch( textControl->fConfigType )
                {
                    case CONFIG_ITEM_STRING:
                        if( doIt )
                            config_PutPsz( p_intf, textControl->fConfigName, textControl->Text() );
                        else
                            textControl->SetText( config_GetPsz( p_intf, textControl->fConfigName ) );
                        break;
                    case CONFIG_ITEM_INTEGER:
                        if( doIt )
                            config_PutInt( p_intf, textControl->fConfigName, atoi( textControl->Text() ) );
                        else
                        {
                            string = "";
                            string << config_GetInt( p_intf, textControl->fConfigName );
                            textControl->SetText( string.String() );
                        }
                        break;
                    case CONFIG_ITEM_FLOAT:
                        if( doIt )
                            config_PutFloat( p_intf, textControl->fConfigName,
                                             strtod( textControl->Text(), NULL ) );
                        else
                        {
                            string = "";
                            string << config_GetFloat( p_intf, textControl->fConfigName );
                            textControl->SetText( string.String() );
                        }
                        break;
                }
            }
            else if( !strcmp( name, "ConfigCheckBox" ) )
            {
                ConfigCheckBox * checkBox;
                checkBox = (ConfigCheckBox*) child;
                if( doIt )
                    config_PutInt( p_intf, checkBox->fConfigName, checkBox->Value() );
                else
                    checkBox->SetValue( config_GetInt( p_intf, checkBox->fConfigName ) );
            }
            else if( !strcmp( name, "ConfigMenuField" ) )
            {
                ConfigMenuField * menuField;
                menuField = (ConfigMenuField*) child;
                BMenu * menu;
                BMenuItem * menuItem;
                menu = menuField->Menu();
                if( doIt )
                {
                    menuItem = menu->FindMarked();
                    if( menuItem )
                        config_PutPsz( p_intf, menuField->fConfigName, menuItem->Label() );
                }
                else
                {
                    char * value;
                    value = config_GetPsz( p_intf, menuField->fConfigName );
                    if( !value ) value = "";
                    for( int k = 0; k < menu->CountItems(); k++ )
                    {
                        menuItem = menu->ItemAt( k );
                        if( !strcmp( value, menuItem->Label() ) )
                        {
                            menuItem->SetMarked( true );
                            break;
                        }
                    }
                }
            }
        }
    }
}

/*****************************************************************************
 * PreferencesWindow::SaveChanges
 *****************************************************************************/
void PreferencesWindow::SaveChanges()
{
    ApplyChanges( true );
    config_SaveConfigFile( p_intf, NULL );
}

/*****************************************************************************
 * PreferencesWindow::ReallyQuit
 *****************************************************************************/
void PreferencesWindow::ReallyQuit()
{
    Lock();
    Hide();
    Quit();
}

/*****************************************************************************
 * PreferencesWindow::BuildConfigView
 *****************************************************************************/
BView * PreferencesWindow::BuildConfigView( module_config_t ** pp_item,
                                            bool stop_after_category )
{
    /* Build the config view for this module */
    BRect rect = fDummyView->Bounds();
    rect.right -= B_V_SCROLL_BAR_WIDTH;
    BView * configView;
    configView = new BView( rect, "config view",
                            B_FOLLOW_NONE, B_WILL_DRAW );
    configView->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );

    rect = configView->Bounds();
    rect.InsetBy( 10, 10 );
    rect.bottom = rect.top + TEXT_HEIGHT;
    ConfigTextControl * textControl;
    ConfigCheckBox * checkBox;
    ConfigMenuField * menuField;
    BPopUpMenu * popUp;

    bool firstItem = true;
    bool categoryHit = false;
    do
    {
        switch( (*pp_item)->i_type )
        {
            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_MODULE:
            case CONFIG_ITEM_DIRECTORY:
                if( !firstItem )
                    rect.OffsetBy( 0, 25 );
                else
                    firstItem = false;

                if( (*pp_item)->ppsz_list && (*pp_item)->ppsz_list[0] )
                {
                    popUp = new BPopUpMenu( "" );
                    menuField = new ConfigMenuField( rect, (*pp_item)->psz_text,
                                                     popUp, (*pp_item)->psz_name );
                    BMenuItem * menuItem;
                    for( int i = 0; (*pp_item)->ppsz_list[i]; i++ )
                    {
                        menuItem = new BMenuItem( (*pp_item)->ppsz_list[i], new BMessage() );
                        popUp->AddItem( menuItem );
                    }
                    configView->AddChild( menuField );
                }
                else
                {
                    textControl = new ConfigTextControl( rect, (*pp_item)->psz_text,
                                                         CONFIG_ITEM_STRING, (*pp_item)->psz_name );
                    configView->AddChild( textControl );
                }
                break;

            case CONFIG_ITEM_INTEGER:
                if( !firstItem )
                    rect.OffsetBy( 0, 25 );
                else
                    firstItem = false;

                textControl = new ConfigTextControl( rect, (*pp_item)->psz_text,
                                                     CONFIG_ITEM_INTEGER, (*pp_item)->psz_name );
                configView->AddChild( textControl );
                break;

            case CONFIG_ITEM_FLOAT:
                if( !firstItem )
                    rect.OffsetBy( 0, 25 );
                else
                    firstItem = false;

                textControl = new ConfigTextControl( rect, (*pp_item)->psz_text,
                                                     CONFIG_ITEM_FLOAT, (*pp_item)->psz_name );
                configView->AddChild( textControl );
                break;

            case CONFIG_ITEM_BOOL:
                if( !strcmp( (*pp_item)->psz_name, "advanced" ) )
                    /* Don't show this one, the interface doesn't handle it anyway */
                    break;

                if( !firstItem )
                    rect.OffsetBy( 0,25 );
                else
                   firstItem = false;

                checkBox = new ConfigCheckBox( rect, (*pp_item)->psz_text,
                                               (*pp_item)->psz_name );
                configView->AddChild( checkBox );
                break;

            case CONFIG_HINT_CATEGORY:
                if( stop_after_category )
                    categoryHit = true;
        }

    } while( !categoryHit &&
             (*pp_item)->i_type != CONFIG_HINT_END &&
             (*pp_item)++ );

    /* Adjust the configView size */
    rect.bottom += 10;
    configView->ResizeTo( configView->Bounds().Width(), rect.bottom );

    return configView;
}

