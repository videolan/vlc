/*****************************************************************************
 * event.h: Event class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: event.h,v 1.5 2003/04/12 21:43:27 asmax Exp $
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


#ifndef VLC_SKIN_EVENT
#define VLC_SKIN_EVENT

//--- GENERAL ---------------------------------------------------------------
#include <string>
#include <list>
using namespace std;



//---------------------------------------------------------------------------
// VLC specific messages
//---------------------------------------------------------------------------

#define MAX_EVENT_SIZE 30
#define MAX_PARAM_SIZE 20

#if !defined _WIN32
#define WM_APP 0x8000
#endif

#define VLC_MESSAGE         (WM_APP)
#define VLC_WINDOW          (WM_APP + 1000)
#define VLC_CONTROL         (WM_APP + 2000)

// VLC messages
#define VLC_NOTHING         (VLC_MESSAGE + 1)
#define VLC_SHOW            (VLC_MESSAGE + 2)
#define VLC_HIDE            (VLC_MESSAGE + 3)

#define VLC_QUIT            (VLC_MESSAGE + 4)
#define VLC_OPEN            (VLC_MESSAGE + 5)
#define VLC_LOAD_SKIN       (VLC_MESSAGE + 6)
#define VLC_DROP            (VLC_MESSAGE + 7)

#define VLC_LOG_SHOW        (VLC_MESSAGE + 20)
#define VLC_LOG_CLEAR       (VLC_MESSAGE + 22)

#define VLC_INTF_REFRESH    (VLC_MESSAGE + 30)
#define VLC_CHANGE_TRAY     (VLC_MESSAGE + 31)
#define VLC_CHANGE_TASKBAR  (VLC_MESSAGE + 32)

#define VLC_FULLSCREEN      (VLC_MESSAGE + 40)

// Stream control
#define VLC_PLAY            (VLC_MESSAGE + 101)
#define VLC_STOP            (VLC_MESSAGE + 102)
#define VLC_PAUSE           (VLC_MESSAGE + 103)
#define VLC_NEXT            (VLC_MESSAGE + 104)
#define VLC_PREV            (VLC_MESSAGE + 105)
#define VLC_STREAMPOS       (VLC_MESSAGE + 106)
#define VLC_ENDSTREAMPOS    (VLC_MESSAGE + 107)
#define VLC_TOTALSTREAMPOS  (VLC_MESSAGE + 108)
#define VLC_STREAMNAME      (VLC_MESSAGE + 109)
#define VLC_HELP_TEXT       (VLC_MESSAGE + 110)

// Volume control
#define VLC_VOLUME_CHANGE   (VLC_MESSAGE + 201)
#define VLC_VOLUME_MUTE     (VLC_MESSAGE + 202)
#define VLC_VOLUME_UP       (VLC_MESSAGE + 203)
#define VLC_VOLUME_DOWN     (VLC_MESSAGE + 204)
#define VLC_VOLUME_SET      (VLC_MESSAGE + 205)

// Playlist events
#define VLC_PLAYLIST_ADD_FILE (VLC_MESSAGE + 301)
#define VLC_TEST_ALL_CLOSED (VLC_MESSAGE + 600)

// Network events
#define VLC_NET_ADDUDP      (VLC_MESSAGE + 701)

// Window event
#define WINDOW_MOVE         (VLC_WINDOW + 1)
#define WINDOW_OPEN         (VLC_WINDOW + 2)
#define WINDOW_CLOSE        (VLC_WINDOW + 3)
#define WINDOW_SHOW         (VLC_WINDOW + 4)
#define WINDOW_HIDE         (VLC_WINDOW + 5)
#define WINDOW_FADE         (VLC_WINDOW + 6)
#define WINDOW_LEAVE        (VLC_WINDOW + 7)
#define WINDOW_REFRESH      (VLC_WINDOW + 8)

// Control event
#define CTRL_ENABLED        (VLC_CONTROL + 1)
#define CTRL_VISIBLE        (VLC_CONTROL + 2)
#define CTRL_SYNCHRO        (VLC_CONTROL + 3)

#define CTRL_SET_SLIDER     (VLC_CONTROL + 10)
#define CTRL_SET_TEXT       (VLC_CONTROL + 11)

// Control event by ID
#define CTRL_ID_VISIBLE     (VLC_CONTROL + 100)
#define CTRL_ID_ENABLED     (VLC_CONTROL + 101)
#define CTRL_ID_MOVE        (VLC_CONTROL + 102)

// Control definition
#define CTRL_SLIDER         (VLC_CONTROL + 301)
#define CTRL_TIME           (VLC_CONTROL + 302)
#define CTRL_PLAYLIST       (VLC_CONTROL + 303)

// Playlist
#define PLAYLIST_ID_DEL     (VLC_CONTROL + 400)

//---------------------------------------------------------------------------
struct intf_thread_t;
class GenericControl;
class Window;
class Event;



//---------------------------------------------------------------------------
// EVENT CLASS
//---------------------------------------------------------------------------
class Event
{
    protected:
        string          EventDesc;
        unsigned int    Message;
        unsigned int    Param1;
        long            Param2;
        unsigned int    GetMessageType( string desc );
        string          Shortcut;
        int             KeyModifier;
        int             Key;
        intf_thread_t * p_intf;

        // Transform expr to special boolean :
        //   0 = false
        //   1 = true
        //   2 = change boolean
        int GetBool( string expr );

    public:
        // Constructor
        Event( intf_thread_t *_p_intf, string Desc, string shortcut );
        Event( intf_thread_t *_p_intf, unsigned int msg, unsigned int par1,
               long par2 );

        // Destructor
        virtual ~Event();
        void DestructParameters();

        // General operations on events
        GenericControl * FindControl( string id );
        void CreateEvent();
        virtual void CreateOSEvent( string para1, string para2,
                                    string para3 ) = 0;
        virtual bool IsEqual( Event *evt );

        // Event sending
        virtual bool SendEvent() = 0;
        void PostTextMessage( string text );
        void PostSynchroMessage( bool autodelete = false );

        // Shortcuts methods
        bool MatchShortcut( int key, int mod );
        void CreateShortcut();

        // Getters
        unsigned int GetMessage()   { return Message; }
        unsigned int GetParam1()    { return Param1; }
        long         GetParam2()    { return Param2; }

        // Setters
        void SetParam1( unsigned int p1 )    { Param1 = p1; }
        void SetParam2( unsigned int p2 )    { Param2 = p2; }
};
//---------------------------------------------------------------------------

#define ACTION_MATCH_ALL    0
#define ACTION_MATCH_FIRST  1
#define ACTION_MATCH_ONE    2

//---------------------------------------------------------------------------
class Action
{
    private:
        list<Event *> EventList;
        int GetBool( string expr );
        intf_thread_t * p_intf;

    public:
        // Constructor
        Action::Action( intf_thread_t *_p_intf, string code );

        // Destructor
        Action::~Action();

        // Send event
        bool SendEvent();
        bool MatchEvent( Event *evt, int flag = ACTION_MATCH_ALL );
};
//---------------------------------------------------------------------------

#endif
