/*****************************************************************************
 * intf_macosx.c: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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

#define MODULE_NAME macosx
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "interface.h"
#include "intf_msg.h"
#include "intf_playlist.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

#include "modules.h"
#include "main.h"

#include "macosx_common.h"

extern main_t *p_main;


/*****************************************************************************
 * Constants & more
 *****************************************************************************/

//how often to have callback to main loop.  Target of 30fps then 30hz + maybe some more...
//it doesn't really scale if we move to 2x the hz...  something else is slowing us down...
#define kMainLoopFrequency  (kEventDurationSecond / 45)		//45 for good measure

// Menu defs
enum
{
/*    kMenuApple  = 128,
    kMenuFile   = 129,
    kMenuControls   = 130,

    kAppleAbout = 1, 
    kAppleQuit = 7, //is this always the same?

    kFileNew   = 1, 
    kFileOpen   = 2, 
    kFileCloseDivisor   = 3,
    kFileClose   = 4,

    kControlsPlayORPause   = 1, 
    kControlsStop   = 2, 
    kControlsForward   = 3, 
    kControlsRewind   = 4, 
    kControlsChapterDiv   = 5, 
    kControlsChapterNext   = 6, 
    kControlsChapterPrevious   = 7, 
    kControlsDVDdiv   = 8, 
    kControlsDVDMenu   = 9, 
    kControlsVolumeDiv   = 10, 
    kControlsVolumeUp   = 11, 
    kControlsVolumeDown   = 12, 
    kControlsVolumeMute   = 13, 
    kControlsEjectDiv   = 14, 
    kControlsEject   = 15 
*/

    kMenuApple  = 128,
    kMenuFile,
    kMenuControls,

    kAppleAbout = 1, 
    kAppleQuit = 8, //is this always the same?

    kFileNew   = 1, 
    kFileOpen,
    kFileCloseDivisor,
    kFileClose,
    kFileQuitHack,

    kControlsPlayORPause   = 1, 
    kControlsStop,
    kControlsForward,
    kControlsRewind,
    kControlsChapterDiv,
    kControlsChapterNext,
    kControlsChapterPrevious,
    kControlsDVDdiv,
    kControlsDVDMenu,
    kControlsVolumeDiv,
    kControlsVolumeUp,
    kControlsVolumeDown,
    kControlsVolumeMute,
    kControlsEjectDiv,
    kControlsEject 

#if 0
//virtual key codes ; raw subtract 0x40 from these values
//http://devworld.apple.com/techpubs/mac/Text/Text-577.html#HEADING577-0
    kLeftArrow = 0x7B,
    kRightArrow = 0x7C,
    kDownArrow = 0x7D,
    kUpArrow = 0x7E,

//http://devworld.apple.com/techpubs/mac/Text/Text-571.html#MARKER-9-18    
    kPeriod = 47, //(decimal)
    kSpace = 49, //(decimal)
    kEscape = 53 //(decimal)
#endif

};

// Initial Window Constants
enum
{
    kAboutWindowOffset = 200,
    kAboutWindowWidth = 200, //400
    kAboutWindowHeight = 50 //100
};


/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    EventLoopTimerRef manageTimer;
    Rect aboutRect;
    WindowRef	p_aboutWindow;
} intf_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/* OS Specific */

static int MakeAboutWindow		( intf_thread_t *p_intf );

void CarbonManageCallback ( EventLoopTimerRef inTimer, void *inUserData );

OSErr MyOpenDocument(const FSSpecPtr defaultLocationfssPtr);

void playorpause ( intf_thread_t *p_intf );
void stop ( intf_thread_t *p_intf );


