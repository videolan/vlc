/*****************************************************************************
 * vout_vlc_wrapper.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: vout_vlc_wrapper.h,v 1.2 2002/05/07 20:17:07 massiot Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net> 
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

#define MOUSE_MOVED             0x00000001
#define MOUSE_NOT_MOVED         0x00000002
#define MOUSE_LAST_MOVED        0x00000004
#define MOUSE_NOT_LAST_MOVED    0x00000008

@interface Vout_VLCWrapper : NSObject
{

}

+ (Vout_VLCWrapper *)instance;
+ (NSPort *)sendPort;

- (void)mouseEvent:(unsigned int)ui_status forVout:(void *)_p_vout;
- (BOOL)keyDown:(NSEvent *)o_event forVout:(void *)_p_vout;

@end

@interface Vout_VLCWrapper (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
@end
