/*****************************************************************************
 * MsgVals.h
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: MsgVals.h,v 1.6 2001/06/15 09:07:10 tcastley Exp $
 *
 * Authors: Tony Castley <tcastley@mail.powerup.com.au>
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

/* MsgVals.h */
#define PLAYING		0
#define PAUSED		1

const uint32 OPEN_FILE      = 'OPFL';
const uint32 OPEN_DVD       = 'OPDV';
const uint32 OPEN_PLAYLIST  = 'OPPL';
const uint32 STOP_PLAYBACK  = 'STPL';
const uint32 START_PLAYBACK = 'PLAY';
const uint32 PAUSE_PLAYBACK = 'PAPL';
const uint32 FASTER_PLAY    = 'FAPL';
const uint32 SLOWER_PLAY    = 'SLPL';
const uint32 SEEK_PLAYBACK  = 'SEEK';
const uint32 VOLUME_CHG     = 'VOCH';
const uint32 VOLUME_MUTE	= 'MUTE';
const uint32 SELECT_CHANNEL = 'CHAN';
const uint32 SELECT_SUBTITLE = 'SUBT';

