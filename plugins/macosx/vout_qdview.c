/*****************************************************************************
 * vout_qdview.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: vout_qdview.c,v 1.1 2002/02/18 01:34:44 jlj Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "macosx.h"

/*****************************************************************************
 * VLCView implementation 
 *****************************************************************************/
@implementation VLCView

- (id)initWithVout:(struct vout_thread_s *)_p_vout
{
    if( [super init] == nil )
        return nil;

    p_vout = _p_vout;

    return self;
}

- (void)drawRect:(NSRect)rect
{
    [super drawRect: rect];
    p_vout->i_changes |= VOUT_SIZE_CHANGE;
}

@end