#ifndef CarbonEvents
void EventLoop( intf_thread_t *p_intf );
void DoEvent( intf_thread_t *p_intf , EventRecord *event);
void DoMenuCommand( intf_thread_t *p_intf , long menuResult);
void DrawWindow(WindowRef window);
void DrawAboutWindow(WindowRef window);
#else
/*
pascal OSErr 	QuitEventHandler(const AppleEvent *theEvent, AppleEvent *theReply, SInt32 refCon);
static pascal OSStatus MyKeyHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* userData );
static pascal OSStatus MyWindowEventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData);
*/
#endif

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function checks the interface can be run and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "macosx" ) )
    {
        return( 999 );
    }

    /* Under MacOS X, this plugin always works */
    return( 100 );
}

/*****************************************************************************
 * intf_Open: initialize interface
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    MenuHandle menu;
//    MenuRef windMenu;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    /* Init Mac stuff */
    InitCursor();
    SetQDGlobalsRandomSeed( TickCount() );

// neat menu but don't know if we want it.
// Install the Windows menu. Free of charge!
//    CreateStandardWindowMenu( 0, &windMenu );
//    InsertMenu( windMenu, 0 );
//    DrawMenuBar();

    menu = NewMenu( kMenuApple, "\p\024" );
    AppendMenu( menu, "\pAbout VLCÉ/A" );
    InsertMenu( menu, 0 );

    menu = NewMenu( kMenuFile, "\pFile" );
    AppendMenu( menu, "\pNew Viewer Window/N" );
    AppendMenu( menu, "\pOpenÉ/O" );
    AppendMenu( menu, "\p(-" );
    AppendMenu( menu, "\pClose/W" );
    //standard OS X application menu quit isn't working nicely
    AppendMenu( menu, "\pQuit/Q" );
    InsertMenu( menu, 0 );

//BIG HONKING MENU - in order Mac OS 9 dvd player
//can't get key codes right for menus... argh that's why they use resources!

    menu = NewMenu( kMenuControls, "\pControls" );

    AppendMenu( menu, "\pPlay/," );
//    SetMenuItemCommandKey(menu, 0, false, kSpace);
//    SetMenuItemModifiers( menu, 0, kMenuNoCommandModifier);

    AppendMenu( menu, "\pStop/." );

    AppendMenu( menu, "\pFast Forward/f" );
//    SetMenuItemCommandKey(menu, 2, false, kRightArrow);

    AppendMenu( menu, "\pRewind/r" );
//    SetMenuItemCommandKey(menu, 3, false, kLeftArrow);

    AppendMenu( menu, "\p(-" ); //4

    AppendMenu( menu, "\pNext Chapter/c" );
//    SetMenuItemCommandKey(menu, 5, false, kRightArrow);
//    SetMenuItemModifiers( menu, 5, kMenuNoCommandModifier);

    AppendMenu( menu, "\pPrevious Chapter/p" );
//    SetMenuItemCommandKey(menu, 6, false, kLeftArrow);
//    SetMenuItemModifiers( menu, 6, kMenuNoCommandModifier);

    AppendMenu( menu, "\p(-" ); //7

    AppendMenu( menu, "\pDVD Menu/v" );
//    SetMenuItemCommandKey(menu, 8, false, kEscape);
//    SetMenuItemModifiers( menu, 8, kMenuNoCommandModifier);

    AppendMenu( menu, "\p(-" ); //9

    AppendMenu( menu, "\pVolume Up/u" );
//    SetMenuItemCommandKey(menu, 10, false, kUpArrow);

    AppendMenu( menu, "\pVolume Down/d" );
//    SetMenuItemCommandKey(menu, 11, false, kDownArrow);

    AppendMenu( menu, "\pMute/M" ); //12

    AppendMenu( menu, "\p(-" ); //13

    AppendMenu( menu, "\pEject/E" ); //14

    InsertMenu( menu, 0 );

//Hmm, eventually we might want more than one player window, but for now we assume one only (like OS 9 player)
//and since we start with a window open, we temporarily disable the 'new' menu
    DisableMenuItem( GetMenuHandle(kMenuFile), kFileNew);

