/*****************************************************************************
 * PreferencesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.cpp,v 1.14 2003/04/22 16:36:16 titer Exp $
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

/* system headers */
#include <malloc.h>
#include <string.h>

/* BeOS headers */
#include <InterfaceKit.h>
#include <Entry.h>
#include <Path.h>
#include <TranslatorRoster.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>

/* BeOS module headers */
#include "VlcWrapper.h"
#include "MsgVals.h"
#include "PreferencesWindow.h"

static const char* kTranslatorField = "be:translator";
static const char* kTypeField = "be:type";
static const char* kDefaultScreenShotPath = "/boot/home/vlc screenshot";
static const uint32 kDefaultScreenShotFormat = 'PNG ';

// add_translator_items
status_t
add_translator_items( BMenu* intoMenu, uint32 fromType, uint32 command )
{ 
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	translator_id* ids = NULL;
	int32 count = 0;
	
	status_t err = B_NO_INIT;
	if ( roster )
		err = roster->GetAllTranslators( &ids, &count );
	if ( err < B_OK )
		return err;
	for ( int32 tix = 0; tix < count; tix++ )
	{ 
		const translation_format* formats = NULL;
		int32 num_formats = 0;
		bool checkOutFormats = false; 
		err = roster->GetInputFormats( ids[tix], &formats, &num_formats );
		if ( err == B_OK )
		{
			for ( int iix = 0; iix < num_formats; iix++ )
			{
				if ( formats[iix].type == fromType )
				{ 
					checkOutFormats = true;
					break;
				}
			}
		}
		if ( !checkOutFormats )
			continue;
		err = roster->GetOutputFormats(ids[tix], &formats, &num_formats);
		if ( err == B_OK )
		{
			for ( int32 oix = 0; oix < num_formats; oix++ )
			{
	 			if ( formats[oix].type != fromType )
	 			{
					BMessage* message = new BMessage( command );
					message->AddInt32( kTranslatorField, ids[tix] );
					message->AddInt32( kTypeField, formats[oix].type );
					intoMenu->AddItem( new BMenuItem( formats[oix].name, message ) );
	 			}
			}
		} 
	}
	delete[] ids;
	return B_OK; 
}

// get_config_string
char*
get_config_string( intf_thread_t* intf, const char* field, const char* defaultString )
{
	char* string = config_GetPsz( intf, field );
	if ( !string )
	{
		string = strdup( defaultString );
		config_PutPsz( intf, field, string );
	}
	return string;
}

// get_config_int
int32
get_config_int( intf_thread_t* intf, const char* field, int32 defaultValue )
{
	int32 value = config_GetInt( intf, field );
	if ( value < 0 )
	{
		value = defaultValue;
		config_PutInt( intf, field, value );
	}
	return value;
}

// get_config_float
float
get_config_float( intf_thread_t* intf, const char* field, float defaultValue )
{
	float value = config_GetFloat( intf, field );
	if ( value < 0 )
	{
		value = defaultValue;
		config_PutFloat( intf, field, value );
	}
	return value;
}


/*****************************************************************************
 * DirectoryTextControl class
 *****************************************************************************/
class DirectoryTextControl : public BTextControl
{
 public:
	DirectoryTextControl( BRect frame, const char* name, 
						  const char* label, const char* text,
						  BMessage* message,
						  uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP,
						  uint32 flags = B_WILL_DRAW | B_NAVIGABLE );
	virtual				~DirectoryTextControl();

	virtual	void		MessageReceived(BMessage *msg);
};

DirectoryTextControl::DirectoryTextControl( BRect frame, const char* name, 
											const char* label, const char* text,
											BMessage* message,
											uint32 resizingMode, uint32 flags)
	: BTextControl( frame, name, label, text, message, resizingMode, flags )
{
}

DirectoryTextControl::~DirectoryTextControl()
{
}

/*****************************************************************************
 * DirectoryTextControl::MessageReceived
 *****************************************************************************/
