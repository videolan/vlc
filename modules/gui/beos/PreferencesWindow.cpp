/*****************************************************************************
 * PreferencesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Eric Petit <titer@m0k.org>
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
#include <vlc_config_cat.h>

#include "PreferencesWindow.h"

#define TYPE_CATEGORY 0
#define TYPE_SUBCATEGORY 2
#define TYPE_MODULE 3

/*****************************************************************************
 * PreferencesWindow::PreferencesWindow
 *****************************************************************************/
PreferencesWindow::PreferencesWindow( intf_thread_t * _p_intf,
                                      BRect frame, const char * name )
    : BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_ZOOMABLE )
{
    p_intf   = _p_intf;
    fCurrent = NULL;

    BRect rect;

    SetSizeLimits( PREFS_WINDOW_WIDTH, 2000, PREFS_WINDOW_HEIGHT, 2000 );

    /* The "background" view */
    fPrefsView = new BView( Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    fPrefsView->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    AddChild( fPrefsView );

    /* Create a scrollable outline view for the preferences tree */
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

    /* Create a dummy, empty view so we can correctly place the real
       config views later */
    rect.bottom -= 40;
    rect.left = rect.right + 15 + B_V_SCROLL_BAR_WIDTH;
    rect.right = Bounds().right - 15;
    fDummyView = new BView( rect, "", B_FOLLOW_ALL_SIDES, B_WILL_DRAW );
    fDummyView->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    fPrefsView->AddChild( fDummyView );

    /* Fill the tree */
    vlc_list_t * p_list;
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    if( !p_list )
    {
        msg_Warn( p_intf, "couldn't find any module !" );
        return;
    }

    /* Find the main module */
    module_t * p_module = NULL;
    module_config_t * p_item = NULL;
    for( int i = 0; i < p_list->i_count; i++ )
    {
        p_module = (module_t*) p_list->p_values[i].p_object;

        if( !strcmp( p_module->psz_object_name, "main" ) &&
            ( p_item = p_module->p_config ) )
            break;
        else
            p_module = NULL;
    }

    ConfigItem * catItem = NULL, * subcatItem, * otherItem;

    if( p_module )
    {
        /* We found the main module, build the category tree */
        for( ; p_item->i_type != CONFIG_HINT_END; p_item++ )
        {
            switch( p_item->i_type )
            {
                case CONFIG_CATEGORY:
                    catItem = new ConfigItem( p_intf,
                        config_CategoryNameGet( p_item->i_value ),
                        false,
                        p_item->i_value,
                        TYPE_CATEGORY,
                        config_CategoryHelpGet( p_item->i_value ) );
                    fOutline->AddItem( catItem );
                    break;

                case CONFIG_SUBCATEGORY:
                    if( catItem )
                    {
                        subcatItem = new ConfigItem( p_intf,
                            config_CategoryNameGet( p_item->i_value ),
                            false,
                            p_item->i_value,
                            TYPE_SUBCATEGORY,
                            config_CategoryHelpGet( p_item->i_value ) );
                        fOutline->AddUnder( subcatItem, catItem );
                    }
                    else
                    {
                        msg_Warn( p_intf, "subcategory without a category" );
                    }
                    break;
            }
        }
    }

    /* Now parse all others modules */

    int category, subcategory, options;

    for( int i = 0; i < p_list->i_count; i++ )
    {
        category    = -1;
        subcategory = -1;
        options     = 0;

        p_module = (module_t*) p_list->p_values[i].p_object;

        if( !strcmp( p_module->psz_object_name, "main" ) )
            continue;

        if( p_module->b_submodule ||
            !( p_item = p_module->p_config ) )
            continue;

        for( ; p_item->i_type != CONFIG_HINT_END; p_item++ )
        {
            switch( p_item->i_type )
            {
                case CONFIG_CATEGORY:
                    category = p_item->i_value;
                    break;
                case CONFIG_SUBCATEGORY:
                    subcategory = p_item->i_value;
                    break;
                default:
                    if( p_item->i_type & CONFIG_ITEM )
                        options++;
            }
            if( options > 0 && category >= 0 && subcategory >= 0 )
            {
                break;
            }
        }

        if( options < 1 || category < 0 || subcategory < 0 )
            continue;

        catItem = NULL;
        for( int j = 0; j < fOutline->CountItemsUnder( NULL, true ); j++ )
        {
            catItem = (ConfigItem*)
                fOutline->ItemUnderAt( NULL, true, j );
            if( catItem->ObjectId() == category )
                break;
            else
                catItem = NULL;
        }

        if( !catItem )
            continue;

        subcatItem = NULL;
        for( int j = 0; j < fOutline->CountItemsUnder( catItem, true ); j++ )
        {
            subcatItem = (ConfigItem*)
                fOutline->ItemUnderAt( catItem, true, j );
            if( subcatItem->ObjectId() == subcategory )
                break;
            else
                subcatItem = NULL;
        }

        if( !subcatItem )
            subcatItem = catItem;

        otherItem = new ConfigItem( p_intf,
            p_module->psz_shortname ?
              p_module->psz_shortname : p_module->psz_object_name,
            p_module->b_submodule,
            p_module->b_submodule ?
              ((module_t *)p_module->p_parent)->i_object_id :
              p_module->i_object_id,
            TYPE_MODULE,
            NULL );
        fOutline->AddUnder( otherItem, subcatItem );
    }

    vlc_list_release( p_list );

    /* Collapse the whole tree */
    for( int i = 0; i < fOutline->FullListCountItems(); i++ )
    {
        otherItem = (ConfigItem *) fOutline->FullListItemAt( i );
        fOutline->Collapse( otherItem );
    }

    /* Set the correct values */
    Apply( false );

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
            config_SaveConfigFile( p_intf, NULL );
            Apply( false );
            break;

        case PREFS_APPLY:
            Apply( true );
            break;

        case PREFS_SAVE:
            Apply( true );
            config_SaveConfigFile( p_intf, NULL );
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
    fCurrent->UpdateScrollBar();
}

/*****************************************************************************
 * PreferencesWindow::Update
 *****************************************************************************/
void PreferencesWindow::Update()
{
    /* Get the selected item, if any */
    if( fOutline->CurrentSelection() < 0 )
        return;

    /* Detach the old box if any */
    if( fCurrent )
    {
        fCurrent->ResetScroll();
        fDummyView->RemoveChild( fCurrent->Box() );
    }

    /* Add the new one... */
    fCurrent = (ConfigItem *)
        fOutline->ItemAt( fOutline->CurrentSelection() );
    fDummyView->AddChild( fCurrent->Box() );

    /* ...then resize it (we must resize it after it's attached or the
       children don't get adjusted) */
    fCurrent->Box()->ResizeTo( fDummyView->Bounds().Width(),
                               fDummyView->Bounds().Height() );
    fCurrent->UpdateScrollBar();
}

/*****************************************************************************
 * PreferencesWindow::Apply
 * Apply changes if doIt is true, revert them otherwise
 *****************************************************************************/
void PreferencesWindow::Apply( bool doIt )
{
    ConfigItem * item;

    for( int i = 0; i < fOutline->FullListCountItems(); i++ )
    {
        item = (ConfigItem*) fOutline->FullListItemAt( i );
        item->Apply( doIt );
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

/***********************************************************************
 * ConfigItem::ConfigItem
 ***********************************************************************
 *
 **********************************************************************/
ConfigItem::ConfigItem( intf_thread_t * _p_intf, char * name,
                        bool subModule, int objectId,
                        int type, char * help )
    : BStringItem( name )
{
    p_intf     = _p_intf;
    fSubModule = subModule;
    fObjectId  = objectId;
    fType      = type;
    fHelp      = strdup( help );

    BRect r;
    r = BRect( 0, 0, 100, 100 );
    fBox = new BBox( r, NULL, B_FOLLOW_ALL );
    fBox->SetLabel( name );

    fTextView = NULL;
    fScroll   = NULL;
    fView     = NULL;

    if( fType == TYPE_CATEGORY )
    {
        /* Category: we just show the help text */
        r = fBox->Bounds();
        r.InsetBy( 10, 10 );
        r.top += 5;

        fTextView = new VTextView( r, NULL, B_FOLLOW_ALL, B_WILL_DRAW);
        fTextView->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
        fTextView->MakeEditable( false );
        fTextView->MakeSelectable( false );
        fTextView->Insert( fHelp );
        fBox->AddChild( fTextView );

        return;
    }

    vlc_list_t * p_list = NULL;
    module_t * p_module = NULL;
    if( fType == TYPE_MODULE )
    {
        p_module = (module_t *) vlc_object_get( p_intf, fObjectId );
    }
    else
    {
        if( !( p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE,
                                       FIND_ANYWHERE ) ) )
        {
            return;
        }
        for( int i = 0; i < p_list->i_count; i++ )
        {
            p_module = (module_t*) p_list->p_values[i].p_object;

            if( !strcmp( p_module->psz_object_name, "main" ) )
                break;
            else
                p_module = NULL;
        }
    }

    if( !p_module || p_module->i_object_type != VLC_OBJECT_MODULE )
    {
        /* Shouldn't happen */
        return;
    }

    module_config_t * p_item;
    p_item = fSubModule ? ((module_t *)p_module->p_parent)->p_config :
               p_module->p_config;

    if( fType == TYPE_SUBCATEGORY )
    {
        for( ; p_item->i_type != CONFIG_HINT_END; p_item++ )
        {
            if( p_item->i_type == CONFIG_SUBCATEGORY &&
                p_item->i_value == fObjectId )
            {
                break;
            }
        }
    }

    r = fBox->Bounds();
    r = BRect( 10,20,fBox->Bounds().right-B_V_SCROLL_BAR_WIDTH-10,
               fBox->Bounds().bottom-10 );
    fView = new BView( r, NULL, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
                       B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE );
    fView->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );

    r = fView->Bounds();
    r.InsetBy( 10,10 );

    ConfigWidget * widget;
    for( ; p_item->i_type != CONFIG_HINT_END; p_item++ )
    {
        if( ( p_item->i_type == CONFIG_CATEGORY ||
              p_item->i_type == CONFIG_SUBCATEGORY ) &&
            fType == TYPE_SUBCATEGORY &&
            p_item->i_value != fObjectId )
        {
            break;
        }

        widget = new ConfigWidget( p_intf, r, p_item );
        if( !widget->InitCheck() )
        {
            delete widget;
            continue;
        }
        fView->AddChild( widget );
        r.top += widget->Bounds().Height();
    }

    if( fType == TYPE_MODULE )
    {
        vlc_object_release( p_module );
    }
    else
    {
        vlc_list_release( p_list );
    }

    /* Create a scroll view around our fView */
    fScroll = new BScrollView( NULL, fView, B_FOLLOW_ALL, 0, false,
                               true, B_FANCY_BORDER );
    fScroll->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    fBox->AddChild( fScroll );

    /* Adjust fView's height to the size it actually needs (we do this
       only now so the BScrollView fits the BBox) */
    fView->ResizeTo( fView->Bounds().Width(), r.top + 10 );
}

/***********************************************************************
 * ConfigItem::~ConfigItem
 ***********************************************************************
 *
 **********************************************************************/
ConfigItem::~ConfigItem()
{
    if( fHelp )
    {
        free( fHelp );
    }
}

/*****************************************************************************
 * ConfigItem::UpdateScrollBar
 *****************************************************************************/
void ConfigItem::UpdateScrollBar()
{
    /* We have to fix the scrollbar manually because it doesn't handle
       correctly simple BViews */

    if( !fScroll )
    {
        return;
    }

    /* Get the available BRect for display */
    BRect display = fScroll->Bounds();
    display.right -= B_V_SCROLL_BAR_WIDTH;

    /* Fix the scrollbar */
    BScrollBar * scrollBar;
    BRect visible = display & fView->Bounds();
    BRect total   = display | fView->Bounds();
    scrollBar = fScroll->ScrollBar( B_VERTICAL );
    long max = (long)( fView->Bounds().Height() - visible.Height() );
    if( max < 0 ) max = 0;
    scrollBar->SetRange( 0, max );
    scrollBar->SetProportion( visible.Height() / total.Height() );
    scrollBar->SetSteps( 10, 100 );

    /* We have to force redraw to avoid visual bugs when resizing
       (BeOS bug?) */
    fScroll->Invalidate();
    fView->Invalidate();
}

/*****************************************************************************
 * ConfigItem::ResetScroll
 *****************************************************************************/
void ConfigItem::ResetScroll()
{
    if( !fScroll )
    {
        return;
    }

    fView->ScrollTo( 0, 0 );
}

/***********************************************************************
 * ConfigItem::Apply
 ***********************************************************************
 *
 **********************************************************************/
void ConfigItem::Apply( bool doIt )
{
    if( !fScroll )
    {
        return;
    }

    /* Call ConfigWidget::Apply for every child of your fView */
    ConfigWidget * widget;
    for( int i = 0; i < fView->CountChildren(); i++ )
    {
        widget = (ConfigWidget*) fView->ChildAt( i );
        widget->Apply( doIt );
    }
}

/***********************************************************************
 * ConfigWidget::ConfigWidget
 ***********************************************************************
 * Builds a view with the right controls for the given config variable.
 *  rect: the BRect where we place ourselves. All we care is its width
 *    and its top coordinate, since we adapt our height to take only
 *    the place we need
 **********************************************************************/
ConfigWidget::ConfigWidget( intf_thread_t * _p_intf, BRect rect,
                            module_config_t * p_item )
    : BView( rect, NULL, B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
             B_WILL_DRAW )
{
    p_intf = _p_intf;

    SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );

    BRect r;
    BMenuItem * menuItem;
    /* Skip deprecated options */
    if( p_item->psz_current )
    {
        fInitOK = false;
        return;
    }

    fInitOK = true;

    fType = p_item->i_type;
    fName = strdup( p_item->psz_name );

    switch( fType )
    {
        case CONFIG_ITEM_MODULE:
        case CONFIG_ITEM_MODULE_CAT:
        case CONFIG_ITEM_MODULE_LIST_CAT:
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_DIRECTORY:
        case CONFIG_ITEM_INTEGER:
        case CONFIG_ITEM_FLOAT:
            ResizeTo( Bounds().Width(), 25 );
            fTextControl = new VTextControl( Bounds(), NULL,
                p_item->psz_text, NULL, new BMessage(),
                B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP );
            AddChild( fTextControl );
            break;
        case CONFIG_ITEM_KEY:
            ResizeTo( Bounds().Width(), 25 );
            r = Bounds();
            r.left = r.right - 100;
            fPopUpMenu = new BPopUpMenu( "" );
            fMenuField = new BMenuField( r, NULL, NULL, fPopUpMenu,
                B_FOLLOW_RIGHT | B_FOLLOW_TOP );
            for( unsigned i = 0;
                 i < sizeof( vlc_keys ) / sizeof( key_descriptor_t );
                 i++ )
            {
                menuItem = new BMenuItem( vlc_keys[i].psz_key_string, NULL );
                fPopUpMenu->AddItem( menuItem );
            }
            r.right = r.left - 10; r.left = r.left - 60;
            fShiftCheck = new BCheckBox( r, NULL, "Shift",
                new BMessage(), B_FOLLOW_RIGHT | B_FOLLOW_TOP );
            r.right = r.left - 10; r.left = r.left - 60;
            fCtrlCheck = new BCheckBox( r, NULL, "Ctrl",
                new BMessage(), B_FOLLOW_RIGHT | B_FOLLOW_TOP );
            r.right = r.left - 10; r.left = r.left - 60;
            fAltCheck = new BCheckBox( r, NULL, "Alt",
                new BMessage(), B_FOLLOW_RIGHT | B_FOLLOW_TOP );
            r.right = r.left - 10; r.left = 0; r.bottom -= 10;
            fStringView = new BStringView( r, NULL, p_item->psz_text,
                B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP );
            AddChild( fStringView );
            AddChild( fAltCheck );
            AddChild( fCtrlCheck );
            AddChild( fShiftCheck );
            AddChild( fMenuField );
            break;
        case CONFIG_ITEM_BOOL:
            ResizeTo( Bounds().Width(), 25 );
            fCheckBox = new BCheckBox( Bounds(), NULL, p_item->psz_text,
                new BMessage(), B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP );
            AddChild( fCheckBox );
            break;
        case CONFIG_SECTION:
            fInitOK = false;
            break;
        default:
            fInitOK = false;
    }
}

