/*****************************************************************************
 * intf_macosx.c: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "intf_msg.h"
#include "interface.h"

/* FIXME: get rid of this and do menus & command keys*/
#include "keystrokes.h"

#include "modules.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "audio_output.h"

#include "main.h"

#include <Carbon/Carbon.h>

//how often to have callback to main loop.  Target of 30fps then 30hz + maybe some more...
//it doesn't really scale if we move to 2x the hz...  something else is slowing us down...

#define kMainLoopFrequency  (kEventDurationSecond / 45)		//45 for good measure

#define PLAYING		0
#define PAUSED		1

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
    kMenuFile   = 129,
    kMenuControls   = 130,

    kAppleAbout = 1, 
    kAppleQuit = 7, //is this always the same?

    kFileNew   = 1, 
    kFileOpen,
    kFileCloseDivisor,
    kFileClose,

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

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    EventLoopTimerRef manageTimer;
    
} intf_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/* OS Specific */

void CarbonManageCallback ( EventLoopTimerRef inTimer, void *inUserData );

#ifndef CarbonEvents
void EventLoop( intf_thread_t *p_intf );
void DoEvent( intf_thread_t *p_intf , EventRecord *event);
void DoMenuCommand( intf_thread_t *p_intf , long menuResult);
void DrawWindow(WindowRef window);
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

#if 0
    p_intf->p_intf_get_key = intf_GetKey;

    intf_AssignKey( p_intf , 'Q', INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf , 'q', INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf ,  27, INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf ,   3, INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf , '0', INTF_KEY_SET_CHANNEL, 0);
    intf_AssignKey( p_intf , '1', INTF_KEY_SET_CHANNEL, 1);
    intf_AssignKey( p_intf , '2', INTF_KEY_SET_CHANNEL, 2);
    intf_AssignKey( p_intf , '3', INTF_KEY_SET_CHANNEL, 3);
    intf_AssignKey( p_intf , '4', INTF_KEY_SET_CHANNEL, 4);
    intf_AssignKey( p_intf , '5', INTF_KEY_SET_CHANNEL, 5);
    intf_AssignKey( p_intf , '6', INTF_KEY_SET_CHANNEL, 6);
    intf_AssignKey( p_intf , '7', INTF_KEY_SET_CHANNEL, 7);
    intf_AssignKey( p_intf , '8', INTF_KEY_SET_CHANNEL, 8);
    intf_AssignKey( p_intf , '9', INTF_KEY_SET_CHANNEL, 9);
    intf_AssignKey( p_intf , '0', INTF_KEY_SET_CHANNEL, 0);
    intf_AssignKey( p_intf , '+', INTF_KEY_INC_VOLUME, 0);
    intf_AssignKey( p_intf , '-', INTF_KEY_DEC_VOLUME, 0);
    intf_AssignKey( p_intf , 'm', INTF_KEY_TOGGLE_VOLUME, 0);
    intf_AssignKey( p_intf , 'M', INTF_KEY_TOGGLE_VOLUME, 0);
    intf_AssignKey( p_intf , 'g', INTF_KEY_DEC_GAMMA, 0);
    intf_AssignKey( p_intf , 'G', INTF_KEY_INC_GAMMA, 0);
    intf_AssignKey( p_intf , 'c', INTF_KEY_TOGGLE_GRAYSCALE, 0);
    intf_AssignKey( p_intf , ' ', INTF_KEY_TOGGLE_INTERFACE, 0);
    intf_AssignKey( p_intf , 'i', INTF_KEY_TOGGLE_INFO, 0);
    intf_AssignKey( p_intf , 's', INTF_KEY_TOGGLE_SCALING, 0);
    intf_AssignKey( p_intf , 'd', INTF_KEY_DUMP_STREAM, 0);


//EJ - neat menu but don't know if we want it.
// Install the Windows menu. Free of charge!
//    CreateStandardWindowMenu( 0, &windMenu );
//    InsertMenu( windMenu, 0 );
//    DrawMenuBar();

#else

    menu = NewMenu( kMenuApple, "\p\024" );
    AppendMenu( menu, "\pAbout VLCÉ/A" );
    InsertMenu( menu, 0 );

    menu = NewMenu( kMenuFile, "\pFile" );
    AppendMenu( menu, "\pNew Viewer Window/N" );
    AppendMenu( menu, "\pOpenÉ/O" );
    AppendMenu( menu, "\p(-" );
    AppendMenu( menu, "\pClose/W" );
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
#endif

    DrawMenuBar();

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

//    EventTypeSpec windowEventType = { kEventClassWindow, kEventWindowClose };
//    EventHandlerUPP windowHandlerUPP;

    //kinda going out of bounds here... need to bring window creation to this file.
//    main_t *p_main;

