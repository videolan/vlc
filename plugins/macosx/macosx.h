/*****************************************************************************
 * macosx.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: macosx.h,v 1.10 2002/06/01 12:32:00 sam Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *          Florian G. Pflug <fgp@phlo.org>
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

#import <Cocoa/Cocoa.h>
#import <QuickTime/QuickTime.h>

#include "vout_window.h"
#include "vout_qdview.h"

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_s
{
    NSPort *o_port;
    NSAutoreleasePool *o_pool;

    vlc_bool_t b_mute;
    int i_saved_volume;
    
    int i_part;
    vlc_bool_t b_disabled_menus;
};

/*****************************************************************************
 * vout_sys_t: MacOS X video output method descriptor
 *****************************************************************************/
struct vout_sys_s
{
    VLCWindow *o_window;

    NSRect s_rect;
    int b_pos_saved;

    vlc_bool_t b_mouse_moved;
    vlc_bool_t b_mouse_pointer_visible;
    mtime_t i_time_mouse_last_moved;
    
    CodecType i_codec;
    CGrafPtr p_qdport;
    ImageSequence i_seq;
    MatrixRecordPtr p_matrix;
    DecompressorComponent img_dc;
    ImageDescriptionHandle h_img_descr;
};

/*****************************************************************************
 * vout_req_t: MacOS X video output request 
 *****************************************************************************/
#define VOUT_REQ_CREATE_WINDOW  0x00000001
#define VOUT_REQ_DESTROY_WINDOW 0x00000002

typedef struct vout_req_s
{
    int i_type;
    int i_result;

    NSConditionLock *o_lock;

    vout_thread_t *p_vout;
} vout_req_t;
