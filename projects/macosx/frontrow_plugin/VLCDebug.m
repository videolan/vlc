/*****************************************************************************
 * VLCDebug.m: Front Row plugin
 *****************************************************************************
 * Copyright (C) 2007 - 2008 the VideoLAN Team
 * $Id$
 *
 * Authors: hyei
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

#import "VLCDebug.h"

static VLCFakeObject * sharedFakeObject = nil;

@implementation VLCFakeObject

+ (id)sharedFakeObject
{
    if(sharedFakeObject == nil) {
        sharedFakeObject = [[VLCFakeObject alloc] init];
    }
    
    return sharedFakeObject;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSLog(@"methodSignatureForSelector: %@", NSStringFromSelector(aSelector));
    return nil;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    NSLog(@"forwardInvocation: %@", anInvocation);
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return YES;
}

@end