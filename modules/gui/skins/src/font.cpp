/*****************************************************************************
 * font.cpp: Font class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: font.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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
#include "font.h"



//---------------------------------------------------------------------------
// Font object
//---------------------------------------------------------------------------
Font::Font( intf_thread_t *_p_intf, string fontname, int size, int color,
    int weight, bool italic, bool underline )
{
    p_intf = _p_intf;

    FontName  = fontname;
    Size      = size;
    Color     = OSAPI_GetNonTransparentColor( color );
    Italic    = italic;
    Underline = underline;
    Weight    = weight;
    if( Weight > 1000 )
        Weight = 1000;
    if( Weight < 1 )
        Weight = 1;
}
//---------------------------------------------------------------------------
Font::~Font()
{
}
//---------------------------------------------------------------------------
