/*****************************************************************************
 * VLCPlayerChapter.m: MacOS X interface module
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

#import "VLCPlayerChapter.h"

#import "extensions/NSString+Helpers.h"

@implementation VLCPlayerChapter

- (instancetype)initWithChapter:(const struct vlc_player_chapter *)p_chapter
{
    self = [super init];
    if (self && p_chapter != NULL) {
        _name = toNSStr(p_chapter->name);
        _time = p_chapter->time;
        _timeString = [NSString stringWithTimeFromTicks:_time];
    }
    return self;
}

@end
