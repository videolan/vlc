/*****************************************************************************
 * vout.h: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: vout.h,v 1.4 2003/01/05 01:55:07 massiot Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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

/*****************************************************************************
 * VLCWindow interface
 *****************************************************************************/
@interface VLCWindow : NSWindow
{
    vout_thread_t * p_vout;
}

- (void)setVout:(vout_thread_t *)_p_vout;
- (vout_thread_t *)getVout;

- (void)toggleFullscreen;
- (BOOL)isFullscreen;

- (BOOL)windowShouldClose:(id)sender;

@end

/*****************************************************************************
 * VLCView interface
 *****************************************************************************/
@interface VLCView : NSQuickDrawView
{
}

@end

/*****************************************************************************
 * VLCVout interface
 *****************************************************************************/
@interface VLCVout : NSObject
{
}

- (void)createWindow:(NSValue *)o_value;
- (void)destroyWindow:(NSValue *)o_value;

@end

/*****************************************************************************
 * vout_sys_t: MacOS X video output method descriptor
 *****************************************************************************/
struct vout_sys_t
{
    NSConditionLock * o_lock;

    NSRect s_rect;
    int b_pos_saved;
    VLCWindow * o_window;

    vlc_bool_t b_mouse_moved;
    vlc_bool_t b_mouse_pointer_visible;
    mtime_t i_time_mouse_last_moved;

#ifdef __QUICKTIME__
    CodecType i_codec;
    CGrafPtr p_qdport;
    ImageSequence i_seq;
    MatrixRecordPtr p_matrix;
    DecompressorComponent img_dc;
    ImageDescriptionHandle h_img_descr;
    Ptr p_fullscreen_state;
#endif
};
