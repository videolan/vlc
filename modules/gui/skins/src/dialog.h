/*****************************************************************************
 * dialog.h: Classes for some dialog boxes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dialog.h,v 1.2 2003/03/20 09:29:07 karibu Exp $
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


#ifndef VLC_SKIN_DIALOG
#define VLC_SKIN_DIALOG

//--- GENERAL ---------------------------------------------------------------
#include <string>
#include <list>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
struct msg_subscription_t;

//---------------------------------------------------------------------------
class OpenFileDialog
{
    private:

    protected:
        char   *Filter;
        int    FilterLength;
        string Title;
        bool   MultiSelect;

        intf_thread_t *p_intf;

    public:
        // Constructors
        OpenFileDialog( intf_thread_t *_p_intf, string title,
                        bool multiselect );

        // Destructors
        virtual ~OpenFileDialog();

        // List of files
        list<string> FileList;

        // Specific methods
        virtual void AddFilter( string name, string type ) = 0;
        virtual bool Open() = 0;
};
//---------------------------------------------------------------------------
class LogWindow
{
    protected:
        bool Visible;
        intf_thread_t *p_intf;

    public:
        // Constructors
        LogWindow( intf_thread_t *_p_intf );

        // Destructors
        virtual ~LogWindow();

        virtual void Clear() = 0;
        virtual void AddLine( string line ) = 0;
        virtual void ChangeColor( int color, bool bold = false ) = 0;
        virtual void Show() = 0;
        virtual void Hide() = 0;

        // Getters
        bool IsVisible()    { return Visible; };

        void Update( msg_subscription_t *Sub );

};
//---------------------------------------------------------------------------

#endif

