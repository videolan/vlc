/*****************************************************************************
 * vlcplugin.cpp: a VideoLAN Client plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlcplugin.cpp,v 1.1 2002/09/17 08:18:24 sam Exp $
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
#include <npapi.h>

#include <vlc/vlc.h>

#include "vlcpeer.h"
#include "vlcplugin.h"

/*****************************************************************************
 * VlcPlugin methods
 *****************************************************************************/
VlcPlugin::VlcPlugin( NPP instance )
{
    p_instance = instance; 
    p_peer = NULL;
}


VlcPlugin::~VlcPlugin()
{
    if( p_peer )
    {
        p_peer->Disable();
        p_peer->Release();
    }
}


void VlcPlugin::SetInstance( NPP instance )
{
    p_instance = instance;
}


NPP VlcPlugin::GetInstance()
{
    return p_instance;
}


void VlcPlugin::SetFileName(const char * filename)
{
fprintf(stderr, "VlcPlugin::SetFilename %s\n", filename);
#if 0
    FILE * fh;
    fh = fopen(filename, "rb");
    if(!fh)
    {
        fprintf(stderr, "Error while opening %s.\n", filename);
        return;
    }
    fseek(fh, 0, SEEK_END);
    m_lSize = ftell(fh);
    m_szSound = (char*) malloc(m_lSize);
    if(!m_szSound)
    {
        fprintf(stderr, "Error while allocating memory.\n");
        fclose(fh);
        return;
    }
    rewind(fh);
    long pos = 0;
    do
    {
        pos += fread(m_szSound + pos, 1, m_lSize - pos, fh);
        fprintf(stderr, "pos = %d\n", pos);
    }
    while (pos < m_lSize -1);
    fclose (fh);
    fprintf(stderr, "File loaded\n");
#endif
    return; 
}

void VlcPlugin::Play()
{
fprintf(stderr, "VlcPlugin::Play\n");
}

void VlcPlugin::Pause()
{
fprintf(stderr, "VlcPlugin::Pause\n");
}

void VlcPlugin::Stop()
{
fprintf(stderr, "VlcPlugin::Stop\n");
}

VlcIntf* VlcPlugin::getScriptable()
{
    if( !p_peer )
    {
        p_peer = new VlcPeer( this );
        if( p_peer == NULL ) 
        {
            return NULL;
        }
        
        NS_ADDREF( p_peer );
    }
    // a getter should addref for its caller.
    NS_ADDREF( p_peer );
    return p_peer;
}

