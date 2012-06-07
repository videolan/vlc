/*****************************************************************************
 * VLCMediaListPlayer.h: VLCKit.framework VLCMediaListPlayer implementation
 *****************************************************************************
 * Copyright (C) 2009 Pierre d'Herbemont
 * Partial Copyright (C) 2009 Felix Paul Kühne
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Paul Kühne <fkuehne # videolan.org
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

@class VLCMedia, VLCMediaPlayer, VLCMediaList;

/**
 * VLCRepeatMode
 * (don't repeat anything, repeat one, repeat all)
 */
enum VLCRepeatMode {
    VLCDoNotRepeat,
    VLCRepeatCurrentItem,
    VLCRepeatAllItems
};
typedef NSInteger VLCRepeatMode;

@interface VLCMediaListPlayer : NSObject {
    void *instance;
    VLCMedia *_rootMedia;
    VLCMediaPlayer *_mediaPlayer;
    VLCMediaList *_mediaList;
    VLCRepeatMode _repeatMode;
}

@property (readwrite, retain) VLCMediaList *mediaList;

/**
 * rootMedia - Use this method to play a media and its subitems.
 * This will erase mediaList.
 * Setting mediaList will erase rootMedia.
 */
@property (readwrite, retain) VLCMedia *rootMedia;
@property (readonly, retain) VLCMediaPlayer *mediaPlayer;


/**
 * Basic play and stop are here. For other method, use the mediaPlayer.
 * This may change.
 */
- (void)play;
- (void)stop;

/**
 * Playmode selection (don't repeat anything, repeat one, repeat all)
 * See VLCRepeatMode.
 */

@property (readwrite) VLCRepeatMode repeatMode;

/**
 * media must be in the current media list.
 */
- (void)playMedia:(VLCMedia *)media;

@end
