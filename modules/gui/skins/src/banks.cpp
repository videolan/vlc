/*****************************************************************************
 * banks.cpp: Bitmap bank, Event, bank, Font bank and OffSet bank
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: banks.cpp,v 1.5 2003/04/21 22:12:37 asmax Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "bitmap.h"
#include "../os_bitmap.h"
#include "event.h"
#include "../os_event.h"
#include "font.h"
#include "../os_font.h"
#include "banks.h"
#include "skin_common.h"



//---------------------------------------------------------------------------
//  Bitmap Bank
//---------------------------------------------------------------------------
BitmapBank::BitmapBank( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;

    // Create default bitmap
    Add( DEFAULT_BITMAP_NAME, "", 0 );
}
//---------------------------------------------------------------------------
BitmapBank::~BitmapBank()
{
    for( map<string,Bitmap *>::iterator iter = Bank.begin();
         iter != Bank.end(); iter++ )
    {
        delete (OSBitmap *)iter->second;
    }
}
//---------------------------------------------------------------------------
bool BitmapBank::Add( string Id, string FileName, int AColor )
{
    if( Bank[Id] != NULL )
    {
        msg_Warn( p_intf, "Bitmap name already exists: %s", Id.c_str() );
        return false;
    }

    Bank[Id] = (Bitmap *)new OSBitmap( p_intf, FileName, AColor );
    return true;
}
//---------------------------------------------------------------------------
Bitmap * BitmapBank::Get( string Id )
{
    // If the specified bitmap doesn't exist, use the default one
    if( Bank[Id] == NULL )
    {
        msg_Warn( p_intf, "Unknown bitmap name '%s', using default one",
                  Id.c_str() );
        return Bank[DEFAULT_BITMAP_NAME];
    }

    return Bank[Id];
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//  Font Bank
//---------------------------------------------------------------------------
FontBank::FontBank( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;

    // Create default font
    Add( DEFAULT_FONT_NAME, "arial", 12, 0, 400, false, false );
}
//---------------------------------------------------------------------------
FontBank::~FontBank()
{
    for( map<string,SkinFont *>::iterator iter = Bank.begin();
         iter != Bank.end(); iter++ )
    {
        delete (OSFont *)iter->second;
    }
}
//---------------------------------------------------------------------------
bool FontBank::Add( string name, string fontname, int size,
                    int color, int weight, bool italic, bool underline )
{
    if( Bank[name] != NULL )
    {
        msg_Warn( p_intf, "Font name already exists: %s", name.c_str() );
        return false;
    }

    Bank[name] = (SkinFont *)new OSFont( p_intf, fontname, size, color,
                                     weight, italic, underline );
    return true;
}
//---------------------------------------------------------------------------
SkinFont * FontBank::Get( string Id )
{
    // If the specified font doesn't exist, use the default one
    if( Bank[Id] == NULL )
    {
        msg_Warn( p_intf, "Unknown font name '%s', using default one",
                  Id.c_str() );
        return Bank[DEFAULT_FONT_NAME];
    }

    return Bank[Id];
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//  Event Bank
//---------------------------------------------------------------------------
EventBank::EventBank( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;

    // Create default event
    Add( DEFAULT_EVENT_NAME, "VLC_NOTHING",             "none" );

    Add( "none",             "VLC_NOTHING",             "none" );
    Add( "time",             "VLC_STREAMPOS",           "none" );
    Add( "left_time",        "VLC_ENDSTREAMPOS",        "none" );
    Add( "total_time",       "VLC_TOTALSTREAMPOS",      "none" );
    Add( "file_name",        "VLC_STREAMNAME",          "none" );
    Add( "help",             "VLC_HELP_TEXT",           "none" );

    Add( "tray",             "VLC_CHANGE_TRAY",         "CTRL+T" );
    Add( "taskbar",          "VLC_CHANGE_TASKBAR",      "CTRL+B" );

    Add( "playlist_refresh", "CTRL_PLAYLIST",           "none" );
    Add( "play",             "VLC_PLAY",                "X" );
    Add( "pause",            "VLC_PAUSE",               "C" );
    Add( "stop",             "VLC_STOP",                "V" );
    Add( "next",             "VLC_NEXT",                "B" );
    Add( "prev",             "VLC_PREV",                "Z" );
    Add( "fullscreen",       "VLC_FULLSCREEN",          "F" );

    // Volume control
    Add( "mute",             "VLC_VOLUME_CHANGE(MUTE)", "none" );
    Add( "volume_up",        "VLC_VOLUME_CHANGE(UP)",   "none" );
    Add( "volume_down",      "VLC_VOLUME_CHANGE(DOWN)", "none" );
    Add( "volume_refresh",   "VLC_VOLUME_CHANGE(SET)",  "none" );

    // Dialogs events
    Add( "show_log",         "VLC_LOG_SHOW(TRUE)",      "none" );
    Add( "hide_log",         "VLC_LOG_SHOW(FALSE)",     "none" );
    Add( "clear_log",        "VLC_LOG_CLEAR",           "none" );
    Add( "show_prefs",       "VLC_PREFS_SHOW",          "none" );
    Add( "show_info",        "VLC_INFO_SHOW",           "none" );

    Add( "quit",             "VLC_HIDE(VLC_QUIT)",      "CTRL+C" );
    Add( "open",             "VLC_OPEN",                "CTRL+O" );
    Add( "add_file",         "VLC_PLAYLIST_ADD_FILE",   "CTRL+A" );
    Add( "load_skin",        "VLC_LOAD_SKIN",           "CTRL+S" );

}
//---------------------------------------------------------------------------
EventBank::~EventBank()
{
    for( map<string,Event *>::iterator iter = Bank.begin();
         iter != Bank.end(); iter++ )
    {
        iter->second->DestructParameters( true );
        delete (OSEvent *)iter->second;
    }
}
//---------------------------------------------------------------------------
bool EventBank::Add( string Name, string EventDesc, string shortcut )
{
    if( Bank[Name] != NULL )
    {
        msg_Warn( p_intf, "Event name already exists: %s", Name.c_str() );
        return false;
    }

    Bank[Name] = (Event *)new OSEvent( p_intf, EventDesc, shortcut );
    return true;
}
//---------------------------------------------------------------------------
void EventBank::TestShortcut( int key, int mod )
{
    for( map<string,Event *>::iterator iter = Bank.begin();
         iter != Bank.end(); iter++ )
    {
        // If key and modifier match to event shortcut, send event
        if( iter->second->MatchShortcut( key, mod ) )
            iter->second->SendEvent();
    }
}
//---------------------------------------------------------------------------
Event * EventBank::Get( string Id )
{
    // If the specified font doesn't exist, use the default one
    if( Bank[Id] == NULL )
    {
        msg_Warn( p_intf, "Unknown event name '%s', using default one",
                  Id.c_str() );
        return Bank[DEFAULT_EVENT_NAME];
    }

    return Bank[Id];
}
//---------------------------------------------------------------------------
void EventBank::Init()
{
    for( map<string,Event *>::iterator iter = Bank.begin();
         iter != Bank.end(); iter++ )
    {
        iter->second->CreateEvent();
    }
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//  Offset Bank
//---------------------------------------------------------------------------
OffSetBank::OffSetBank( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
    XOff = 0;
    YOff = 0;
}
//---------------------------------------------------------------------------
OffSetBank::~OffSetBank()
{
    if( !XList.empty() )
        msg_Warn( p_intf, "At least one offset remains" );
}
//---------------------------------------------------------------------------
void OffSetBank::PushOffSet( int X, int Y )
{
    XList.push_front( X );
    YList.push_front( Y );
    XOff += X;
    YOff += Y;
}
//---------------------------------------------------------------------------
void OffSetBank::PopOffSet()
{
    if( XList.empty() )
    {
        msg_Warn( p_intf, "No offset to pop" );
        return;
    }

    XOff -= XList.front();
    YOff -= YList.front();
    XList.pop_front();
    YList.pop_front();
}
//---------------------------------------------------------------------------
void OffSetBank::GetOffSet( int &X, int &Y )
{
    X = XOff;
    Y = YOff;
}
//---------------------------------------------------------------------------

