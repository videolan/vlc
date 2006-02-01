/*****************************************************************************
 * vlm_wrapper.cpp : Wrapper around VLM
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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

/* FIXME: This is not wx-specific */

#include "dialogs/vlm/vlm_wrapper.hpp"

VLMWrapper::VLMWrapper( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
    p_vlm = NULL;
}

VLMWrapper::~VLMWrapper()
{
   /* FIXME :you have to destroy vlm here to close
    * but we shouldn't destroy vlm here in case somebody else wants it */
   if( p_vlm )
        vlm_Delete( p_vlm );
}

vlc_bool_t VLMWrapper::AttachVLM()
{
    p_vlm = vlm_New( p_intf );
    return p_vlm ? VLC_TRUE: VLC_FALSE ;
}

void VLMWrapper::LockVLM()
{
    vlc_mutex_lock( &p_vlm->object_lock );
}

void VLMWrapper::UnlockVLM()
{
    vlc_mutex_unlock( &p_vlm->object_lock );
}

void VLMWrapper::AddBroadcast( const char* name, const char* input,
                               const char* output,
                               vlc_bool_t b_enabled, vlc_bool_t b_loop  )
{
    vlm_message_t *message;
    string command = "new " + string(name) + " broadcast";
    vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
    vlm_MessageDelete( message );
    EditBroadcast( name, input, output, b_enabled, b_loop );
}

void VLMWrapper::EditBroadcast( const char* name, const char* input,
                               const char* output,
                               vlc_bool_t b_enabled, vlc_bool_t b_loop  )
{
    vlm_message_t *message;
    string command;
    command = "setup " + string(name) + " inputdel all";
    vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
    vlm_MessageDelete( message );
    command = "setup " + string(name) + " input " + string(input);
    vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
    vlm_MessageDelete( message );
    if( strlen(output) > 0 )
    {
        command = "setup " + string(name) + " output " + string(output);
        vlm_ExecuteCommand( p_vlm, (char*)command.c_str(), &message );
        vlm_MessageDelete( message );
    }
    if( b_enabled )
    {
        command = "setup " + string(name) + " enabled";
        vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
        vlm_MessageDelete( message );
    }
    if( b_loop )
    {
        command = "setup " + string(name) + " loop";
        vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
        vlm_MessageDelete( message );
    }
}

void VLMWrapper::AddVod( const char* name, const char* input,
                         const char* output,
                         vlc_bool_t b_enabled, vlc_bool_t b_loop  )
{
    vlm_message_t *message;
    string command = "new " + string(name) + " vod";
    vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
    vlm_MessageDelete( message );
    EditVod( name, input, output, b_enabled, b_loop );
}

void VLMWrapper::EditVod( const char* name, const char* input,
                          const char* output,
                          vlc_bool_t b_enabled, vlc_bool_t b_loop  )
{
    vlm_message_t *message;
    string command;
    command = "setup " + string(name) + " input " + string(input);
    vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
    vlm_MessageDelete( message );
    if( strlen(output) > 0 )
    {
        command = "setup " + string(name) + " output " + string(output);
        vlm_ExecuteCommand( p_vlm, (char*)command.c_str(), &message );
        vlm_MessageDelete( message );
    }
    if( b_enabled )
    {
        command = "setup " + string(name) + " enabled";
        vlm_ExecuteCommand( p_vlm, command.c_str(), &message );
        vlm_MessageDelete( message );
    }
}