//FIXME - Disabled Menus which are not implemented yet
    DisableMenuItem( GetMenuHandle(kMenuControls), kControlsDVDMenu);
    DisableMenuItem( GetMenuHandle(kMenuControls), kControlsEject);

    DrawMenuBar();

    if( MakeAboutWindow( p_intf ) )
    {
        intf_ErrMsg( "vout error: can't make about window" );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: main loop
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    OSStatus err;
    EventLoopTimerUPP manageUPP;

/*
Eventually we want to use Carbon events, or maybe even write this app in Cocoa
 
//kinda going out of bounds here... need to bring window creation to this file.
    main_t *p_main;

    EventTypeSpec windowEventType = { kEventClassWindow, kEventWindowClose };
    EventHandlerUPP windowHandlerUPP;

    EventTypeSpec keyboardEventType = { kEventClassKeyboard, kEventRawKeyDown };
    EventHandlerUPP keyboardHandlerUPP;
*/

    manageUPP = NewEventLoopTimerUPP ( CarbonManageCallback );
    err = InstallEventLoopTimer ( GetCurrentEventLoop(), 0, kMainLoopFrequency, manageUPP, (void *) p_intf, &p_intf->p_sys->manageTimer );
    assert(err == noErr);
    DisposeEventLoopTimerUPP(manageUPP);

/*
    windowHandlerUPP = NewEventHandlerUPP ( MyWindowEventHandler );
    err = InstallWindowEventHandler ( p_main->p_vout->p_sys->p_window , windowHandlerUPP, 	GetEventTypeCount(windowEventType), &windowEventType, (void *) p_intf, NULL );
    assert(err == noErr);
    DisposeEventHandlerUPP(windowHandlerUPP);
*/


#ifndef CarbonEvents
    //UGLY Event Loop!
    EventLoop( p_intf );
#else
    RunApplicationEventLoop();
#endif
    err = RemoveEventLoopTimer(p_intf->p_sys->manageTimer);
    assert(err == noErr);
}


/*****************************************************************************
 * MakeAboutWindow: similar to MakeWindow in vout_macosx.c ; 
 * open and set-up a Mac OS window to be used for 'about' program... 
 * create it hidden and only show it when requested
 *****************************************************************************/
static int MakeAboutWindow( intf_thread_t *p_intf )
{
    int left = 0;
    int top = 0;
    int bottom = kAboutWindowHeight;
    int right = kAboutWindowWidth;

    WindowAttributes windowAttr = kWindowCloseBoxAttribute |
                                    kWindowStandardHandlerAttribute |
                                    kWindowInWindowMenuAttribute;
    
    SetRect( &p_intf->p_sys->aboutRect, left, top, right, bottom );
    OffsetRect( &p_intf->p_sys->aboutRect, kAboutWindowOffset, kAboutWindowOffset );

    CreateNewWindow( kDocumentWindowClass, windowAttr, &p_intf->p_sys->aboutRect, &p_intf->p_sys->p_aboutWindow );
    if ( p_intf->p_sys->p_aboutWindow == nil )
    {
        return( 1 );
    }

    InstallStandardEventHandler(GetWindowEventTarget(p_intf->p_sys->p_aboutWindow));
    SetWindowTitleWithCFString( p_intf->p_sys->p_aboutWindow, CFSTR("About DVD.app & VLC") );
    
    return( 0 );
}


void CarbonManageCallback ( EventLoopTimerRef inTimer, void *inUserData )
{
    intf_thread_t * p_intf = (intf_thread_t *) inUserData;

    /* Manage core vlc functions through the callback */
    p_intf->pf_manage( p_intf );
    
    if ( p_intf->b_die )
    {
	QuitApplicationEventLoop();
    }
}

#ifndef CarbonEvents

void EventLoop( intf_thread_t *p_intf )
{
    Boolean	gotEvent;
    EventRecord	event;
    
    do
    {
    p_intf->pf_manage( p_intf );
        gotEvent = WaitNextEvent(everyEvent,&event,32767,nil);
        if (gotEvent)
            DoEvent( p_intf, &event);
    } while (! p_intf->b_die );
    
    //ExitToShell();					
}


