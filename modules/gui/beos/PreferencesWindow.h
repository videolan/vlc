/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.h,v 1.9 2003/02/09 17:10:52 stippi Exp $
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

#ifndef BEOS_PREFERENCES_WINDOW_H
#define BEOS_PREFERENCES_WINDOW_H

#include <Window.h>
#include <String.h>

#define PREFS_WINDOW_WIDTH   400
#define PREFS_WINDOW_HEIGHT  280

#define PREFS_OK       'prok'
#define PREFS_CANCEL   'prcb'
#define PREFS_DEFAULTS 'prde'
#define PREFS_REVERT   'prrv'
#define FFMPEG_UPDATE  'ffup'
#define ADJUST_UPDATE  'ajst'
#define DVDMENUS_CHECK 'dvme'
#define SET_TRANSLATOR 'sttr'
#define SET_FOLDER 'stdr'

class BTabView;
class BCheckBox;
class BSlider;
class BStringView;
class BMenuField;
class BTextControl;

class PreferencesWindow : public BWindow
{
 public:
								PreferencesWindow( intf_thread_t* p_intf,
												   BRect frame,
												   const char* name );
	virtual						~PreferencesWindow();

	virtual	bool				QuitRequested();
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				Show();

			void				ReallyQuit();

 private:
			void				_SetGUI( bool dvdMenus,
										 int32 postProcessing,
										 float brightness,
										 float contrast,
										 int32 hue,
										 float saturation,
										 const char* screenShotPath,
										 uint32 screenShotTranslator );
			void				_SetDefaults();
			void				_SetToSettings();
			void				_RevertChanges();

			void				_ApplyChanges();

			void				_ApplyScreenShotSettings();
			void				_ApplyPictureSettings();
			void				_ApplyFFmpegSettings();
			void				_ApplyDVDSettings();

	BView*						fPrefsView;
	BTabView*					fTabView;
	BView*						fGeneralView;
	BView*						fAdjustView;
	BTab*						fGeneralTab;
	BTab*						fAdjustTab;
	BCheckBox*					fDvdMenusCheck;
	BSlider*					fPpSlider;
	BSlider*					fContrastSlider;
	BSlider*					fBrightnessSlider;
	BSlider*					fHueSlider;
	BSlider*					fSaturationSlider;
	BStringView*				fRestartString;
	BMenuField*					fScreenShotFormatMF;
	BTextControl*				fScreenShotPathTC;

	bool						fDVDMenusBackup;
	int32						fPostProcessingBackup;
	float						fBrightnessBackup;
	float						fContrastBackup;
	int32						fHueBackup;
	float						fSaturationBackup;
	BString						fScreenShotPathBackup;
	uint32						fScreenShotFormatBackup;

	intf_thread_t*				p_intf;
};

// some global support functions
int32
get_config_int( intf_thread_t* intf,
				const char* field,
				int32 defaultValue );

float
get_config_float( intf_thread_t* intf,
				  const char* field,
				  float defaultValue );

// don't leak the return value! (use free())
char*
get_config_string( intf_thread_t* intf,
				   const char* field,
				   const char* defaultString );

#endif    // BEOS_PREFERENCES_WINDOW_H

