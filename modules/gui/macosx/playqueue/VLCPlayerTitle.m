/*****************************************************************************
 * VLCPlayerTitle.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCPlayerTitle.h"

#import "extensions/NSString+Helpers.h"
#import "playqueue/VLCPlayerChapter.h"

@implementation VLCPlayerTitle

- (instancetype)initWithTitle:(const struct vlc_player_title *)p_title atIndex:(size_t)index
{
    self = [super init];
    if (self) {
        _index = index;
        _name = toNSStr(p_title->name);
        _length = p_title->length;
        _lengthString = [NSString stringWithTimeFromTicks:_length];
        _flags = p_title->flags;
        _chapterCount = p_title->chapter_count;

        NSMutableArray * const chapters = [NSMutableArray arrayWithCapacity:self.chapterCount];
        for (NSUInteger i = 0; i < self.chapterCount; i++) {
            const vlc_tick_t nextTime = (i + 1 < self.chapterCount)
                ? p_title->chapters[i + 1].time
                : p_title->length;
            const vlc_tick_t duration = nextTime - p_title->chapters[i].time;
            VLCPlayerChapter * const chapter =
                [[VLCPlayerChapter alloc] initWithChapter:&p_title->chapters[i]
                                                 duration:duration];
            [chapters addObject:chapter];
        }
        _chapters = chapters.copy;
    }
    return self;
}

@end
