/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.h,v 1.10 2003/05/03 13:37:21 titer Exp $
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

#define PREFS_WINDOW_WIDTH   600
#define PREFS_WINDOW_HEIGHT  300
#define PREFS_ITEM_SELECTED  'pris'
#define PREFS_OK             'prok'
#define PREFS_REVERT         'prre'
#define PREFS_APPLY          'prap'
#define TEXT_HEIGHT 16

class ConfigView : public BView
{
  public:
                            ConfigView( BRect frame, const char * name,
                                        uint32 resizingMode, uint32 flags );

    /* When we create the view, we have to give it an arbitrary size because
       it will be the size of the BScrollView. That's why we keep the real size
       in fRealBounds so we can have a correct BScrollBar later */
    BRect                   fRealBounds;
};

class StringItemWithView : public BStringItem
{
  public:
                            StringItemWithView( const char * text );

    /* Here we store the ConfigView associated to this module */
    ConfigView *            fConfigView;
};

class PreferencesWindow : public BWindow
{
  public:
                            PreferencesWindow( intf_thread_t * p_intf,
                                               BRect frame,
                                               const char * name );
    virtual                 ~PreferencesWindow();

    virtual bool            QuitRequested();
    virtual void            MessageReceived(BMessage* message);
    virtual void            FrameResized( float, float );

            void            Update();
            void            UpdateScrollBar();
            void            ApplyChanges( bool doIt );

            void            ReallyQuit();

  private:
    BView *                 fPrefsView;
    BOutlineListView *      fOutline;
    BView *                 fDummyView;
    BScrollView *           fConfigScroll;

    intf_thread_t *         p_intf;
};

#endif    // BEOS_PREFERENCES_WINDOW_H
