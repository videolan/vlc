/*****************************************************************************
 * PreferencesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.cpp,v 1.16 2003/05/05 13:06:02 titer Exp $
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

#include <InterfaceKit.h>
#include <SupportKit.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "PreferencesWindow.h"

/*****************************************************************************
 * StringItemWithView::StringItemWithView
 *****************************************************************************/
StringItemWithView::StringItemWithView( const char * text )
    : BStringItem( text )
{
    /* We use the default constructor */
}

/*****************************************************************************
 * ConfigView::ConfigView
 *****************************************************************************/
ConfigView::ConfigView( BRect frame, const char * name,
                        uint32 resizingMode, uint32 flags )
    : BView( frame, name, resizingMode, flags )
{
    /* We use the default constructor */
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
    BRect rect;

    /* The "background" view */
    rgb_color background = ui_color( B_PANEL_BACKGROUND_COLOR );
    fPrefsView = new BView( Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    fPrefsView->SetViewColor( background );
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
    fPrefsView->AddChild( fDummyView );
   
    /* Fill the tree */
    /* TODO:
        - manage CONFIG_HINT_SUBCATEGORY
        - use a pop-up for CONFIG_HINT_MODULE
        - use BSliders for integer_with_range and float_with_range
        - add a tab for BeOS specific configution (screenshot path, etc)
        - add the needed LockLooper()s
        - fix window resizing
        - make this intuitive ! */
    vlc_list_t * p_list;
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    if( !p_list )
    {
        msg_Warn( p_intf, "couldn't find any module !" );
        return;
    }

    module_t * p_module;
    for( int i = 0; i < p_list->i_count; i++ )
    {
        p_module = (module_t*) p_list->p_values[i].p_object;
        
        /* If the module has no config option, ignore it */
        module_config_t * p_item;
        p_item = p_module->p_config;
        if( !p_item )
            continue;
        do
        {
            if( p_item->i_type & CONFIG_ITEM )
                break;
        } while( p_item->i_type != CONFIG_HINT_END && p_item++ );
        if( p_item->i_type == CONFIG_HINT_END )
            continue;
        
        /* Build the config view for this module */
        rect = fDummyView->Bounds();
        rect.right -= B_V_SCROLL_BAR_WIDTH;
        ConfigView * configView;
        configView = new ConfigView( rect, "config view",
                                     B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW );
        configView->SetViewColor( background );
        
        rect = configView->Bounds();
        rect.InsetBy( 10, 10 );
        rect.bottom = rect.top + TEXT_HEIGHT;
        BTextControl * textControl;
        BCheckBox * checkBox;

        /* FIXME: we use the BControl name to store the VLC variable name.
           To know what variable type it is, I add one character at the beginning
           of the name (see ApplyChanges()); it's not pretty, but it works. To
           be cleaned later. */    
        char name[128];
        p_item = p_module->p_config;
        bool firstItem = true;
        do
        {
            switch( p_item->i_type )
            {
                case CONFIG_ITEM_STRING:
                case CONFIG_ITEM_FILE:
                case CONFIG_ITEM_MODULE:
                case CONFIG_ITEM_DIRECTORY:
                    if( !firstItem )
                        rect.OffsetBy( 0, 25 );
                    else
                        firstItem = false;
                    
                    memset( name, 0, 128 );
                    sprintf( name, "s%s", p_item->psz_name );
                    textControl = new BTextControl( rect, name, p_item->psz_text,
                                                    "", new BMessage(),
                                                    B_FOLLOW_NONE );
                    configView->AddChild( textControl );
                    break;

                case CONFIG_ITEM_INTEGER:
                    if( !firstItem )
                        rect.OffsetBy( 0, 25 );
                    else
                        firstItem = false;
                        
                    memset( name, 0, 128 );
                    sprintf( name, "i%s", p_item->psz_name );
                    textControl = new BTextControl( rect, name, p_item->psz_text,
                                                    "", new BMessage(),
                                                    B_FOLLOW_NONE );
                    configView->AddChild( textControl );
                    break;

                case CONFIG_ITEM_FLOAT:
                    if( !firstItem )
                        rect.OffsetBy( 0, 25 );
                    else
                        firstItem = false;
                        
                    memset( name, 0, 128 );
                    sprintf( name, "f%s", p_item->psz_name );
                    textControl = new BTextControl( rect, name, p_item->psz_text,
                                                    "", new BMessage(),
                                                    B_FOLLOW_NONE );
                    configView->AddChild( textControl );
                    break;
            
                case CONFIG_ITEM_BOOL:
                    if( !firstItem )
                        rect.OffsetBy( 0,25 );
                    else
                       firstItem = false;
                       
                    memset( name, 0, 128 );
                    sprintf( name, "b%s", p_item->psz_name );
                    checkBox = new BCheckBox( rect, name, p_item->psz_text,
                                              new BMessage(), B_FOLLOW_NONE );
                    configView->AddChild( checkBox );
                    break;
            }

        } while( p_item->i_type != CONFIG_HINT_END && p_item++ );
    
        /* Adjust the configView size */
        rect.bottom += 10;
        configView->fRealBounds = BRect( 0, 0, configView->Bounds().Width(), rect.bottom );
        configView->ResizeTo( configView->Bounds().Width(), configView->Bounds().Height() );

        /* Add the item to the tree */
        StringItemWithView * stringItem;
        stringItem = new StringItemWithView( p_module->psz_object_name );
        stringItem->fConfigView = configView;
        fOutline->AddItem( stringItem );
    }
    
    vlc_list_release( p_list );
    
    /* Set the correct values */
    ApplyChanges( false );
    
    /* Select the first item */
    fOutline->Select( 0 );
    
    /* Add the buttons */
    BButton * button;
    rect = Bounds();
    rect.InsetBy( 10, 10 );
    rect.left = rect.right - 80;
    rect.top = rect.bottom - 25;
    button = new BButton( rect, "", _("OK"), new BMessage( PREFS_OK ),
                          B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM );
    fPrefsView->AddChild( button );
    rect.OffsetBy( -90, 0 );
    button = new BButton( rect, "", _("Revert"), new BMessage( PREFS_REVERT ),
                          B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM );
    fPrefsView->AddChild( button );
    rect.OffsetBy( -90, 0 );
    button = new BButton( rect, "", _("Apply"), new BMessage( PREFS_APPLY ),
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
        
        case PREFS_OK:
            ApplyChanges( true );
            PostMessage( B_QUIT_REQUESTED );
            break;
        
        case PREFS_REVERT:
            ApplyChanges( false );
            break;
        
        case PREFS_APPLY:
            ApplyChanges( true );
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
    
    StringItemWithView * item;
    ConfigView * view;
    for( int i = 0; i < fOutline->CountItems(); i++ )
    {
        /* Fix ConfigView sizes */
        item = (StringItemWithView*) fOutline->ItemAt( i );
        view = item->fConfigView;
        view->ResizeTo( fDummyView->Bounds().Width() - B_V_SCROLL_BAR_WIDTH,
                        fDummyView->Bounds().Height() );
    }

    UpdateScrollBar();
}

/*****************************************************************************
 * PreferencesWindow::Update
 *****************************************************************************/
void PreferencesWindow::Update()
{
    /* Get the selected item */
    if( fOutline->CurrentSelection() < 0 )
        return;
    StringItemWithView * selectedItem =
        (StringItemWithView*) fOutline->ItemAt( fOutline->CurrentSelection() );

    if( fConfigScroll )
    {
        /* If we don't do this, the ConfigView will remember a wrong position */
        BScrollBar * scrollBar = fConfigScroll->ScrollBar( B_VERTICAL );
        scrollBar->SetValue( 0 );

        /* Detach the current ConfigView, remove the BScrollView */
        BView * view;
        while( ( view = fConfigScroll->ChildAt( 0 ) ) )
            fConfigScroll->RemoveChild( view );
        fDummyView->RemoveChild( fConfigScroll );
        delete fConfigScroll;
    }
    
    /* Create a BScrollView with the new ConfigView in it */
    fConfigScroll = new BScrollView( "", selectedItem->fConfigView, B_FOLLOW_ALL_SIDES,
                                     0, false, true, B_NO_BORDER );
    fDummyView->AddChild( fConfigScroll );
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
    ConfigView * view;
    if( fOutline->CurrentSelection() < 0 )
        return;
    StringItemWithView * selectedItem =
        (StringItemWithView*) fOutline->ItemAt( fOutline->CurrentSelection() );
    view = selectedItem->fConfigView;
    
    /* Get the available BRect for display */
    BRect display = fConfigScroll->Bounds();
    display.right -= B_V_SCROLL_BAR_WIDTH;
    
    /* Fix the scrollbar */
    BScrollBar * scrollBar;
    long max;
	BRect visible = display & view->fRealBounds;
	BRect total = display | view->fRealBounds;
    scrollBar = fConfigScroll->ScrollBar( B_VERTICAL );
    max = (long)( view->fRealBounds.Height() - visible.Height() );
    if( max < 0 ) max = 0;
    scrollBar->SetRange( 0, max );
    scrollBar->SetProportion( visible.Height() / total.Height() );
}

/*****************************************************************************
 * PreferencesWindow::ApplyChanges
 * Apply changes if doIt is true, revert them otherwise
 *****************************************************************************/
void PreferencesWindow::ApplyChanges( bool doIt )
{
    StringItemWithView * item;
    ConfigView * view;
    BView * child;
    const char * name;
    BString string;
    for( int i = 0; i < fOutline->CountItems(); i++ )
    {
        item = (StringItemWithView*) fOutline->ItemAt( i );
        view = item->fConfigView;
        
        for( int j = 0; j < view->CountChildren(); j++ )
        {
            child = view->ChildAt( j );
            name = child->Name();
            switch( *name )
            {
                case 's': /* BTextControl, string variable */
                    if( doIt )
                        config_PutPsz( p_intf, name + 1, ((BTextControl*)child)->Text() );
                    else
                        ((BTextControl*)child)->SetText( config_GetPsz( p_intf, name + 1 ) );
                    break;
                case 'i': /* BTextControl, int variable */
                    if( doIt )
                        config_PutInt( p_intf, name + 1, atoi( ((BTextControl*)child)->Text() ) );
                    else
                    {
                        string = "";
                        string << config_GetInt( p_intf, name + 1 );
                        ((BTextControl*)child)->SetText( string.String() );
                    }
                    break;
                case 'f': /* BTextControl, float variable */
                    if( doIt )
                        config_PutFloat( p_intf, name + 1,
                                         strtod( ((BTextControl*)child)->Text(), NULL ) );
                    else
                    {
                        string = "";
                        string << config_GetFloat( p_intf, name + 1 );
                        ((BTextControl*)child)->SetText( string.String() );
                    }
                    break;
                case 'b': /* BCheckBox, bool variable */
                    if( doIt )
                        config_PutInt( p_intf, name + 1, ((BCheckBox*)child)->Value() );
                    else
                        ((BCheckBox*)child)->SetValue( config_GetInt( p_intf, name + 1 ) );
                    break;
            }
        }
    }
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
