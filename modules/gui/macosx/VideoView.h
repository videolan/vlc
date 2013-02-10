/*****************************************************************************
 * VideoView.h: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import <vlc_vout.h>


/*****************************************************************************
 * VLCVoutView interface
 *****************************************************************************/
@interface VLCVoutView : NSView
{
    NSInteger i_lastScrollWheelDirection;
    NSTimeInterval t_lastScrollEvent;

    CGFloat f_cumulated_magnification;

    vout_thread_t *p_vout;
}

- (void)setVoutThread:(vout_thread_t *)p_vout_thread;
- (vout_thread_t *)voutThread;
- (void)releaseVoutThread;

@end
