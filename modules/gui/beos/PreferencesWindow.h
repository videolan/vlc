/*****************************************************************************
 * PreferencesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
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

#ifndef BEOS_PREFERENCES_WINDOW_H
#define BEOS_PREFERENCES_WINDOW_H

#include <InterfaceKit.h>

#define PREFS_WINDOW_WIDTH   700
#define PREFS_WINDOW_HEIGHT  400
#define PREFS_ITEM_SELECTED  'pris'
#define PREFS_DEFAULTS       'prde'
#define PREFS_APPLY          'prap'
#define PREFS_SAVE           'prsa'

class VTextView : public BTextView
{
    public:
             VTextView( BRect frame, const char *name,
                        uint32 resizingMode, uint32 flags );
        void FrameResized( float width, float height );
};

class VTextControl : public BTextControl
{
    public:
             VTextControl( BRect frame, const char *name,
                           const char *label, const char *text,
                           BMessage * message, uint32 resizingMode );
        void FrameResized( float width, float height );
};

class ConfigWidget : public BView
{
    public:
                        ConfigWidget( intf_thread_t * p_intf, BRect rect,
                                      module_config_t * p_item );
                        ~ConfigWidget();
        bool            InitCheck() { return fInitOK; }
        void            Apply( bool doIt );

    private:
        intf_thread_t * p_intf;

        bool            fInitOK;
        int             fType;
        char          * fName;

        VTextControl  * fTextControl;
        BCheckBox     * fCheckBox;
        BPopUpMenu    * fPopUpMenu;
        BMenuField    * fMenuField;
        BSlider       * fSlider;
        BStringView   * fStringView;
        BCheckBox     * fAltCheck;
        BCheckBox     * fCtrlCheck;
        BCheckBox     * fShiftCheck;
};

class ConfigItem : public BStringItem
{
    public:
                      ConfigItem( intf_thread_t * p_intf,
                                  char * name, bool subModule,
                                  int objectId, int type, char * help );
                      ~ConfigItem();
        int           ObjectId() { return fObjectId; }
        BBox        * Box() { return fBox; }
        void          UpdateScrollBar();
        void          ResetScroll();
        void          Apply( bool doIt );

    private:
        intf_thread_t * p_intf;

        bool            fSubModule;
        int             fObjectId;
        int             fType;
        char          * fHelp;

        BBox          * fBox;
        VTextView     * fTextView;
        BScrollView   * fScroll;
        BView         * fView;
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
            void            Apply( bool doIt );

            void            ReallyQuit();

  private:
    void                    BuildConfigView( ConfigItem * stringItem,
                                             module_config_t ** pp_item,
                                             bool stop_after_category );

    BView                 * fPrefsView;
    BOutlineListView      * fOutline;
    BView                 * fDummyView;
    ConfigItem            * fCurrent;

    intf_thread_t         * p_intf;
};

#endif    // BEOS_PREFERENCES_WINDOW_H
