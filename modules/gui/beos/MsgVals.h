/*****************************************************************************
 * MsgVals.h
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: MsgVals.h,v 1.2 2002/09/30 18:30:27 titer Exp $
 *
 * Authors: Tony Castley <tcastley@mail.powerup.com.au>
 *          Stephan Aßmus <stippi@yellowbites.com>
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

#ifndef BEOS_MESSAGE_VALUES_H
#define BEOS_MESSAGE_VALUES_H

#define PLAYING		0
#define PAUSED		1

const uint32 OPEN_FILE			= 'opfl';
const uint32 OPEN_DVD			= 'opdv';
const uint32 OPEN_PLAYLIST		= 'oppl';
const uint32 STOP_PLAYBACK		= 'stpl';
const uint32 START_PLAYBACK		= 'play';
const uint32 PAUSE_PLAYBACK		= 'papl';
const uint32 FASTER_PLAY		= 'fapl';
const uint32 SLOWER_PLAY		= 'slpl';
const uint32 NORMAL_PLAY		= 'nrpl';
const uint32 SEEK_PLAYBACK		= 'seek';
const uint32 VOLUME_CHG			= 'voch';
const uint32 VOLUME_MUTE		= 'mute';
const uint32 SELECT_CHANNEL		= 'chan';
const uint32 SELECT_SUBTITLE	= 'subt';
const uint32 PREV_TITLE			= 'prti';
const uint32 NEXT_TITLE			= 'nxti';
const uint32 TOGGLE_TITLE		= 'tgti';
const uint32 PREV_CHAPTER		= 'prch';
const uint32 NEXT_CHAPTER		= 'nxch';
const uint32 TOGGLE_CHAPTER		= 'tgch';
const uint32 PREV_FILE			= 'prfl';
const uint32 NEXT_FILE			= 'nxfl';
const uint32 NAVIGATE_PREV		= 'navp';	// could be chapter, title or file
const uint32 NAVIGATE_NEXT		= 'navn';	// could be chapter, title or file
const uint32 TOGGLE_ON_TOP		= 'ontp';
const uint32 TOGGLE_FULL_SCREEN	= 'tgfs';
const uint32 RESIZE_50			= 'rshl';
const uint32 RESIZE_100			= 'rsor';
const uint32 RESIZE_200			= 'rsdb';
const uint32 RESIZE_TRUE		= 'rstr';
const uint32 ASPECT_CORRECT		= 'asco';
const uint32 VERT_SYNC			= 'vsyn';
const uint32 WINDOW_FEEL		= 'wfel';
const uint32 SCREEN_SHOT		= 'scrn';
const uint32 INTERFACE_CREATED	= 'ifcr';  /* see VlcApplication::MessageReceived()
                                            * in src/misc/beos_specific.cpp */

#endif	// BEOS_MESSAGE_VALUES_H
