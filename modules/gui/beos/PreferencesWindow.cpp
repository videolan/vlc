/*****************************************************************************
 * PreferencesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.cpp,v 1.27 2003/12/22 00:06:05 titer Exp $
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

#include <String.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_keys.h>

#include "PreferencesWindow.h"

/* TODO:
    - add the needed LockLooper()s
    - fix window resizing */

/* We use this function to order the items of the BOutlineView */
static int compare_func( const BListItem * _first,
                         const BListItem * _second )
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
               B_NOT_ZOOMABLE | B_NOT_RESIZABLE ),
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
    BScrollView * scrollview =
        new BScrollView( "scrollview", fOutline,
                         B_FOLLOW_LEFT | B_FOLLOW_TOP_BOTTOM,
                         0, false, true );
    fPrefsView->AddChild( scrollview );

    /* We need to be informed if the user selects an item */
    fOutline->SetSelectionMessage( new BMessage( PREFS_ITEM_SELECTED ) );

    /* Create a dummy view so we can correctly place the real config
       views later */
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
            BuildConfigView( stringItem, &p_item, true );
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
        {
            continue;
        }
        do
        {
            if( p_item->i_type & CONFIG_ITEM )
            {
                break;
            }
        } while( p_item->i_type != CONFIG_HINT_END && p_item++ );

        if( p_item->i_type == CONFIG_HINT_END )
        {
            continue;
        }

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
                if( p_submodule->psz_capability &&
                        *p_submodule->psz_capability )
                {
                    psz_capability = p_submodule->psz_capability;
                    break;
                }
            }
        }

        StringItemWithView * capabilityItem;
        capabilityItem = NULL;
        for( int j = 0;
             j < fOutline->CountItemsUnder( modulesItem, true ); j++ )
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
        BuildConfigView( stringItem, &p_item, false );
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
    button = new BButton( rect, "", _("Defaults"),
                          new BMessage( PREFS_DEFAULTS ),
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
    {
        Hide();
    }
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
    fCurrent = (StringItemWithView*)
        fOutline->ItemAt( fOutline->CurrentSelection() );

    if( !fCurrent->fConfigBox )
        /* This is a category */
        return;

    /* Detach the old item */
    if( fDummyView->CountChildren() > 0 )
        fDummyView->RemoveChild( fDummyView->ChildAt( 0 ) );

    /* Resize and show the new config box */
    fCurrent->fConfigBox->ResizeTo( fDummyView->Bounds().Width(),
                                    fDummyView->Bounds().Height() );
    fDummyView->AddChild( fCurrent->fConfigBox );

    /* Force redrawing of its children */
    BRect rect = fCurrent->fConfigBox->Bounds();
    rect.InsetBy( 10,10 );
    rect.top += 10;
    fCurrent->fConfigScroll->ResizeTo( rect.Width(), rect.Height() );
    fCurrent->fConfigScroll->Draw( fCurrent->fConfigScroll->Bounds() );

    UpdateScrollBar();
}


/*****************************************************************************
 * PreferencesWindow::UpdateScrollBar
 *****************************************************************************/
