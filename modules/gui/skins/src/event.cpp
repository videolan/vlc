/*****************************************************************************
 * event.cpp: Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: event.cpp,v 1.3 2003/04/01 12:24:54 gbazin Exp $
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
#include "os_api.h"
#include "skin_common.h"
#include "banks.h"
#include "generic.h"
#include "window.h"
#include "theme.h"
#include "event.h"
#include "os_event.h"



//---------------------------------------------------------------------------
//   VLC Event
//---------------------------------------------------------------------------
Event::Event( intf_thread_t *_p_intf, string Desc, string shortcut )
{
    p_intf = _p_intf;
    EventDesc = Desc;
    Message   = VLC_NOTHING;
    Param1    = 0;
    Param2    = 0;
    Shortcut  = shortcut;
}
//---------------------------------------------------------------------------
Event::Event( intf_thread_t *_p_intf, unsigned int msg, unsigned int par1,
              long par2 )
{
    p_intf = _p_intf;
    Message  = msg;
    Param1   = par1;
    Param2   = par2;
    Shortcut = "none";
}
//---------------------------------------------------------------------------
Event::~Event()
{
}
//---------------------------------------------------------------------------
void Event::DestructParameters()
{
    switch( Message )
    {
        case CTRL_SYNCHRO:
            if( Param2 == (int)true )
                delete (Event *)Param1;
            break;

        case CTRL_SET_TEXT:
            delete (char *)Param2;
            break;
    }

}
//---------------------------------------------------------------------------
bool Event::IsEqual( Event *evt )
{
    return( evt->GetMessage() == Message && evt->GetParam1() == Param1 &&
            evt->GetParam2()  == Param2 );
}
//---------------------------------------------------------------------------
void Event::PostSynchroMessage( bool autodelete )
{
    OSAPI_PostMessage( NULL, CTRL_SYNCHRO, (unsigned int)this,
                      (long)autodelete );
}
//---------------------------------------------------------------------------
void Event::PostTextMessage( string text )
{
    char *txt = new char[text.size()];
    strcpy( txt, text.c_str() );
    OSAPI_PostMessage( NULL, CTRL_SET_TEXT, (unsigned int)this, (long)txt );
}
//---------------------------------------------------------------------------
unsigned int Event::GetMessageType( string Desc )
{
    if( Desc == "VLC_NOTHING" )
        return VLC_NOTHING;

    // VLC messages
    else if( Desc == "VLC_QUIT" )
        return VLC_QUIT;
    else if( Desc == "VLC_HIDE" )
        return VLC_HIDE;
    else if( Desc == "VLC_OPEN" )
        return VLC_OPEN;
    else if( Desc == "VLC_LOAD_SKIN" )
        return VLC_LOAD_SKIN;
    else if( Desc == "VLC_CHANGE_TRAY" )
        return VLC_CHANGE_TRAY;
    else if( Desc == "VLC_CHANGE_TASKBAR" )
        return VLC_CHANGE_TASKBAR;

    // Stream control
    else if( Desc == "VLC_PLAY" )
        return VLC_PLAY;
    else if( Desc == "VLC_STOP" )
        return VLC_STOP;
    else if( Desc == "VLC_PAUSE" )
        return VLC_PAUSE;
    else if( Desc == "VLC_NEXT" )
        return VLC_NEXT;
    else if( Desc == "VLC_PREV" )
        return VLC_PREV;
    else if( Desc == "VLC_STREAMPOS" )
        return VLC_STREAMPOS;
    else if( Desc == "VLC_ENDSTREAMPOS" )
        return VLC_ENDSTREAMPOS;
    else if( Desc == "VLC_TOTALSTREAMPOS" )
        return VLC_TOTALSTREAMPOS;
    else if( Desc == "VLC_STREAMNAME" )
        return VLC_STREAMNAME;
    else if( Desc == "VLC_HELP_TEXT" )
        return VLC_HELP_TEXT;

    // Volume control
    else if( Desc == "VLC_VOLUME_CHANGE" )
        return VLC_VOLUME_CHANGE;
    else if( Desc == "VLC_VOLUME_MUTE" )
        return VLC_VOLUME_MUTE;
    else if( Desc == "VLC_VOLUME_UP" )
        return VLC_VOLUME_UP;
    else if( Desc == "VLC_VOLUME_DOWN" )
        return VLC_VOLUME_DOWN;
    else if( Desc == "VLC_VOLUME_SET" )
        return VLC_VOLUME_SET;

    // Logs
    else if( Desc == "VLC_LOG_SHOW" )
        return VLC_LOG_SHOW;
    else if( Desc == "VLC_LOG_CLEAR" )
        return VLC_LOG_CLEAR;

    // Playlist events
    else if( Desc == "VLC_PLAYLIST_ADD_FILE" )
        return VLC_PLAYLIST_ADD_FILE;

    // Video output events
    else if( Desc == "VLC_FULLSCREEN" )
        return VLC_FULLSCREEN;

    // Window event
    else if( Desc == "WINDOW_MOVE" )
        return WINDOW_MOVE;
    else if( Desc == "WINDOW_OPEN" )
        return WINDOW_OPEN;
    else if( Desc == "WINDOW_CLOSE" )
        return WINDOW_CLOSE;
    else if( Desc == "WINDOW_SHOW" )
        return WINDOW_SHOW;
    else if( Desc == "WINDOW_HIDE" )
        return WINDOW_HIDE;
    else if( Desc == "WINDOW_FADE" )
        return WINDOW_FADE;

    // Control event
    else if( Desc == "CTRL_ENABLED" )
        return CTRL_ENABLED;
    else if( Desc == "CTRL_VISIBLE" )
        return CTRL_VISIBLE;
    else if( Desc == "CTRL_SYNCHRO" )
        return CTRL_SYNCHRO;
    else if( Desc == "CTRL_SET_TEXT" )
        return CTRL_SET_TEXT;
    else if( Desc == "CTRL_SET_SLIDER" )
        return CTRL_SET_SLIDER;


    // Control event by ID
    else if( Desc == "CTRL_ID_VISIBLE" )
        return CTRL_ID_VISIBLE;
    else if( Desc == "CTRL_ID_ENABLED" )
        return CTRL_ID_ENABLED;
    else if( Desc == "CTRL_ID_MOVE" )
        return CTRL_ID_MOVE;

    // Control definition
    else if( Desc == "CTRL_SLIDER" )
        return CTRL_SLIDER;
    else if( Desc == "CTRL_TIME" )
        return CTRL_TIME;
    else if( Desc == "CTRL_PLAYLIST" )
        return CTRL_PLAYLIST;

    // Playlist
    else if( Desc == "PLAYLIST_ID_DEL" )
        return PLAYLIST_ID_DEL;

    // Not found
    else
    {
        msg_Warn( p_intf, "Theme: Unknown event (%s)", EventDesc.c_str() );
        return VLC_NOTHING;
    }
}
//---------------------------------------------------------------------------
void Event::CreateEvent()
{
    // Initiatization
    int x, y;
    char *msg   = new char[MAX_EVENT_SIZE];
    char *para1 = new char[MAX_PARAM_SIZE];
    char *para2 = new char[MAX_PARAM_SIZE];
    char *para3 = new char[MAX_PARAM_SIZE];

    // Scan the event
    int scan = sscanf( EventDesc.c_str(),
        "%[^(](%[^,)],%[^,)],%[^,)])", msg, para1, para2, para3 );

    // Check parameters
    if( scan < 1 )
        strcpy( msg, "VLC_NOTHING" );
    if( scan < 2 )
        strcpy( para1, "" );
    if( scan < 3 )
        strcpy( para2, "" );
    if( scan < 4 )
        strcpy( para3, "" );

    // Find Message type
    Message = GetMessageType( msg );

    // Find Parameters
    switch( Message )
    {
        case VLC_HIDE:
            Param1 = GetMessageType( para1 );
            break;

        case VLC_VOLUME_CHANGE:
            if( strcmp( para1, "MUTE" ) == 0 )
                Param1 = VLC_VOLUME_MUTE;
            else if( strcmp( para1, "UP" ) == 0 )
                Param1 = VLC_VOLUME_UP;
            else if( strcmp( para1, "DOWN" ) == 0 )
                Param1 = VLC_VOLUME_DOWN;
            else if( strcmp( para1, "SET" ) == 0 )
            {
                Param1 = VLC_VOLUME_SET;
                Param2 = atoi( para2 ) * AOUT_VOLUME_MAX / 100;
            }
            break;

        case VLC_LOG_SHOW:
            Param2 = GetBool( para1 );
            break;

        case CTRL_ID_VISIBLE:
            Param1 = (unsigned int)FindControl( para1 );
            Param2 = GetBool( para2 );
            break;

        case CTRL_ID_ENABLED:
            Param1 = (unsigned int)FindControl( para1 );
            Param2 = GetBool( para2 );
            break;

        case CTRL_ID_MOVE:
            Param1 = (unsigned int)FindControl( para1 );
            x = atoi( para2 );
            y = atoi( para3 );
            if( x < 0 )
                x = -x + 0x8000;
            if( y < 0 )
                y = -y + 0x8000;
            Param2 = ( y << 16 ) | x;
            break;

        case WINDOW_OPEN:
            Param1 = GetBool( para2 );
            break;

        case WINDOW_CLOSE:
            Param1 = GetBool( para2 );
            break;

        case PLAYLIST_ID_DEL:
            Param1 = (unsigned int)FindControl( para1 );
            break;

        default:
            break;
    }

    // Get OS specific parameters
    CreateOSEvent( para1, para2, para3 );

    // Free memory
    delete[] msg;
    delete[] para1;
    delete[] para2;
    delete[] para3;

    // Create shortcut
    CreateShortcut();

}
//---------------------------------------------------------------------------
GenericControl * Event::FindControl( string id )
{
    list<Window *>::const_iterator win;
    unsigned int i;

    for( win = p_intf->p_sys->p_theme->WindowList.begin();
         win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
    {
        for( i = 0; i < (*win)->ControlList.size(); i++ )
        {
            if( (*win)->ControlList[i]->IsID( id ) )
                return (*win)->ControlList[i];
        }
    }
    return NULL;

}
//---------------------------------------------------------------------------
int Event::GetBool( string expr )
{
    if( expr == "FALSE" )
    {
        return 0;
    }
    else if( expr == "TRUE" )
    {
        return 1;
    }
    else if( expr == "CHANGE" )
    {
        return 2;
    }
    return 1;
}
//---------------------------------------------------------------------------
void Event::CreateShortcut()
{
    if( Shortcut == "none" )
        return;

    // Initiatization
    char *mod = new char[4];
    char *key = new char[4];

    // Scan the event
    int scan = sscanf( Shortcut.c_str(), "%[^+]+%s", mod, key );

    // Check parameters
    if( scan == 2 )
    {
        Key = (int)key[0];
        if( (string)mod == "ALT" )
            KeyModifier = 1;
        else if( (string)mod == "CTRL" )
            KeyModifier = 2;
        else
            KeyModifier = 0;
    }
    else if( scan == 1 )
    {
        Key = (int)mod[0];
        KeyModifier = 0;
    }

    delete[] mod;
    delete[] key;
}
//---------------------------------------------------------------------------
bool Event::MatchShortcut( int key, int mod )
{
    // Modifier
    // None    = 0
    // ALT     = 1
    // CONTROL = 2
    if( Shortcut != "none" && key == Key && mod == KeyModifier )
        return true;
    else
        return false;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Action
//---------------------------------------------------------------------------
Action::Action( intf_thread_t *_p_intf, string code )
{
    p_intf = _p_intf;

    // Initiatization
    int scan;
    char *evt  = new char[MAX_EVENT_SIZE];
    char *next = new char[MAX_PARAM_SIZE];

    // Create events separated with a semicolon
    while( code != "none" )
    {
        scan  = sscanf( code.c_str(), "%[^;];%s", evt, next );
        EventList.push_back( p_intf->p_sys->p_theme->EvtBank->Get( evt ) );

        // Check if script is finished
        if( scan < 2 )
            code = "none";
        else
            code = next;
    }

    // Free memory
    delete[] evt;
    delete[] next;

}
//---------------------------------------------------------------------------
Action::~Action()
{

}
//---------------------------------------------------------------------------
bool Action::SendEvent()
{
    bool res = false;
    for( list<Event *>::const_iterator evt = EventList.begin();
         evt != EventList.end(); evt++ )
    {
        res |= (*evt)->SendEvent();
    }
    return res;
}
//---------------------------------------------------------------------------
bool Action::MatchEvent( Event *evt, int flag )
{
    list<Event *>::const_iterator event;

    switch( flag )
    {
        case ACTION_MATCH_ALL:
            for( event = EventList.begin(); event != EventList.end(); event++ )
                if( !(*event)->IsEqual( evt ) )
                    return false;
            break;

        case ACTION_MATCH_ONE:
            for( event = EventList.begin(); event != EventList.end(); event++ )
                if( (*event)->IsEqual( evt ) )
                    return true;
            return false;
            break;

        case ACTION_MATCH_FIRST:
            if( !(*EventList.begin())->IsEqual( evt ) )
                return false;
            break;
    }
    return true;
}
//---------------------------------------------------------------------------

