/*****************************************************************************
 * gtk2_dialog.h: GTK2 implementation of some dialog boxes
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_dialog.h,v 1.2 2003/04/20 20:28:39 ipkiss Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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


#ifndef VLC_SKIN_GTK2_DIALOG
#define VLC_SKIN_GTK2_DIALOG

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;



//---------------------------------------------------------------------------
class GTK2OpenFileDialog : OpenFileDialog
{
    private:

    protected:

    public:
        // Constructors
        GTK2OpenFileDialog( intf_thread_t *_p_intf, string title,
                             bool multiselect );

        // Destructors
        virtual ~GTK2OpenFileDialog();

        virtual void AddFilter( string name, string type );
        virtual bool Open();
};
//---------------------------------------------------------------------------

#endif

