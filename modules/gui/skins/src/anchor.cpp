/*****************************************************************************
 * anchor.cpp: Anchor class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: anchor.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


//--- GENERAL ---------------------------------------------------------------
#include <math.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "anchor.h"
#include "window.h"


//---------------------------------------------------------------------------
// Anchors
//---------------------------------------------------------------------------
Anchor::Anchor( intf_thread_t *_p_intf, int x, int y, int len, int priority,
                Window *parent )
{
    p_intf = _p_intf;
    Parent   = parent;
    Left     = x;
    Top      = y;
    Priority = priority;
    Len      = len;
}
//---------------------------------------------------------------------------
bool Anchor::IsInList( Anchor *anc )
{
    // Declare iterator
    list<Anchor *>::const_iterator elt;

    // Iterate through list
    for( elt = HangList.begin(); elt != HangList.end(); elt++)
    {
        if( (*elt) == anc )
            return true;
    }

    return false;
}
//---------------------------------------------------------------------------
void Anchor::Add( Anchor *anc )
{
    HangList.push_back( anc );
}
//---------------------------------------------------------------------------
void Anchor::Remove( Anchor *anc )
{
    HangList.remove( anc );
}
//---------------------------------------------------------------------------
bool Anchor::Hang( Anchor *anc, int mx, int my )
{
    // Get position of anchor
    int x, y, px, py;
    Parent->GetPos( px, py );
    anc->GetPos( x, y );
    x += mx - px;
    y += my - py;

    // Len of 0 is equal to unactivate anchor
    if( Len > 0 && sqrt( (Left-x)*(Left-x) + (Top-y)*(Top-y) ) <= Len )
    {
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------
void Anchor::GetPos( int &x, int &y )
{
    x = Left;
    y = Top;
}
//---------------------------------------------------------------------------