/*
    EventTypeSpec keyboardEventType = { kEventClassKeyboard, kEventRawKeyDown };
    EventHandlerUPP keyboardHandlerUPP;
*/

    manageUPP = NewEventLoopTimerUPP ( CarbonManageCallback );
    err = InstallEventLoopTimer ( GetCurrentEventLoop(), 0, kMainLoopFrequency, manageUPP, (void *) p_intf, &p_intf->p_sys->manageTimer );
    assert(err == noErr);
    DisposeEventLoopTimerUPP(manageUPP);

/*    windowHandlerUPP = NewEventHandlerUPP ( MyWindowEventHandler );
    err = InstallWindowEventHandler ( p_main->p_vout->p_sys->p_window , windowHandlerUPP, GetEventTypeCount(windowEventType), &windowEventType, (void *) p_intf, NULL );
    assert(err == noErr);
    DisposeEventHandlerUPP(windowHandlerUPP);
*/


#ifndef CocoaEvents
    //UGLY Event Loop!
    EventLoop( p_intf );
#else
    //Our big event loop !-)
    RunApplicationEventLoop();
#endif
    err = RemoveEventLoopTimer(p_intf->p_sys->manageTimer);
    assert(err == noErr);
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

#ifndef CocoaEvents

void EventLoop( intf_thread_t *p_intf )
{
    Boolean	gotEvent;
    EventRecord	event;
    
    do
    {
        gotEvent = WaitNextEvent(everyEvent,&event,32767,nil);
        if (gotEvent)
            DoEvent( p_intf, &event);
    } while (! p_intf->b_die );
    
    ExitToShell();					
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
                    //DisposeWindow(whichWindow);
                    //ExitToShell();
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

void DoMenuCommand( intf_thread_t *p_intf , long menuResult)
{
    short	menuID;		/* the resource ID of the selected menu */
    short	menuItem;	/* the item number of the selected menu */
	
    static int vol_val;	// remember the current volume
    static int playback_status;		// remember playback state

    menuID = HiWord(menuResult);    /* use macros to get item & menu number */
    menuItem = LoWord(menuResult);

    switch (menuID) 
    {
        case kMenuApple:
            switch (menuItem) 
            {
                case kAppleAbout:
                    //Fixme
                    SysBeep(30);
                    //DoAboutBox();
                    break;
                    
                case kAppleQuit:
                    p_intf->b_die = true;
                    return;
                    break;
				
                default:
                    break;
            }
            break;
        
        case kMenuFile:
            switch (menuItem) 
            {
                case kFileNew:
                    //Fixme
                    SysBeep(30);
                    //DoAboutBox();
                    break;

                case kFileOpen:
                    //Fixme
/*
	    const char **device;
	    char device_method_and_name[B_FILE_NAME_LENGTH + 4];
	    if(p_message->FindString("device", device) != B_ERROR)
	    	{
	    	sprintf(device_method_and_name, "dvd:%s", *device); 
	    	intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, device_method_and_name );
    		}

*/
                    SysBeep(30);
                    //DoAboutBox();
                    break;

                case kFileClose:
                    HideWindow( FrontWindow() );
                    //Fixme
                    SysBeep(30);
                    //DoAboutBox();
                    break;
                    
                default:
                    break;
            }
            break;
		
        case kMenuControls:
            switch (menuItem) 
            {
                case kControlsPlayORPause:
                // pause the playback
                    if (p_intf->p_input != NULL )
                    {
                            // mute the volume if currently playing
                            if (playback_status == PLAYING)
                            {
                                    if (p_main->p_aout != NULL)
                                    {
                                            p_main->p_aout->vol = 0;
                                    }
                                    playback_status = PAUSED;
                            }
                            else
                            // restore the volume
                            {
                                    if (p_main->p_aout != NULL)
                                    {
                                            p_main->p_aout->vol = vol_val;
                                    }
                                    playback_status = PLAYING;
                            }
                            //snooze(400000);
                            input_SetStatus(p_intf->p_input, INPUT_STATUS_PAUSE);
                    }
                    break;

                case kControlsStop:
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
                    //Fixme
                    SysBeep(30);
                    break;

                case kControlsChapterPrevious:
                    //Fixme
                    SysBeep(30);
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
                                            p_main->p_aout->vol = vol_val;
                                    }	
                                    else
                                    {
                                            //p_vol->SetEnabled(false);
                                            vol_val = p_main->p_aout->vol;
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
    Rect		tempRect;
    GrafPtr		curPort;
	
    GetPort(&curPort);
    SetPort(GetWindowPort(window));
    BeginUpdate(window);
    EraseRect(GetWindowPortBounds(window, &tempRect));
    DrawControls(window);
    DrawGrowIcon(window);
    EndUpdate(window);
    SetPort(curPort);
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