void DoEvent( intf_thread_t *p_intf , EventRecord *event)
{
    short	part;
    Boolean	hit;
    char	key;
    Rect	tempRect;
    WindowRef	whichWindow;
        
    switch (event->what) 
    {
        case mouseDown:
            part = FindWindow(event->where, &whichWindow);
            switch (part)
            {
                case inMenuBar:  /* process a moused menu command */
                    DoMenuCommand( p_intf, MenuSelect(event->where));
                    break;
                    
                case inSysWindow:
                    break;
                
                case inContent:
                    if (whichWindow != FrontWindow()) 
                        SelectWindow(whichWindow);
                    break;
                
                case inDrag:	/* pass screenBits.bounds */
                    GetRegionBounds(GetGrayRgn(), &tempRect);
                    DragWindow(whichWindow, event->where, &tempRect);
                    break;
                    
                case inGrow:
                    break;
                    
                case inGoAway:
                    p_intf->b_die = true;
                    return;
                    break;
                    
                case inZoomIn:
                case inZoomOut:
                    hit = TrackBox(whichWindow, event->where, part);
                    if (hit) 
                    {
                        SetPort(GetWindowPort(whichWindow));   // window must be current port
                        EraseRect(GetWindowPortBounds(whichWindow, &tempRect));   // inval/erase because of ZoomWindow bug
                        ZoomWindow(whichWindow, part, true);
                        InvalWindowRect(whichWindow, GetWindowPortBounds(whichWindow, &tempRect));	
                    }
                    break;
                }
                break;
		
                case keyDown:
		case autoKey:
                    key = event->message & charCodeMask;
                    if (event->modifiers & cmdKey)
                        if (event->what == keyDown)
                            DoMenuCommand( p_intf, MenuKey(key));
                            
		case activateEvt:	       /* if you needed to do something special */
                    break;
                    
                case updateEvt:
                        DrawWindow((WindowRef) event->message);
                    break;
                        
                case kHighLevelEvent:
			AEProcessAppleEvent( event );
                    break;
		
                case diskEvt:
                    break;
	}
}

//the code for playorpause and stop taken almost directly from the BeOS code
void playorpause ( intf_thread_t *p_intf )
{
// pause the playback
    if (p_intf->p_input != NULL )
    {
            // mute the volume if currently playing
            if (p_main->p_vout->p_sys->playback_status == PLAYING)
            {
                    if (p_main->p_aout != NULL)
                    {
                            p_main->p_aout->vol = 0;
                    }
                    p_main->p_vout->p_sys->playback_status = PAUSED;
                SetMenuItemText( GetMenuHandle(kMenuControls), kControlsPlayORPause, "\pPlay");
            }
            else
            // restore the volume
            {
                    if (p_main->p_aout != NULL)
                    {
                            p_main->p_aout->vol = p_main->p_vout->p_sys->vol_val;
                    }
                    p_main->p_vout->p_sys->playback_status = PLAYING;
                SetMenuItemText( GetMenuHandle(kMenuControls), kControlsPlayORPause, "\pPause");
            }
            //snooze(400000);
            input_SetStatus(p_intf->p_input, INPUT_STATUS_PAUSE);
    }
}

void stop ( intf_thread_t *p_intf )
{
    // this currently stops playback not nicely
    if (p_intf->p_input != NULL )
    {
            // silence the sound, otherwise very horrible
            if (p_main->p_aout != NULL)
            {
                    p_main->p_aout->vol = 0;
            }
            //snooze(400000);
            input_SetStatus(p_intf->p_input, INPUT_STATUS_END);
    }
    p_main->p_vout->p_sys->playback_status = STOPPED;
}


