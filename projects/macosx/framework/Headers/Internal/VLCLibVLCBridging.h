/*****************************************************************************
 * VLCLibVLCbridging.h: VLCKit.framework VLCLibVLCBridging (Private) header
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import "VLCLibrary.h"
#if !TARGET_OS_IPHONE
#import "VLCStreamOutput.h"
#endif
#import "VLCMediaPlayer.h"

/**
 * Bridges functionality between libvlc and VLCMediaList implementation.
 */
@interface VLCMediaList (LibVLCBridging)
/* Factories */
/**
 * Manufactures new object wrapped around specified media list.
 * \param p_new_mlist LibVLC media list pointer.
 * \return Newly create media list instance using specified media list
 * pointer.
 */
+ (id)mediaListWithLibVLCMediaList:(void *)p_new_mlist;

/* Initializers */
/**
 * Initializes new object wrapped around specified media list.
 * \param p_new_mlist LibVLC media list pointer.
 * \return Newly create media list instance using specified media list
 * pointer.
 */
- (id)initWithLibVLCMediaList:(void *)p_new_mlist;

/* Properties */
@property (readonly) void * libVLCMediaList;    //< LibVLC media list pointer.
@end

/**
 * Bridges functionality between libvlc and VLCMedia implementation.
 */
@interface VLCMedia (LibVLCBridging)
/* Factories */
/**
 * Manufactures new object wrapped around specified media descriptor.
 * \param md LibVLC media descriptor pointer.
 * \return Newly created media instance using specified descriptor.
 */
+ (id)mediaWithLibVLCMediaDescriptor:(void *)md;

/* Initializers */
/**
 * Initializes new object wrapped around specified media descriptor.
 * \param md LibVLC media descriptor pointer.
 * \return Newly created media instance using specified descriptor.
 */
- (id)initWithLibVLCMediaDescriptor:(void *)md;

+ (id)mediaWithMedia:(VLCMedia *)media andLibVLCOptions:(NSDictionary *)options;

/**
 * Returns the receiver's internal media descriptor pointer.
 * \return The receiver's internal media descriptor pointer.
 */
@property (readonly) void * libVLCMediaDescriptor;
@end

/**
 * Bridges functionality between VLCMedia and VLCMediaPlayer
 */
@interface VLCMediaPlayer (LibVLCBridging)

/* Properties */
@property (readonly) void * libVLCMediaPlayer;    //< LibVLC media list pointer.
@end

/**
 * Bridges functionality between VLCMediaPlayer and LibVLC core
 */
@interface VLCMedia (VLCMediaPlayerBridging)
/**
 * Set's the length of the media object.  This value becomes available once the
 * media object is being played.
 * \param value
 */
- (void)setLength:(VLCTime *)value;
@end

/**
 * Bridges functionality between VLCLibrary and LibVLC core.
 */
@interface VLCLibrary (VLCLibVLCBridging)
/**
 * Shared singleton instance of libvlc library instance.
 * \return libvlc pointer of library instance.
 */
+ (void *)sharedInstance;

/**
 * Instance of libvlc library instance.
 * \return libvlc pointer of library instance.
 */
@property (readonly) void * instance;
@end

/**
 * Bridges functionality between VLCLibrary and VLCAudio.
 */
@interface VLCLibrary (VLCAudioBridging)
/**
 * Called by VLCAudio, each library has a singleton VLCaudio instance.  VLCAudio
 * calls this function to let the VLCLibrary instance know how to get in touch
 * with the VLCAudio instance.  TODO: Each media player instance should have it's
 * own audio instance...not each library instance.
 */
- (void)setAudio:(VLCAudio *)value;
@end

/**
 * Bridges functionality between VLCAudio and VLCLibrary.
 */
@interface VLCAudio (VLCAudioBridging)
/* Initializers */
/**
 * Initializes a new object using the specified mediaPlayer instance.
 * \return Newly created audio object using specified VLCMediaPlayer instance.
 */
- (id)initWithMediaPlayer:(VLCMediaPlayer *)mediaPlayer;
@end

/**
 * TODO: Documentation
 */
#if !TARGET_OS_IPHONE
@interface VLCStreamOutput (LibVLCBridge)
- (NSString *)representedLibVLCOptions;
@end
#endif