void
DirectoryTextControl::MessageReceived( BMessage* message )
{
	switch ( message->what )
	{
		case B_SIMPLE_DATA:
		{
			entry_ref ref;
			if ( message->FindRef( "refs", &ref ) == B_OK ) {
				BString directory;
				BEntry entry;
				BPath path;
				if ( entry.SetTo( &ref, true ) == B_OK
					 && entry.IsDirectory()
					 && path.SetTo( &entry ) == B_OK )
				{
					SetText( path.Path() );
				}
			}
			break;
		}
		default:
			BTextControl::MessageReceived( message );
			break;
	}
}



/*****************************************************************************
 * Preferences::PreferencesWindow
 *****************************************************************************/
PreferencesWindow::PreferencesWindow( intf_thread_t * p_interface,
                                      BRect frame, const char * name )
    : BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_ZOOMABLE | B_NOT_RESIZABLE ),
	  fDVDMenusBackup( false ),
	  fPostProcessingBackup( 0 ),
	  fBrightnessBackup( 100.0 ),
	  fContrastBackup( 100.0 ),
	  fHueBackup( 0 ),
	  fSaturationBackup( 100.0 ),
	  fScreenShotPathBackup( kDefaultScreenShotPath ),
	  fScreenShotFormatBackup( kDefaultScreenShotFormat ),
	  p_intf( p_interface )
{
    BRect rect;

    /* "background" view */
    rgb_color background = ui_color( B_PANEL_BACKGROUND_COLOR );
    fPrefsView = new BView( Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    fPrefsView->SetViewColor( background );
    AddChild( fPrefsView );

    /* add the tabs */
    rect = Bounds();
    rect.top += 10.0;
    rect.bottom -= 45.0;
    fTabView = new BTabView( rect, "preferences view" );
    fTabView->SetViewColor( background );
    
    fGeneralView = new BView( fTabView->Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    fGeneralView->SetViewColor( background );
    fAdjustView = new BView( fTabView->Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    fAdjustView->SetViewColor( background );
    
    fGeneralTab = new BTab();
    fTabView->AddTab( fGeneralView, fGeneralTab );
    fGeneralTab->SetLabel( _("General") );
    
    fAdjustTab = new BTab();
    fTabView->AddTab( fAdjustView, fAdjustTab );
    fAdjustTab->SetLabel( _("Picture") );
    
    /* fills the tabs */
    /* general tab */
    rect = fGeneralView->Bounds();
    rect.InsetBy( 10, 10 );
    rect.bottom = rect.top + 10;
    fDvdMenusCheck = new BCheckBox( rect, "dvdmenus", _("Use DVD menus"),
                                  new BMessage( DVDMENUS_CHECK ) );
    fGeneralView->AddChild( fDvdMenusCheck );
    
    rect.top = rect.bottom + 20;
    rect.bottom = rect.top + 30;
    fPpSlider = new BSlider( rect, "post-processing", _("MPEG4 post-processing level"),
                               new BMessage( FFMPEG_UPDATE ),
                               0, 6, B_TRIANGLE_THUMB,
                               B_FOLLOW_LEFT, B_WILL_DRAW ); 
    fPpSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
    fPpSlider->SetHashMarkCount( 7 );
    fPpSlider->SetLimitLabels( _("None"), _("Maximum") );
    fGeneralView->AddChild( fPpSlider );


	rect.top = fPpSlider->Frame().bottom + 5.0;
	rect.bottom = rect.top + 15.0;
	fScreenShotPathTC = new DirectoryTextControl( rect, "screenshot path",
												  _("Screenshot Path:"),
												  fScreenShotPathBackup.String(),
												  new BMessage( SET_FOLDER ) );
//	fScreenShotPathTC->ResizeToPreferred();

	rect.top = fScreenShotPathTC->Frame().bottom + 5.0;
	rect.bottom = rect.top + 15.0;	// TODO: this will be so tricky to get right!
	BMenu* translatorMenu = new BMenu( "translators" );
	add_translator_items( translatorMenu, B_TRANSLATOR_BITMAP, SET_TRANSLATOR );
	fScreenShotFormatMF = new BMenuField( rect, "translators field",
										  _("Screenshot Format:"), translatorMenu );
	fScreenShotFormatMF->Menu()->SetRadioMode( true );
	fScreenShotFormatMF->Menu()->SetLabelFromMarked( true );
	// this will most likely not work for BMenuFields
//	fScreenShotFormatMF->ResizeToPreferred();

	fGeneralView->AddChild( fScreenShotPathTC );
	fGeneralView->AddChild( fScreenShotFormatMF );

	// make sure the controls labels are aligned nicely
	float labelWidthM = fScreenShotFormatMF->StringWidth( fScreenShotFormatMF->Label() ) + 5.0;
	float labelWidthP = fScreenShotPathTC->StringWidth( fScreenShotPathTC->Label() ) + 5.0;
	if ( labelWidthM > labelWidthP )
	{
		fScreenShotPathTC->SetDivider( labelWidthM );
		fScreenShotFormatMF->SetDivider( labelWidthM );
	}
	else
	{
		fScreenShotPathTC->SetDivider( labelWidthP );
		fScreenShotFormatMF->SetDivider( labelWidthP );
	}

    /* restart message */
    rect = fGeneralView->Bounds();
    rect.bottom -= 40.0;
    font_height fh;
    be_plain_font->GetHeight( &fh );
    rect.top = rect.bottom - ceilf( fh.ascent + fh.descent ) - 2.0;
    fRestartString = new BStringView( rect, NULL,
        _("DVD-menu and MPEG4 settings take effect after playback is restarted.") );
    fRestartString->SetAlignment( B_ALIGN_CENTER );
    fGeneralView->AddChild( fRestartString );

    
    /* adjust tab */
    rect = fAdjustView->Bounds();
    rect.InsetBy( 10, 10 );
    rect.bottom = rect.top + 30;
    fBrightnessSlider = new BSlider( rect, "brightness", _("Brightness"),
                                       new BMessage( ADJUST_UPDATE ),
                                       0, 200, B_TRIANGLE_THUMB,
                                       B_FOLLOW_LEFT, B_WILL_DRAW );
    rect.OffsetBy( 0, 40 );
    fContrastSlider = new BSlider( rect, "contrast", _("Contrast"),
                                     new BMessage( ADJUST_UPDATE ),
                                     0, 200, B_TRIANGLE_THUMB,
                                     B_FOLLOW_LEFT, B_WILL_DRAW );
    rect.OffsetBy( 0, 40 );
    fHueSlider = new BSlider( rect, "hue", _("Hue"),
                                new BMessage( ADJUST_UPDATE ),
                                0, 360, B_TRIANGLE_THUMB,
                                B_FOLLOW_LEFT, B_WILL_DRAW );
    rect.OffsetBy( 0, 40 );
    fSaturationSlider = new BSlider( rect, "saturation", _("Saturation"),
                                       new BMessage( ADJUST_UPDATE ),
                                       0, 200, B_TRIANGLE_THUMB,
                                       B_FOLLOW_LEFT, B_WILL_DRAW );
    fAdjustView->AddChild( fBrightnessSlider );
    fAdjustView->AddChild( fContrastSlider );
    fAdjustView->AddChild( fHueSlider );
    fAdjustView->AddChild( fSaturationSlider );
    
    fPrefsView->AddChild( fTabView );

    /* buttons */
    BButton *button;
    rect = Bounds();
    rect.InsetBy( 10, 10 );
    rect.top = rect.bottom - 25;
    rect.left = rect.right - 80;
    button = new BButton( rect, NULL, _("OK"), new BMessage( PREFS_OK ) );
    fPrefsView->AddChild( button );

	SetDefaultButton( button );

    rect.OffsetBy( -90, 0 );
    button = new BButton( rect, NULL, _("Cancel"), new BMessage( PREFS_CANCEL ) );
    fPrefsView->AddChild( button );

    rect.OffsetBy( -90, 0 );
    button = new BButton( rect, NULL, _("Revert"), new BMessage( PREFS_REVERT ) );
    fPrefsView->AddChild( button );

	rect.left = Bounds().left + 10.0;
	rect.right = rect.left + 80.0;
    button = new BButton( rect, NULL, _("Defaults"), new BMessage( PREFS_DEFAULTS ) );
    fPrefsView->AddChild( button );


	// sync GUI to VLC 
	_SetToSettings();

    // start window thread in hidden state
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
bool
PreferencesWindow::QuitRequested()
{
	// work arround problem when window is closed or Ok pressed though
	// the text control has focus (it will not have commited changes)
	config_PutPsz( p_intf, "beos-screenshot-path", fScreenShotPathTC->Text() );
	if ( !IsHidden() )
		Hide();
	return false;
}

/*****************************************************************************
 * PreferencesWindow::MessageReceived
 *****************************************************************************/
void PreferencesWindow::MessageReceived( BMessage * p_message )
{
    switch ( p_message->what )
    {
    	case SET_TRANSLATOR:
    	case SET_FOLDER:
    		_ApplyScreenShotSettings();
    		break;
        case DVDMENUS_CHECK:
        	_ApplyDVDSettings();
        	break;
        case ADJUST_UPDATE:
        	_ApplyPictureSettings();
        	break;
        case FFMPEG_UPDATE:
	        _ApplyFFmpegSettings();
            break;
        case PREFS_REVERT:
        	_RevertChanges();
        	break;
        case PREFS_DEFAULTS:
            _SetDefaults();
            _ApplyChanges();
            break;
        case PREFS_CANCEL:
        	_RevertChanges();
        	// fall through
        case PREFS_OK:
            PostMessage( B_QUIT_REQUESTED );
            break;
        default:
            BWindow::MessageReceived( p_message );
            break;
    }
}

/*****************************************************************************
 * PreferencesWindow::Show
 *****************************************************************************/
void
PreferencesWindow::Show()
{
	// collect settings for backup
    fDVDMenusBackup = fDvdMenusCheck->Value() == B_CONTROL_ON;
    fPostProcessingBackup = fPpSlider->Value();
    fBrightnessBackup = fBrightnessSlider->Value();
    fContrastBackup = fContrastSlider->Value();
    fHueBackup = fHueSlider->Value();
    fSaturationBackup = fSaturationSlider->Value();
    fScreenShotPathBackup.SetTo( fScreenShotPathTC->Text() );
	if ( BMenuItem* item = fScreenShotFormatMF->Menu()->FindMarked() )
	{
		BMessage* message = item->Message();
		if ( message && message->FindInt32( kTypeField,
							(int32*)&fScreenShotFormatBackup ) != B_OK )
			fScreenShotFormatBackup = kDefaultScreenShotFormat;
	}
	else
	    fScreenShotFormatBackup = kDefaultScreenShotFormat;

	BWindow::Show();
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

void
PreferencesWindow::_SetGUI( bool dvdMenus, int32 postProcessing,
						   float brightness, float contrast,
						   int32 hue, float saturation,
						   const char* screenShotPath,
						   uint32 screenShotTranslator)
{
	fDvdMenusCheck->SetValue( dvdMenus );
	fPpSlider->SetValue( postProcessing );
	fBrightnessSlider->SetValue( brightness );
	fContrastSlider->SetValue( contrast );
	fHueSlider->SetValue( hue );
	fSaturationSlider->SetValue( saturation );
	// mark appropriate translator item
	bool found = false;
	for ( int32 i = 0; BMenuItem* item = fScreenShotFormatMF->Menu()->ItemAt( i ); i++ )
	{
		if ( BMessage* message = item->Message() )
		{
			uint32 format;
			if ( message->FindInt32( kTypeField, (int32*)&format ) == B_OK
				 && format == screenShotTranslator )
			{
				item->SetMarked( true );
				found = true;
				break;
			}
		}
	}
	if ( !found )
	{
		if ( BMenuItem* item = fScreenShotFormatMF->Menu()->ItemAt( 0 ) )
			item->SetMarked( true );
	}
	fScreenShotPathTC->SetText( screenShotPath );
}


/*****************************************************************************
 * PreferencesWindow::_SetDefaults
 *****************************************************************************/
void PreferencesWindow::_SetDefaults()
{
	_SetGUI( false, 0, 100.0, 100.0, 0, 100.0,
			kDefaultScreenShotPath, kDefaultScreenShotFormat );
}

/*****************************************************************************
 * PreferencesWindow::_SetToSettings
 *****************************************************************************/
void PreferencesWindow::_SetToSettings()
{
	char* path = get_config_string( p_intf, "beos-screenshot-path", kDefaultScreenShotPath );

	p_intf->p_sys->b_dvdmenus = get_config_int( p_intf, "beos-use-dvd-menus", false );

	_SetGUI( p_intf->p_sys->b_dvdmenus,
			get_config_int( p_intf, "ffmpeg-pp-q", 0 ),
			100 *  get_config_float( p_intf, "brightness", 1.0 ),
			100 * get_config_float( p_intf, "contrast", 1.0 ),
			get_config_int( p_intf, "hue", 0 ),
			100 * get_config_float( p_intf, "saturation", 1.0 ),
			path,
			get_config_int( p_intf, "beos-screenshot-format",
							kDefaultScreenShotFormat ) );
	free( path );
}

/*****************************************************************************
 * PreferencesWindow::_RevertChanges
 *****************************************************************************/
void
PreferencesWindow::_RevertChanges()
{
	_SetGUI( fDVDMenusBackup,
			fPostProcessingBackup,
			fBrightnessBackup,
			fContrastBackup,
			fHueBackup,
			fSaturationBackup,
			fScreenShotPathBackup.String(),
			fScreenShotFormatBackup );

	_ApplyChanges();
}

/*****************************************************************************
 * PreferencesWindow::_ApplyChanges
 *****************************************************************************/
void PreferencesWindow::_ApplyChanges()
{
	_ApplyScreenShotSettings();
	_ApplyPictureSettings();
	_ApplyFFmpegSettings();
	_ApplyDVDSettings();
}

/*****************************************************************************
 * PreferencesWindow::_ApplyScreenShotSettings
 *****************************************************************************/
void
PreferencesWindow::_ApplyScreenShotSettings()
{
	// screen shot settings
	uint32 translator = kDefaultScreenShotFormat;
	if ( BMenuItem* item = fScreenShotFormatMF->Menu()->FindMarked() )
	{
		BMessage* message = item->Message();
		if ( message && message->FindInt32( kTypeField, (int32*)&translator ) != B_OK )
			translator = kDefaultScreenShotFormat;
	}
	config_PutInt( p_intf, "beos-screenshot-format", translator );
	config_PutPsz( p_intf, "beos-screenshot-path", fScreenShotPathTC->Text() );
}

/*****************************************************************************
 * PreferencesWindow::_ApplyPictureSettings
 *****************************************************************************/
void
PreferencesWindow::_ApplyPictureSettings()
{
	VlcWrapper* p_wrapper = p_intf->p_sys->p_wrapper;

	// picture adjustment settings
    config_PutFloat( p_intf, "brightness",
                     (float)fBrightnessSlider->Value() / 100 );
    config_PutFloat( p_intf, "contrast",
                     (float)fContrastSlider->Value() / 100 );
    config_PutInt( p_intf, "hue", fHueSlider->Value() );
    config_PutFloat( p_intf, "saturation",
                     (float)fSaturationSlider->Value() / 100 );

	// take care of changing "filters on the fly"
    if( config_GetFloat( p_intf, "brightness" ) != 1 ||
        config_GetFloat( p_intf, "contrast" ) != 1 ||
        config_GetInt( p_intf, "hue" ) != 0 ||
        config_GetFloat( p_intf, "saturation" ) != 1 )
    {
    	char* string = config_GetPsz( p_intf, "filter" );
        if( !string || strcmp( string, "adjust" ) )
        {
            config_PutPsz( p_intf, "filter", "adjust" );
            p_wrapper->FilterChange();
        }
        if ( string )
        	free( string );
    }
    else
    {
    	char* string = config_GetPsz( p_intf, "filter" );
        if ( string )
        {
            config_PutPsz( p_intf, "filter", NULL );
            p_wrapper->FilterChange();
        	free( string );
        }
    }
}

/*****************************************************************************
 * PreferencesWindow::_ApplyFFmpegSettings
 *****************************************************************************/
void
PreferencesWindow::_ApplyFFmpegSettings()
{
	// ffmpeg post processing
    config_PutInt( p_intf, "ffmpeg-pp-q", fPpSlider->Value() );
}

/*****************************************************************************
 * PreferencesWindow::_ApplyDVDSettings
 *****************************************************************************/
void
PreferencesWindow::_ApplyDVDSettings()
{
	// dvd menus
	bool dvdMenus = fDvdMenusCheck->Value() == B_CONTROL_ON;
	p_intf->p_sys->b_dvdmenus = dvdMenus;
	config_PutInt( p_intf, "beos-use-dvd-menus", dvdMenus );
}