void PreferencesWindow::UpdateScrollBar()
{
    /* We have to fix the scrollbar manually because it doesn't handle
       correctly simple BViews */

    if( !fCurrent )
    {
        return;
    }

    /* Get the available BRect for display */
    BRect display = fCurrent->fConfigScroll->Bounds();
    display.right -= B_V_SCROLL_BAR_WIDTH;

    /* Fix the scrollbar */
    BScrollBar * scrollBar;
    long max;
	BRect visible = display & fCurrent->fConfigView->Bounds();
	BRect total = display | fCurrent->fConfigView->Bounds();
    scrollBar = fCurrent->fConfigScroll->ScrollBar( B_VERTICAL );
    max = (long)( fCurrent->fConfigView->Bounds().Height() - visible.Height() );
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
    BView              * view;
    ConfigWidget       * child;
    BString              string;

    for( int i = 0; i < fOutline->CountItems(); i++ )
    {
        item = (StringItemWithView*) fOutline->ItemAt( i );
        view = item->fConfigView;

        if( !view )
        {
            /* This is a category */
            continue;
        }

        for( int j = 0; j < view->CountChildren(); j++ )
        {
            child = (ConfigWidget*) view->ChildAt( j );
            child->Apply( p_intf, doIt );
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
void PreferencesWindow::BuildConfigView( StringItemWithView * stringItem,
                                         module_config_t ** pp_item,
                                         bool stop_after_category )
{
    /* Build the BBox */
    BRect rect = fDummyView->Bounds();
    stringItem->fConfigBox = new BBox( rect, "config box", B_FOLLOW_ALL );
    stringItem->fConfigBox->SetLabel( stringItem->fText );

    /* Build the BView */
    rect = stringItem->fConfigBox->Bounds();
    rect.InsetBy( 10,10 );
    rect.top += 10;
    rect.right -= B_V_SCROLL_BAR_WIDTH + 5;
    stringItem->fConfigView = new BView( rect, "config view",
                                         B_FOLLOW_NONE, B_WILL_DRAW );
    stringItem->fConfigView->SetViewColor(
            ui_color( B_PANEL_BACKGROUND_COLOR ) );

    /* Add all the settings options */
    rect = stringItem->fConfigView->Bounds();
    rect.InsetBy( 10, 10 );

    ConfigTextControl * textControl;
    ConfigCheckBox    * checkBox;
    ConfigMenuField   * menuField;
    ConfigSlider      * slider;
    ConfigKey         * keyConfig;

    for( ; (*pp_item)->i_type != CONFIG_HINT_END; (*pp_item)++ )
    {
        if( stop_after_category &&
            (*pp_item)->i_type == CONFIG_HINT_CATEGORY )
        {
            break;
        }

        switch( (*pp_item)->i_type )
        {
            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_MODULE:
            case CONFIG_ITEM_DIRECTORY:
                if( (*pp_item)->ppsz_list && (*pp_item)->ppsz_list[0] )
                {
                    menuField = new ConfigMenuField( rect,
                            (*pp_item)->i_type, (*pp_item)->psz_text,
                            (*pp_item)->psz_name, (*pp_item)->ppsz_list );
                    stringItem->fConfigView->AddChild( menuField );
                    rect.top += menuField->Bounds().Height();
                }
                else
                {
                    textControl = new ConfigTextControl( rect,
                            (*pp_item)->i_type, (*pp_item)->psz_text,
                            (*pp_item)->psz_name );
                    stringItem->fConfigView->AddChild( textControl );
                    rect.top += textControl->Bounds().Height();
                }
                break;

            case CONFIG_ITEM_INTEGER:
                if( (*pp_item)->i_min == (*pp_item)->i_max )
                {
                    textControl = new ConfigTextControl( rect,
                            CONFIG_ITEM_INTEGER, (*pp_item)->psz_text,
                            (*pp_item)->psz_name );
                    stringItem->fConfigView->AddChild( textControl );
                    rect.top += textControl->Bounds().Height();
                }
                else
                {
                    slider = new ConfigSlider( rect, CONFIG_ITEM_INTEGER,
                            (*pp_item)->psz_text, (*pp_item)->psz_name,
                            (*pp_item)->i_min, (*pp_item)->i_max );
                    stringItem->fConfigView->AddChild( slider );
                    rect.top += slider->Bounds().Height();
                }
                break;

            case CONFIG_ITEM_FLOAT:
                if( (*pp_item)->f_min == (*pp_item)->f_max )
                {
                    textControl = new ConfigTextControl( rect,
                            CONFIG_ITEM_FLOAT, (*pp_item)->psz_text,
                            (*pp_item)->psz_name );
                    stringItem->fConfigView->AddChild( textControl );
                    rect.top += textControl->Bounds().Height();
                }
                else
                {
                    slider = new ConfigSlider( rect, CONFIG_ITEM_FLOAT,
                            (*pp_item)->psz_text, (*pp_item)->psz_name,
                            100 * (*pp_item)->f_min, 100 * (*pp_item)->f_max );
                    stringItem->fConfigView->AddChild( slider );
                    rect.top += slider->Bounds().Height();
                }
                break;

            case CONFIG_ITEM_BOOL:
                checkBox = new ConfigCheckBox( rect,
                        CONFIG_ITEM_BOOL, (*pp_item)->psz_text,
                        (*pp_item)->psz_name );
                stringItem->fConfigView->AddChild( checkBox );
                rect.top += checkBox->Bounds().Height();
                break;

            case CONFIG_ITEM_KEY:
                keyConfig = new ConfigKey( rect, CONFIG_ITEM_KEY,
                        (*pp_item)->psz_text, (*pp_item)->psz_name );
                stringItem->fConfigView->AddChild( keyConfig );
                rect.top += keyConfig->Bounds().Height();
        }
    }

    /* Put the BView into a BScrollView */
    stringItem->fConfigScroll =
        new BScrollView( "config scroll", stringItem->fConfigView,
                         B_FOLLOW_ALL, 0, false, true, B_FANCY_BORDER );
    stringItem->fConfigScroll->SetViewColor(
            ui_color( B_PANEL_BACKGROUND_COLOR ) );
    stringItem->fConfigBox->AddChild( stringItem->fConfigScroll );

    /* Adjust the configView size */
    stringItem->fConfigView->ResizeTo(
        stringItem->fConfigView->Bounds().Width(), rect.top );
}

ConfigWidget::ConfigWidget( BRect rect, int type, char * configName )
    : BView( rect, NULL, B_FOLLOW_ALL, B_WILL_DRAW )
{
    fConfigType = type;
    fConfigName = strdup( configName );
    SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
}

ConfigTextControl::ConfigTextControl( BRect rect, int type, char * label,
                                      char * configName )
    : ConfigWidget( BRect( rect.left, rect.top,
                           rect.right, rect.top + 25 ),
                    type, configName )
{
    fTextControl = new BTextControl( Bounds(), NULL, label, NULL,
                                     new BMessage() );
    AddChild( fTextControl );
}

void ConfigTextControl::Apply( intf_thread_t * p_intf, bool doIt )
{
    char string[1024];

    switch( fConfigType )
    {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_MODULE:
        case CONFIG_ITEM_DIRECTORY:
            if( doIt )
            {
                config_PutPsz( p_intf, fConfigName, fTextControl->Text() );
            }
            else
            {
                fTextControl->SetText( config_GetPsz( p_intf, fConfigName ) );
            }
            break;
        case CONFIG_ITEM_INTEGER:
            if( doIt )
            {
                config_PutInt( p_intf, fConfigName,
                               atoi( fTextControl->Text() ) );
            }
            else
            {
                memset( string, 0, 1024 );
                snprintf( string, 1023, "%d",
                          config_GetInt( p_intf, fConfigName ) );
                fTextControl->SetText( string );
            }
            break;
        case CONFIG_ITEM_FLOAT:
            if( doIt )
            {
                config_PutFloat( p_intf, fConfigName,
                                 strtod( fTextControl->Text(), NULL ) );
            }
            else
            {
                memset( string, 0, 1024 );
                snprintf( string, 1023, "%f",
                          config_GetFloat( p_intf, fConfigName ) );
                fTextControl->SetText( string );
            }
            break;
    }
}

ConfigCheckBox::ConfigCheckBox( BRect rect, int type, char * label,
                                char * configName )
    : ConfigWidget( BRect( rect.left, rect.top,
                           rect.right, rect.top + 25 ),
                    type, configName )
{
    fCheckBox = new BCheckBox( Bounds(), NULL, label, new BMessage() );
    AddChild( fCheckBox );
}

void ConfigCheckBox::Apply( intf_thread_t * p_intf, bool doIt )
{
    if( doIt )
    {
        config_PutInt( p_intf, fConfigName, fCheckBox->Value() );
    }
    else
    {
        fCheckBox->SetValue( config_GetInt( p_intf, fConfigName ) );
    }
}

ConfigMenuField::ConfigMenuField( BRect rect, int type, char * label,
                                  char * configName, char ** list )
    : ConfigWidget( BRect( rect.left, rect.top,
                           rect.right, rect.top + 25 ),
                    type, configName )
{
    BMenuItem * menuItem;

    fPopUpMenu = new BPopUpMenu( "" );
    fMenuField = new BMenuField( Bounds(), NULL, label, fPopUpMenu );

    for( int i = 0; list[i]; i++ )
    {
        menuItem = new BMenuItem( list[i], new BMessage() );
        fPopUpMenu->AddItem( menuItem );
    }

    AddChild( fMenuField );
}

void ConfigMenuField::Apply( intf_thread_t * p_intf, bool doIt )
{
    BMenuItem * menuItem;

    if( doIt )
    {
        menuItem = fPopUpMenu->FindMarked();
        if( menuItem )
        {
            config_PutPsz( p_intf, fConfigName, menuItem->Label() );
        }
    }
    else
    {
        char * value = config_GetPsz( p_intf, fConfigName );
        if( !value )
        {
            value = "";
        }

        for( int i = 0; i < fPopUpMenu->CountItems(); i++ )
        {
            menuItem = fPopUpMenu->ItemAt( i );
            if( !strcmp( value, menuItem->Label() ) )
            {
                menuItem->SetMarked( true );
                break;
            }
        }
    }
}

ConfigSlider::ConfigSlider( BRect rect, int type, char * label,
                            char * configName, int min, int max )
    : ConfigWidget( BRect( rect.left, rect.top,
                           rect.right, rect.top + 40 ),
                    type, configName )
{
    fSlider = new BSlider( Bounds(), NULL, label, new BMessage(),
                           min, max, B_TRIANGLE_THUMB );
    AddChild( fSlider );
}

void ConfigSlider::Apply( intf_thread_t * p_intf, bool doIt )
{
    switch( fConfigType )
    {
        case CONFIG_ITEM_INTEGER:
            if( doIt )
            {
                config_PutInt( p_intf, fConfigName, fSlider->Value() );
            }
            else
            {
                fSlider->SetValue( config_GetInt( p_intf, fConfigName ) );
            }
            break;

        case CONFIG_ITEM_FLOAT:
            if( doIt )
            {
                config_PutFloat( p_intf, fConfigName,
                                 (float) fSlider->Value() / 100.0 );
            }
            else
            {
                fSlider->SetValue( 100 *
                        config_GetFloat( p_intf, fConfigName ) );
            }
            break;
    }
}

ConfigKey::ConfigKey( BRect rect, int type, char * label,
                            char * configName )
    : ConfigWidget( BRect( rect.left, rect.top,
                           rect.right, rect.top + 25 ),
                    type, configName )
{
    BRect r = Bounds();
    BMenuItem * menuItem;

    r.left = r.right - 60;
    fPopUpMenu = new BPopUpMenu( "" );
    fMenuField = new BMenuField( r, NULL, NULL, fPopUpMenu );
    for( unsigned i = 0;
         i < sizeof( vlc_keys ) / sizeof( key_descriptor_t ); i++ )
    {
        menuItem = new BMenuItem( vlc_keys[i].psz_key_string, NULL );
        fPopUpMenu->AddItem( menuItem );
    }

    r.right = r.left - 10; r.left = r.left - 60;
    fShiftCheck = new BCheckBox( r, NULL, "Shift", new BMessage );

    r.right = r.left - 10; r.left = r.left - 60;
    fCtrlCheck = new BCheckBox( r, NULL, "Ctrl", new BMessage );

    r.right = r.left - 10; r.left = r.left - 60;
    fAltCheck = new BCheckBox( r, NULL, "Alt", new BMessage );

    /* Can someone tell me how we're supposed to get GUI items aligned ? */
    r.right = r.left - 10; r.left = 0;
    r.bottom -= 10;
    fStringView = new BStringView( r, NULL, label );

    AddChild( fStringView );
    AddChild( fAltCheck );
    AddChild( fCtrlCheck );
    AddChild( fShiftCheck );
    AddChild( fMenuField );
}

void ConfigKey::Apply( intf_thread_t * p_intf, bool doIt )
{
    BMenuItem * menuItem;

    if( doIt )
    {
        menuItem = fPopUpMenu->FindMarked();
        if( menuItem )
        {
            int value = vlc_keys[fPopUpMenu->IndexOf( menuItem )].i_key_code;
            if( fAltCheck->Value() )
            {
                value |= KEY_MODIFIER_ALT;
            }
            if( fCtrlCheck->Value() )
            {
                value |= KEY_MODIFIER_CTRL;
            }
            if( fShiftCheck->Value() )
            {
                value |= KEY_MODIFIER_SHIFT;
            }
            config_PutInt( p_intf, fConfigName, value );
        }
    }
    else
    {
        int value = config_GetInt( p_intf, fConfigName );
        fAltCheck->SetValue( value & KEY_MODIFIER_ALT );
        fCtrlCheck->SetValue( value & KEY_MODIFIER_CTRL );
        fShiftCheck->SetValue( value & KEY_MODIFIER_SHIFT );

        for( unsigned i = 0;
             i < sizeof( vlc_keys ) / sizeof( key_descriptor_t ); i++ )
        {
            if( (unsigned) vlc_keys[i].i_key_code ==
                    ( value & ~KEY_MODIFIER ) )
            {
                menuItem = fPopUpMenu->ItemAt( i );
                menuItem->SetMarked( true );
                break;
            }
        }
    }
}

