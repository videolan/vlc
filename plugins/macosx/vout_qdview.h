/*****************************************************************************
 * vout_qdview.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: vout_qdview.h,v 1.2 2002/03/19 03:33:52 jlj Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#import "vout_vlc_wrapper.h"

@interface VLCView : NSQuickDrawView
{
    void *p_vout;
    Vout_VLCWrapper *o_wrapper;
}

- (id)initWithWrapper:(Vout_VLCWrapper *)_o_wrapper forVout:(void *)_p_vout;

@end
