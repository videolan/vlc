/*****************************************************************************
 * voutgl.h: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
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

#import <Cocoa/Cocoa.h>
#import "VLCOpenGLVoutView.h"

struct vout_sys_t
{
    NSAutoreleasePool * o_pool;
    VLCOpenGLVoutView * o_glview;
    bool                b_saved_frame;
    NSRect              s_frame;
    bool                b_got_frame;

    /* Mozilla plugin-related variables */
    bool                b_embedded;
    int                 i_offx, i_offy;
    int                 i_width, i_height;
    WindowRef           theWindow;
    bool                b_clipped_out;
    Rect                clipBounds, viewBounds;
};

struct vout_window_sys_t
{
    NSAutoreleasePool *o_pool;
};


