/*****************************************************************************
 * vlcpeer.cpp: scriptable peer descriptor
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id$
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
#include "config.h"

#include <vlc/vlc.h>

#ifdef DEBUG
/* We do not want to use nsDebug.h */
#   undef DEBUG
#endif

#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif
#include <nsISupports.h>
#include <nsMemory.h>
#include <npapi.h>

#if !defined(XP_MACOSX) && !defined(XP_UNIX) && !defined(XP_WIN)
#define XP_UNIX 1
#elif defined(XP_MACOSX)
#undef XP_UNIX
#endif

#include "vlcpeer.h"
#include "vlcplugin.h"

NS_IMPL_ISUPPORTS2( VlcPeer, VlcIntf, nsIClassInfo )

/*****************************************************************************
 * Scriptable peer constructor and destructor
 *****************************************************************************/
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

/*****************************************************************************
 * Scriptable peer methods
 *****************************************************************************/
void VlcPeer::Disable()
{
    p_plugin = NULL;
}

/*****************************************************************************
 * Scriptable peer plugin methods
 *****************************************************************************/
NS_IMETHODIMP VlcPeer::Play()
{
    if( p_plugin )
    {
        if( !p_plugin->b_stream && p_plugin->psz_target )
        {
            VLC_AddTarget( p_plugin->i_vlc, p_plugin->psz_target, 0, 0,
                           PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            p_plugin->b_stream = 1;
        }

        VLC_Play( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Pause()
{
    if( p_plugin )
    {
        VLC_Pause( p_plugin->i_vlc );
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Stop()
{
    if( p_plugin )
    {
        VLC_CleanUp( p_plugin->i_vlc );
        p_plugin->b_stream = 0;
    }
    return NS_OK;
}

NS_IMETHODIMP VlcPeer::Fullscreen()
{
    if( p_plugin )
    {
#ifdef XP_MACOSX
#else
        VLC_FullScreen( p_plugin->i_vlc );
#endif
    }
    return NS_OK;
}

