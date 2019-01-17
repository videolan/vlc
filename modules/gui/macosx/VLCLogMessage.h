/*****************************************************************************
 * VLCLogMessage.h: Log message class
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import <Foundation/Foundation.h>
#import <vlc_common.h>

@interface VLCLogMessage : NSObject

@property (readonly, getter=typeName) NSString *typeName;
@property (readonly) int type;
@property (readonly) NSString *message;
@property (readonly) NSString *component;
@property (readonly) NSString *function;
@property (readonly) NSString *location;
@property (readonly, getter=fullMessage) NSString *fullMessage;

+ (instancetype)logMessage:(char *)msg type:(int)type info:(const vlc_log_t *)info;
- (instancetype)initWithMessage:(char *)message type:(int)type info:(const vlc_log_t *)info;

@end
