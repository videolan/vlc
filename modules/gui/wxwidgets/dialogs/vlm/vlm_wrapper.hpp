/*****************************************************************************
 * vlm_wrapper.hpp: Wrapper for VLM
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/* This is not WX-specific */

#include <vlc/vlc.h>
#include <vlc_interface.h>
#include <vlc_vlm.h>
#include "dialogs/vlm/vlm_stream.hpp"

#include <vector>
#include <string>
using namespace std;

class VLMWrapper
{
public:
    VLMWrapper( intf_thread_t * );
    virtual ~VLMWrapper();

    bool AttachVLM();
    void LockVLM();
    void UnlockVLM();

    void AddBroadcast( const char*, const char*, const char*,
                       bool b_enabled = true,
                       bool b_loop = true );
    void EditBroadcast( const char*, const char*, const char*,
                       bool b_enabled = true,
                       bool b_loop = true );
    void AddVod( const char*, const char*, const char*,
                       bool b_enabled = true,
                       bool b_loop = true );
    void EditVod( const char*, const char*, const char*,
                       bool b_enabled = true,
                       bool b_loop = true );

    unsigned int NbMedia() { if( p_vlm ) return p_vlm->i_media; return 0; }
    vlm_media_t *GetMedia( int i )
    { if( p_vlm ) return p_vlm->media[i]; return NULL; }

    vlm_t* GetVLM() { return p_vlm; }

protected:

private:
    vlm_t *p_vlm;
    intf_thread_t *p_intf;

};
