/*****************************************************************************
 * VLCVideoView.h: VLC.framework VLCVideoView header
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import <VLC/VLCTime.h>
#import <VLC/VLCPlaylist.h>

@protocol VLCVideoViewDelegate;

/* Notification */
extern NSString * VLCVideoDidChangeVolume;
extern NSString * VLCVideoDidChangeTime;
extern NSString * VLCVideoDidChangeCurrentlyPlayingItem;
extern NSString * VLCVideoDidStop;
extern NSString * VLCVideoDidPause;
extern NSString * VLCVideoDidPlay;

@interface VLCVideoView : NSView
{
    VLCPlaylist   * playlist;
    NSConnection  * connection;
    id              delegate;
    BOOL            stretchVideo;

    void * p_mi;
    void * p_mlp;
}

- (id)initWithFrame:(NSRect)frameRect;
- (void)dealloc;

- (void)setPlaylist: (VLCPlaylist *)newPlaylist;
- (VLCPlaylist *)playlist;

/* Play */
- (void)play;
- (void)playItemAtIndex:(int)index;
- (void)playMedia:(VLCMedia *)media;
- (void)pause;
- (void)setCurrentTime:(VLCTime *)timeObj;

/* State */
- (BOOL)isPlaying;
- (BOOL)isPaused;
- (VLCTime *)currentTime;
- (id)currentPlaylistItem;

/* Video output property */
- (void)setStretchesVideo:(BOOL)flag;
- (BOOL)stretchesVideo;

/* Fullscreen */
- (void)enterFullscreen;
- (void)leaveFullscreen;

/* Delegate */
- (void)setDelegate: (id)newDelegate;
- (id)delegate;
@end

@protocol VLCVideoViewDelegate
- (void)videoDidStop:(NSNotification *)notification;
- (void)videoDidPlay:(NSNotification *)notification;
- (void)videoDidPause:(NSNotification *)notification;
- (void)videoDidPlayNextPlaylistElement:(NSNotification *)notification;

- (void)videoDidChangeVolume:(NSNotification *)notification;

/* Returns NO if the Video shouldn't be paused */
- (BOOL)videoWillPause:(NSNotification *)notification;
/* Returns NO if the Video shouldn't play next playlist element */
- (BOOL)videoWillPlayNextPlaylistElement:(NSNotification *)notification;

/* Posted when the progress of the video has reached a new step
 * (every second?).
 * The -object contained in the notification is the new VLCTime */
- (void)videoDidChangeTime:(NSNotification *)notification;
@end