void DoMenuCommand( intf_thread_t *p_intf , long menuResult)
{
    short	menuID;		/* the resource ID of the selected menu */
    short	menuItem;	/* the item number of the selected menu */
	

    menuID = HiWord(menuResult);    /* use macros to get item & menu number */
    menuItem = LoWord(menuResult);

    switch (menuID) 
    {
        case kMenuApple:
            switch (menuItem) 
            {
                case kAppleAbout:
                    ShowWindow( p_intf->p_sys->p_aboutWindow );
                    SelectWindow( p_intf->p_sys->p_aboutWindow );
                    DrawAboutWindow( p_intf->p_sys->p_aboutWindow); //kludge
                    EnableMenuItem( GetMenuHandle(kMenuFile), kFileClose);
                    break;
                    
                case kAppleQuit:
                    p_intf->b_die = true;
                    //hrmm... don't know what is going on w/ the Quit item in the new application menu...documentation???
                    break;
				
                default:
                    break;
            }
            break;
        
        case kMenuFile:
            switch (menuItem) 
            {
                case kFileNew:
                    ShowWindow( p_main->p_vout->p_sys->p_window );
                    SelectWindow( p_main->p_vout->p_sys->p_window );
                    DisableMenuItem( GetMenuHandle(kMenuFile), kFileNew);
                    EnableMenuItem( GetMenuHandle(kMenuFile), kFileClose);
                    //hmm, can't say to play() right now because I don't know if a file is in playlist yet.
                    //need to see if I can tell this or eve if calling play() w/o a file is bad...not sure of either
                    break;

                case kFileOpen:
                    playorpause( p_intf );
                    MyOpenDocument(nil);
                    // starts playing automatically on open? playorpause( p_intf );
                    break;

                case kFileClose:
                    HideWindow( FrontWindow() );
                    if ( ! IsWindowVisible( p_main->p_vout->p_sys->p_window ) && ! IsWindowVisible( p_intf->p_sys->p_aboutWindow ) )
                    {
                        //calling this even if no file open shouldn't be bad... not sure of opposite situation above
                        stop( p_intf );
                        EnableMenuItem( GetMenuHandle(kMenuFile), kFileNew);
                        DisableMenuItem( GetMenuHandle(kMenuFile), kFileClose);
                    }
                    break;
                    
                case kFileQuitHack:
                        stop( p_intf );
                        p_intf->b_die = true;
                    break;
                    
                default:
                    break;
            }
            break;
		
        case kMenuControls:
            switch (menuItem) 
            {
                case kControlsPlayORPause:
                        playorpause( p_intf );
                    break;

                case kControlsStop:
                        stop( p_intf );
                    break;

                case kControlsForward:
		// cycle the fast playback modes
                    if (p_intf->p_input != NULL )
                    {
                            if (p_main->p_aout != NULL)
                            {
                                    p_main->p_aout->vol = 0;
                            }
                            //snooze(400000);
                            input_SetStatus(p_intf->p_input, INPUT_STATUS_FASTER);
                    }
                    break;

                case kControlsRewind:
		// cycle the slow playback modes
                    if (p_intf->p_input != NULL )
                    {
                            if (p_main->p_aout != NULL)
                            {
                                    p_main->p_aout->vol = 0;
                            }
                            //snooze(400000);
                            input_SetStatus(p_intf->p_input, INPUT_STATUS_SLOWER);
                    }
                    break;
                
                case kControlsChapterNext:
                    if( p_intf->p_input != NULL )
                    {
                        /* FIXME: temporary hack */
                        p_intf->p_input->b_eof = 1;
                    }
                    break;

                case kControlsChapterPrevious:
                    if( p_intf->p_input != NULL )
                    {
                        /* FIXME: temporary hack */
                        intf_PlaylistPrev( p_main->p_playlist );
                        intf_PlaylistPrev( p_main->p_playlist );
                        p_intf->p_input->b_eof = 1;
                    }
                    break;

                case kControlsDVDMenu:
                    //Fixme
                    SysBeep(30);
                    break;

                case kControlsVolumeUp:
		// adjust the volume
                    if (p_main->p_aout != NULL) 
                    {
                        p_main->p_aout->vol++;
                    }
                    break;

                case kControlsVolumeDown:
		// adjust the volume
                    if (p_main->p_aout != NULL) 
                    {
                        p_main->p_aout->vol--;
                    }
                    break;

                case kControlsVolumeMute:
                // mute
                    if (p_main->p_aout != NULL) 
                        {
                                    if (p_main->p_aout->vol == 0)
                                    {
                                            //p_vol->SetEnabled(true);
                                            p_main->p_aout->vol = p_main->p_vout->p_sys->vol_val;
                                    }	
                                    else
                                    {
                                            //p_vol->SetEnabled(false);
                                            p_main->p_vout->p_sys->vol_val = p_main->p_aout->vol;
                                            p_main->p_aout->vol = 0;
                                    }
                            }
                            break;

                case kControlsEject:
                    //Fixme
                    SysBeep(30);
                    break;
                    
                default:
                    break;
            }
            break;

        default:
            break;
    }
    HiliteMenu(0);	/* unhighlight what MenuSelect (or MenuKey) hilited */
}

