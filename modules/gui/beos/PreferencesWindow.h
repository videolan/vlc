/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.h,v 1.18 2003/12/21 21:30:43 titer Exp $
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

class ConfigWidget : public BView
{
    public:
        ConfigWidget( BRect rect, int type, char * configName );
        virtual void Apply( intf_thread_t * p_intf, bool doIt ) = 0;

    protected:
        int          fConfigType;
        char       * fConfigName;
};

class ConfigTextControl : public ConfigWidget
{
    public:
        ConfigTextControl( BRect rect, int type, char * label,
                           char * configName );
        void Apply( intf_thread_t * p_intf, bool doIt );

    private:
        BTextControl * fTextControl;
};

class ConfigCheckBox : public ConfigWidget
{
    public:
        ConfigCheckBox( BRect rect, int type, char * label,
                        char * configName );
        void Apply( intf_thread_t * p_intf, bool doIt );

    private:
        BCheckBox * fCheckBox;
};

class ConfigMenuField : public ConfigWidget
{
    public:
        ConfigMenuField( BRect rect, int type, char * label,
                         char * configName, char ** list );
        void Apply( intf_thread_t * p_intf, bool doIt );

    private:
        BPopUpMenu * fPopUpMenu;
        BMenuField * fMenuField;
};

class ConfigSlider : public ConfigWidget
{
    public:
        ConfigSlider( BRect rect, int type, char * label,
                      char * configName, int min, int max );
        void Apply( intf_thread_t * p_intf, bool doIt );

    private:
        BSlider * fSlider;
};

class ConfigKey : public ConfigWidget
{
    public:
        ConfigKey( BRect rect, int type, char * label,
                   char * configName );
        void Apply( intf_thread_t * p_intf, bool doIt );

    private:
        BStringView * fStringView;
        BCheckBox   * fAltCheck;
        BCheckBox   * fCtrlCheck;
        BCheckBox   * fShiftCheck;
        BPopUpMenu  * fPopUpMenu;
        BMenuField  * fMenuField;
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
