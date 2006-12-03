/*****************************************************************************
 * vlc_intf_strings.h : Strings for main interfaces
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_ISTRINGS_H
#define _VLC_ISTRINGS_H 1

/*************** Open dialogs **************/

#define I_POP_SEL_FILES N_("Select one or more files to open")

/******************* Menus *****************/

/* Playlist popup */
#define I_POP_PLAY N_("Play")
#define I_POP_PREPARSE N_("Fetch information")
#define I_POP_DEL N_("Delete")
#define I_POP_INFO N_("Information...")
#define I_POP_SORT N_("Sort")
#define I_POP_ADD N_("Add node")
#define I_POP_STREAM N_("Stream...")
#define I_POP_SAVE N_("Save...")

/*************** Preferences *************/

#define I_HIDDEN_ADV N_( "Some options are available but hidden. "\
                         "Check \"Advanced options\" to see them." )
#endif
