/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.h,v 1.2 2002/11/23 15:00:54 titer Exp $
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
#define PREFS_CANCEL   'prca'
#define PREFS_DEFAULTS 'prde'
#define SLIDER_UPDATE  'slup'

class PreferencesWindow : public BWindow
{
    public:
                         PreferencesWindow( BRect frame,
                                            const char* name,
                                            intf_thread_t *p_interface );
        virtual          ~PreferencesWindow();
        virtual void     MessageReceived(BMessage *message);
        void             ReallyQuit();

    private:
        void             SetDefaults();
        void             ApplyChanges();
        BView *          p_prefs_view;
        BTabView *       p_tabview;
        BView *          p_ffmpeg_view;
        BView *          p_adjust_view;
        BTab *           p_ffmpeg_tab;
        BTab *           p_adjust_tab;
        BSlider *        p_pp_slider;
        BSlider *        p_contrast_slider;
        BSlider *        p_brightness_slider;
        BSlider *        p_hue_slider;
        BSlider *        p_saturation_slider;
        BStringView *    p_restart_string;

        intf_thread_t *  p_intf;
};

#endif	// BEOS_PREFERENCES_WINDOW_H

