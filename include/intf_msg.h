/*****************************************************************************
 * intf_msg.h: messages interface
 * This library provides basic functions for threads to interact with user
 * interface, such as message output. See config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_msg.h,v 1.18 2002/02/19 00:50:19 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
 * Prototypes
 *****************************************************************************/
#ifndef PLUGIN
void intf_Msg            ( char *psz_format, ... );
void intf_ErrMsg         ( char *psz_format, ... );
void intf_WarnMsg        ( int i_level, char *psz_format, ... );
void intf_StatMsg        ( char *psz_format, ... );

void intf_WarnHexDump    ( int i_level, void *p_data, int i_size );
#else
#   define intf_MsgSub p_symbols->intf_MsgSub
#   define intf_MsgUnsub p_symbols->intf_MsgUnsub

#   define intf_Msg p_symbols->intf_Msg
#   define intf_ErrMsg p_symbols->intf_ErrMsg
#   define intf_StatMsg p_symbols->intf_StatMsg
#   define intf_WarnMsg p_symbols->intf_WarnMsg
#endif