void DrawWindow(WindowRef window)
{
    Rect tempRect;
    GrafPtr previousPort;
    
    GetPort(&previousPort);
    SetPort(GetWindowPort(window));
    BeginUpdate(window);
    EraseRect(GetWindowPortBounds(window, &tempRect));
    DrawControls(window);
    DrawGrowIcon(window);
    EndUpdate(window);
    SetPort(previousPort);
}

void DrawAboutWindow(WindowRef window)
{
    GrafPtr previousPort;

    GetPort(&previousPort);
    SetPort(GetWindowPort(window));
    
    MoveTo(10,30);
    DrawString("\phttp://www.videolan.org");

    SetPort(previousPort);
}

#else

static pascal OSStatus MyEventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
    WindowRef                     window;
    Rect                          bounds;
    UInt32                        whatHappened;
    HICommand                     commandStruct;
    MenuRef                       theMenuRef;
    UInt16                        theMenuItem;
    OSStatus                      result = eventNotHandledErr; // report failure by default
    
    GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(window), NULL, &window);
    
    whatHappened = GetEventKind(event);
    
    switch (whatHappened)
    {
        case kEventWindowActivated:
            break;
        
        case kEventWindowDeactivated:
            break;
        
        case kEventWindowDrawContent:
            //DoUpdate(window);
            result = noErr;
            break;

        case kEventWindowBoundsChanged:
            InvalWindowRect(window, GetWindowPortBounds(window, &bounds));
            //DoUpdate(window);
            result = noErr;
            break;

        case kEventWindowClickContentRgn:
            /*DoContentClick(window);
            DoUpdate(window);
            AdjustMenus();*/
            result = noErr;
            break;

        case kEventCommandProcess:
            GetEventParameter (event, kEventParamDirectObject, 
                                        typeHICommand, NULL, sizeof(HICommand), 
                                        NULL, &commandStruct);
            theMenuRef = commandStruct.menu.menuRef;

            if (theMenuRef == GetMenuHandle(kMenuApple)) 
                {
                    // Because the event didn't occur *in* the window, the 
                    // window reference isn't valid until we set it here 
                    window = FrontWindow(); 

                    theMenuItem = commandStruct.menu.menuItemIndex;
                    switch ( theMenuItem ) 
                            {
                                case iStop:
                                        SetLight(window, true);
                                        break;
                                case iGo:
                                        SetLight(window, false);
                                        break;
                            }
                    DoUpdate(window);
                    AdjustMenus();
                    result = noErr;
                }
            */
            break; 

        case kEventMouseMoved:
            /*
            CursorRgn = NewRgn();
            GetEventParameter (event, kEventParamMouseLocation, typeQDPoint,
                                        NULL, sizeof(Point), NULL, &wheresMyMouse);
            AdjustCursor(wheresMyMouse, CursorRgn);
            DisposeRgn(CursorRgn);
            */
            result = noErr;
            break;

        default: 
            // If nobody handled the event, it gets propagated to the
            // application-level handler.
            break;
    }
    return result;
}
#endif

