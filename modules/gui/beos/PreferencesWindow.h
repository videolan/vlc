/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.h,v 1.17 2003/05/25 17:21:36 titer Exp $
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

#include <InterfaceKit.h>

#define PREFS_WINDOW_WIDTH   700
#define PREFS_WINDOW_HEIGHT  400
#define PREFS_ITEM_SELECTED  'pris'
#define PREFS_DEFAULTS       'prde'
#define PREFS_APPLY          'prap'
#define PREFS_SAVE           'prsa'

class StringItemWithView : public BStringItem
{
  public:
                            StringItemWithView( const char * text )
                                : BStringItem( text )
                            {
                                fConfigBox = NULL;
                                fConfigScroll = NULL;
                                fConfigView = NULL;
                                fText = strdup( text );
                            }

    /* Here we store the config BBox associated to this module */
    BBox *                  fConfigBox;
    BScrollView *           fConfigScroll;
    BView *                 fConfigView;
    char *                  fText;
};

class ConfigTextControl : public BTextControl
{
  public:
                            ConfigTextControl( BRect rect, char * label,
                                               int type, char * configName )
                                : BTextControl( rect, "ConfigTextControl", label,
                                                "", new BMessage() )
                            {
                                fConfigType = type;
                                fConfigName = strdup( configName );
                            }

    int                     fConfigType;
    char *                  fConfigName;
};

class ConfigCheckBox : public BCheckBox
{
    public:
                            ConfigCheckBox( BRect rect, char * label,
                                            char * configName )
                               : BCheckBox( rect, "ConfigCheckBox", label,
                                            new BMessage() )
                            {
                                fConfigName = strdup( configName );
                            }

    char *                  fConfigName;
};

class ConfigMenuField : public BMenuField
{
    public:
                            ConfigMenuField( BRect rect, char * label,
                                             BPopUpMenu * popUp, char * configName )
                               : BMenuField( rect, "ConfigMenuField", label,
                                             popUp, new BMessage() )
                            {
                                fConfigName = strdup( configName );
                            }

    char *                  fConfigName;
};

class ConfigSlider : public BSlider
{
    public:
                            ConfigSlider( BRect rect, char * label, int type,
                                          int min, int max, char * configName )
                               : BSlider( rect, "ConfigSlider", label,
                                          new BMessage(), min, max )
                            {
                                fConfigType = type;
                                fConfigName = strdup( configName );
                            }
                            
    int                     fConfigType;
    char * fConfigName;
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
            void            SaveChanges();

            void            ReallyQuit();

  private:
    void                    BuildConfigView( StringItemWithView * stringItem,
                                             module_config_t ** pp_item,
                                             bool stop_after_category );

    BView *                 fPrefsView;
    BOutlineListView *      fOutline;
    BView *                 fDummyView;
    BScrollView *           fConfigScroll;
    StringItemWithView *    fCurrent;

    intf_thread_t *         p_intf;
};

#endif    // BEOS_PREFERENCES_WINDOW_H
