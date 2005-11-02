/*****************************************************************************
 * vlm_stream.cpp : Implementation of the VLMStream class hierarchy
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: timer.cpp 11981 2005-08-03 15:03:23Z xtophe $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "dialogs/vlm/vlm_stream.hpp"
#include "dialogs/vlm/vlm_wrapper.hpp"

/*****************************************************************************
 * VLMStream class
 *****************************************************************************/
VLMStream::VLMStream( intf_thread_t *_p_intf, vlm_media_t *_p_media,
                      VLMWrapper * _p_vlm )
{
    p_intf = _p_intf;
    p_vlm = _p_vlm;
    p_media = _p_media;
}

VLMStream::~VLMStream()
{
}

void VLMStream::Enable()
{
    p_media->b_enabled = VLC_TRUE;
}

void VLMStream::Disable()
{
    p_media->b_enabled = VLC_FALSE;
}

void VLMStream::Delete()
{
    vlm_message_t *message;
    string command = "del " + string( p_media->psz_name );
    /* FIXME: Should be moved to vlm_Wrapper */
    vlm_ExecuteCommand( p_vlm->GetVLM(), (char*)command.c_str(), & message );
    vlm_MessageDelete( message );
}

/*****************************************************************************
 * VLMBroadcastStream class
 *****************************************************************************/
VLMBroadcastStream::VLMBroadcastStream( intf_thread_t *_p_intf,
 vlm_media_t *_p_media, VLMWrapper *_p_vlm ): VLMStream( _p_intf, _p_media, _p_vlm )
{

}

VLMBroadcastStream::~VLMBroadcastStream()
{
}

void VLMBroadcastStream::Play()
{
    vlm_message_t *message;
    string command = "control " + string( p_media->psz_name ) + " play";
    /* FIXME: Should be moved to vlm_Wrapper */
    vlm_ExecuteCommand( p_vlm->GetVLM(), (char*)command.c_str(), & message );
    vlm_MessageDelete( message );
}

void VLMBroadcastStream::Stop()
{
    vlm_message_t *message;
    string command = "control " + string( p_media->psz_name ) + " stop";
    vlm_ExecuteCommand( p_vlm->GetVLM(), (char*)command.c_str(), & message );
    vlm_MessageDelete( message );
}

void VLMBroadcastStream::Pause()
{
    vlm_message_t *message;
    string command = "control " + string( p_media->psz_name ) + " pause";
    vlm_ExecuteCommand( p_vlm->GetVLM(), (char*)command.c_str(), & message );
    vlm_MessageDelete( message );
}

/*****************************************************************************
 * VLMVODStream class
 *****************************************************************************/
VLMVODStream::VLMVODStream( intf_thread_t *_p_intf, vlm_media_t *_p_media,
                VLMWrapper *_p_vlm ): VLMStream( _p_intf, _p_media, _p_vlm )
{

}

VLMVODStream::~VLMVODStream()
{
}