//FIXME Adding this has introduced or surfaced a lot of bugs...
//comented out a lot of things to strip this down to make this a 'quicky'
OSErr MyOpenDocument(const FSSpecPtr defaultLocationfssPtr)
{
    NavDialogOptions    dialogOptions;
//    AEDesc              defaultLocation;
//    NavEventUPP         eventProc = NewNavEventProc(myEventProc);
//    NavObjectFilterUPP  filterProc = 
//                        NewNavObjectFilterProc(myFilterProc);
    OSErr               anErr = noErr;
    
    //  Specify default options for dialog box
    anErr = NavGetDefaultDialogOptions(&dialogOptions);
    if (anErr == noErr)
    {
        //  Adjust the options to fit our needs
        //  Set default location option
//        dialogOptions.dialogOptionFlags |= kNavSelectDefaultLocation;
        //  Clear preview option
        dialogOptions.dialogOptionFlags ^= kNavAllowPreviews;
        
        // make descriptor for default location
//        anErr = AECreateDesc(typeFSS, defaultLocationfssPtr,
//                             sizeof(*defaultLocationfssPtr),
//                             &defaultLocation );
        if (anErr == noErr)
        {
            // Get 'open' resource. A nil handle being returned is OK,
            // this simply means no automatic file filtering.
            NavTypeListHandle typeList = (NavTypeListHandle)GetResource(
                                        'open', 128);
            NavReplyRecord reply;
            
            // Call NavGetFile() with specified options and
            // declare our app-defined functions and type list
//            anErr = NavGetFile (&defaultLocation, &reply, &dialogOptions,
            anErr = NavGetFile (nil, &reply, &dialogOptions,
//                                eventProc, nil, filterProc,
                                nil, nil, nil,
                                typeList, nil);
            if (anErr == noErr && reply.validRecord)
            {
                //  Deal with multiple file selection
                long    count;
                
                anErr = AECountItems(&(reply.selection), &count);
                // Set up index for file list
                if (anErr == noErr)
                {
                    long index;
                    
                    for (index = 1; index <= count; index++)
                    {
                        AEKeyword   theKeyword;
                        DescType    actualType;
                        Size        actualSize;
                        FSSpec      documentFSSpec;
                        
                        // Get a pointer to selected file
                        anErr = AEGetNthPtr(&(reply.selection), index,
                                            typeFSS, &theKeyword,
                                            &actualType,&documentFSSpec,
                                            sizeof(documentFSSpec),
                                            &actualSize);
                        if (anErr == noErr)
                        {
//                            anErr = DoOpenFile(&documentFSSpec);
//HERE
                            FSRef newRef;
                            char path[MAXPATHLEN];
                            
                            //make an FSRef out of an FSSpec
                            anErr = FSpMakeFSRef( &documentFSSpec, &newRef);
                            if (anErr != noErr)
                            {
                                return(anErr);
                            }
                            //make a path out of the FSRef
                            anErr = FSRefMakePath( &newRef, path, MAXPATHLEN);
                            if (anErr != noErr)
                            {
                                return(anErr);
                            }
                            
                            //else, ok...add it to playlist!
                            intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, path );

                            
                        }
                    }
                }
                //  Dispose of NavReplyRecord, resources, descriptors
                anErr = NavDisposeReply(&reply);
            }
            if (typeList != NULL)
            {
                ReleaseResource( (Handle)typeList);
            }
            //(void) AEDisposeDesc(&defaultLocation);
        }
    }
//    DisposeRoutineDescriptor(eventProc);
//    DisposeRoutineDescriptor(filterProc);
    return anErr;
}