ConfigWidget::~ConfigWidget()
{
    free( fName );
}

/***********************************************************************
 * ConfigWidget::Apply
 ***********************************************************************
 *
 **********************************************************************/
void ConfigWidget::Apply( bool doIt )
{
    BMenuItem * menuItem;
    char string[256];
    vlc_value_t val;

    switch( fType )
    {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_MODULE:
        case CONFIG_ITEM_MODULE_CAT:
        case CONFIG_ITEM_MODULE_LIST_CAT:
        case CONFIG_ITEM_DIRECTORY:
            if( doIt )
            {
                config_PutPsz( p_intf, fName, fTextControl->Text() );
            }
            else
            {
                fTextControl->SetText( config_GetPsz( p_intf, fName ) );
            }
            break;

        case CONFIG_ITEM_INTEGER:
            if( doIt )
            {
                config_PutInt( p_intf, fName, atoi( fTextControl->Text() ) );
            }
            else
            {
                snprintf( string, 256, "%d", config_GetInt( p_intf, fName ) );
                fTextControl->SetText( string );
            }
            break;

        case CONFIG_ITEM_FLOAT:
            if( doIt )
            {
                config_PutFloat( p_intf, fName, atof( fTextControl->Text() ) );
            }
            else
            {
                snprintf( string, 256, "%f", config_GetFloat( p_intf, fName ) );
                fTextControl->SetText( string );
            }
            break;

        case CONFIG_ITEM_KEY:
            if( doIt )
            {
                menuItem = fPopUpMenu->FindMarked();
                if( menuItem )
                {
                    val.i_int = vlc_keys[fPopUpMenu->IndexOf( menuItem )].i_key_code;
                    if( fAltCheck->Value() )
                    {
                        val.i_int |= KEY_MODIFIER_ALT;
                    }
                    if( fCtrlCheck->Value() )
                    {
                        val.i_int |= KEY_MODIFIER_CTRL;
                    }
                    if( fShiftCheck->Value() )
                    {
                        val.i_int |= KEY_MODIFIER_SHIFT;
                    }
                    var_Set( p_intf->p_vlc, fName, val );
                }
            }
            else
            {
                val.i_int = config_GetInt( p_intf, fName );
                fAltCheck->SetValue( val.i_int & KEY_MODIFIER_ALT );
                fCtrlCheck->SetValue( val.i_int & KEY_MODIFIER_CTRL );
                fShiftCheck->SetValue( val.i_int & KEY_MODIFIER_SHIFT );
        
                for( unsigned i = 0;
                     i < sizeof( vlc_keys ) / sizeof( key_descriptor_t ); i++ )
                {
                    if( (unsigned) vlc_keys[i].i_key_code ==
                            ( val.i_int & ~KEY_MODIFIER ) )
                    {
                        menuItem = fPopUpMenu->ItemAt( i );
                        menuItem->SetMarked( true );
                        break;
                    }
                }
            }
            break;

        case CONFIG_ITEM_BOOL:
            if( doIt )
            {
                config_PutInt( p_intf, fName, fCheckBox->Value() );
            }
            else
            {
                fCheckBox->SetValue( config_GetInt( p_intf, fName ) );
            }
            break;

        default:
            break;
    }
}

VTextView::VTextView( BRect frame, const char *name,
                      uint32 resizingMode, uint32 flags )
    : BTextView( frame, name, BRect( 10,10,10,10 ), resizingMode, flags )
{
    FrameResized( Bounds().Width(), Bounds().Height() );
}

void VTextView::FrameResized( float width, float height )
{
    BTextView::FrameResized( width, height );
    SetTextRect( BRect( 10,10, width-11, height-11 ) );
}

VTextControl::VTextControl( BRect frame, const char *name,
                            const char *label, const char *text,
                            BMessage * message, uint32 resizingMode )
    : BTextControl( frame, name, label, text, message, resizingMode )
{
    FrameResized( Bounds().Width(), Bounds().Height() );
}

void VTextControl::FrameResized( float width, float height )
{
    BTextControl::FrameResized( width, height );
    SetDivider( width / 2 );
}
