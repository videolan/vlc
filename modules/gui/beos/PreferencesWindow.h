/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.h,v 1.1 2002/10/28 17:18:18 titer Exp $
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

#define PREFS_OK      'prok'
#define PREFS_CANCEL  'prca'
#define SLIDER_UPDATE 'slup'

class PreferencesWindow : public BWindow
{
    public:
                         PreferencesWindow( BRect frame,
                                            const char* name,
                                            intf_thread_t *p_interface );
        virtual          ~PreferencesWindow();
        virtual bool     QuitRequested();
        virtual void     MessageReceived(BMessage *message);
        virtual void     FrameResized(float width, float height);
        void             ReallyQuit();

    private:
        void             CancelChanges();
        void             ApplyChanges();
        intf_thread_t *  p_intf;
        BView *          p_preferences_view;
        BSlider *        p_pp_slider;
        BSlider *        p_lum_slider;
        BStringView *    p_restart_string;
};

#endif	// BEOS_PREFERENCES_WINDOW_H

