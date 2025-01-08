/*****************************************************************************
 * VLCDrawable.h :
 *****************************************************************************
 * Copyright (C) 2023-2024 VLC authors and VideoLAN
 *
 * Authors: Maxime Chapelet <umxprime at videolabs dot io>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

# import <TargetConditionals.h>
# if TARGET_OS_OSX
#     import <Cocoa/Cocoa.h>
#     define VLCView NSView
# else
#     import <Foundation/Foundation.h>
#     import <UIKit/UIKit.h>
#     define VLCView UIView
# endif

/**
 * Protocol used by the picture in picture to control playback or gather media
 * infos
 */
@protocol VLCPictureInPictureMediaControlling <NSObject>

/// Called by picture in picture to play/resume the media
- (void)play;

/// Called by picture in picture to pause the media
- (void)pause;

/// Called by picture in picture to seek with time offset
/// - Parameter offset: offset duration in milliseconds
/// - Parameter completion: block to call as soon as seek is finished
- (void)seekBy:(int64_t)offset completion:(dispatch_block_t)completion;

/// Called by picture in picture to get the current media duration
/// - Returns Must return media duration in milliseconds
- (int64_t)mediaLength;

/// Called by picture in picture to get the current media duration
/// - Returns Must return current media time in milliseconds
- (int64_t)mediaTime;

/// Called by picture in picture to figure out if media is seekable
/// - Returns Must return YES if media is seekable, else return NO
- (BOOL)isMediaSeekable;

/// Called by picture in picture to get the media playback status
/// - Returns Must return YES if media is playing, else return NO
- (BOOL)isMediaPlaying;
@end

/**
 * Protocol used by the client to control picture in picture activation and
 * state update
 */
@protocol VLCPictureInPictureWindowControlling <NSObject>

/// Property to set the event handler block that will notify when the picture
/// in picture is started or stopped
@property (nonatomic) void(^stateChangeEventHandler)(BOOL isStarted);

/// Call to present the display in picture in picture mode
- (void)startPictureInPicture;

/// Call to stop picture in picture
- (void)stopPictureInPicture;

/// Must be called each time media info is updated or playback state has changed
- (void)invalidatePlaybackState;
@end

/**
 * Protocol that can be used by the client to conform an object to expected
 * selectors for a video output display.
 */
@protocol VLCDrawable <NSObject>

/// Add a view to a view hierarchy
/// - Parameter view: the display view to present, can be an UIView or a NSView
- (void)addSubview:(VLCView *)view;

/// Get the display view's parent's frame bounds
/// - Returns Must return the display view's parent's frame bounds
- (CGRect)bounds;
@end

/**
 * Protocol that can be used by the client to enable picture in picture for the
 * video output display.
 */
@protocol VLCPictureInPictureDrawable <NSObject>

/// Get the object to let picture in picture control playback or gather media
/// infos
/// - Returns Must return the object that handles playback controls for
/// picture in picture
- (id<VLCPictureInPictureMediaControlling>) mediaController;

/// Get the block that will be called once picture in picture is ready to be used
/// - Returns Must return the block where picture in picture activation
/// controller is passed
- (void (^)(id<VLCPictureInPictureWindowControlling>)) pictureInPictureReady;
@end
