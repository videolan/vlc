/*****************************************************************************
 * banks.h: Bitmap bank, Event bank, Font bank and OffSet bank
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: banks.h,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


#ifndef VLC_SKIN_BANKS
#define VLC_SKIN_BANKS

//---------------------------------------------------------------------------
//--- GENERAL ---------------------------------------------------------------
#include <map>
#include <list>
#include <string>
using namespace std;

//---------------------------------------------------------------------------
#define DEFAULT_BITMAP_NAME   "DEFAULT_BITMAP"
#define DEFAULT_FONT_NAME     "DEFAULT_FONT"
#define DEFAULT_EVENT_NAME    "DEFAULT_EVENT"

//---------------------------------------------------------------------------
struct intf_thread_t;
class Bitmap;
class Font;
class Event;

//---------------------------------------------------------------------------
class BitmapBank
{
    private:
        map<string,Bitmap *> Bank;
        intf_thread_t *p_intf;
    public:
        BitmapBank( intf_thread_t *_p_intf );
        ~BitmapBank();
        bool Add( string Id, string FileName, int AColor );   // Add a bitmap
        Bitmap * Get( string Id );  // Return the bitmap with matching ID
};
//---------------------------------------------------------------------------
class FontBank
{
    private:
        map<string,Font *> Bank;
        intf_thread_t *p_intf;
    public:
        FontBank( intf_thread_t *_p_intf );
        ~FontBank();
        bool Add( string name, string fontname, int size,
                  int color, int weight, bool italic, bool underline );
        Font * Get( string Id ); // Return the font with matching ID
};
//---------------------------------------------------------------------------
class EventBank
{
    private:
        map<string,Event *> Bank;
        intf_thread_t *p_intf;
    public:
        EventBank( intf_thread_t *_p_intf );
        ~EventBank();
        bool Add( string Name, string EventDesc, string shortcut );
        void TestShortcut( int key, int mod );
        Event * Get( string Id );   // Return the event with matching ID
        void Init();
};
//---------------------------------------------------------------------------
class OffSetBank
{
    private:
        int XOff;
        int YOff;
        list<int> XList;
        list<int> YList;
        intf_thread_t *p_intf;
    public:
        OffSetBank( intf_thread_t *_p_intf );
        ~OffSetBank();
        void PushOffSet( int X, int Y );
        void PopOffSet();
        void GetOffSet( int &X, int &Y );
};
//---------------------------------------------------------------------------

#endif

