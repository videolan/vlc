/*****************************************************************************
 * vlcpeer.cpp: a VideoLAN Client plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlcpeer.cpp,v 1.1 2002/09/17 08:18:24 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

#include "npapi.h"
#include "vlcpeer.h"
#include "vlcplugin.h"

#include "nsMemory.h"


NS_IMPL_ISUPPORTS2( VlcPeer, VlcIntf, nsIClassInfo )

VlcPeer::VlcPeer()
{
    NS_INIT_ISUPPORTS();
}

VlcPeer::VlcPeer( VlcPlugin * plugin )
{
    NS_INIT_ISUPPORTS();
    p_plugin = plugin;
}

VlcPeer::~VlcPeer()
{
    ;
}

NS_IMETHODIMP VlcPeer::Play()
{
    if( p_plugin )
    {
        p_plugin->Play();
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Pause()
{
    if( p_plugin )
    {
        p_plugin->Pause();
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Stop()
{
    if( p_plugin )
    {
        p_plugin->Stop();
    }
    return NS_OK;
}

