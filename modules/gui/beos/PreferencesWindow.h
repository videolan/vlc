/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.h,v 1.8 2003/01/27 10:29:22 titer Exp $
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

#define PREFS_WINDOW_WIDTH   400
#define PREFS_WINDOW_HEIGHT  280

#define PREFS_OK       'prok'
#define PREFS_SAVE     'prsa'
#define PREFS_DEFAULTS 'prde'
#define SLIDER_UPDATE  'slup'
#define DVDMENUS_CHECK 'dvme'

class PreferencesWindow : public BWindow
{
    public:
                         PreferencesWindow( intf_thread_t * p_intf,
                                            BRect frame,
                                            const char * name );
        virtual          ~PreferencesWindow();
        virtual void     MessageReceived(BMessage *message);
        void             ReallyQuit();

    private:
        void             SetDefaults();
        void             ApplyChanges();
        BView *          fPrefsView;
        BTabView *       fTabView;
        BView *          fGeneralView;
        BView *          fAdjustView;
        BTab *           fGeneralTab;
        BTab *           fAdjustTab;
        BCheckBox *      fDvdMenusCheck;
        BSlider *        fPpSlider;
        BSlider *        fContrastSlider;
        BSlider *        fBrightnessSlider;
        BSlider *        fHueSlider;
        BSlider *        fSaturationSlider;
        BStringView *    fRestartString;

        intf_thread_t *  p_intf;
};

#endif    // BEOS_PREFERENCES_WINDOW_H

