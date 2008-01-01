/*****************************************************************************
 * VLCExceptionHandler.m: VLCExceptionHandler implementation
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

#import "VLCExceptionHandler.h"
#import <ExceptionHandling/ExceptionHandling.h>


@implementation VLCExceptionHandler
+ (void)load
{
    [[NSExceptionHandler defaultExceptionHandler] setDelegate:[[VLCExceptionHandler alloc] init]];
 
    [[NSExceptionHandler defaultExceptionHandler] setExceptionHandlingMask:
                0xffff /* Catch all */ ];

    [[NSExceptionHandler defaultExceptionHandler] setExceptionHangingMask:
                NSHangOnUncaughtExceptionMask|
                NSHangOnUncaughtSystemExceptionMask|
                NSHangOnUncaughtRuntimeErrorMask|
                NSHangOnTopLevelExceptionMask|
                NSHangOnOtherExceptionMask];
}

/* From Apple's guide on exception */
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldLogException:(NSException *)exception mask:(unsigned int)aMask
{
    [self printStackTrace:exception];
    return YES;
}
- (void)printStackTrace:(NSException *)e
{
    NSString *stack = [[e userInfo] objectForKey:NSStackTraceKey];
    if (!stack)
    {
        NSLog(@"No stack trace available.");
        return;
    }

    NSTask *ls = [[NSTask alloc] init];
    NSString *pid = [[NSNumber numberWithInt:[[NSProcessInfo processInfo] processIdentifier]] stringValue];
    NSMutableArray *args = [NSMutableArray arrayWithCapacity:20];

    [args addObject:@"-p"];
    [args addObject:pid];
    [args addObjectsFromArray:[stack componentsSeparatedByString:@"  "]];
    /* Note: function addresses are separated by double spaces, not a single space. */

    [ls setLaunchPath:@"/usr/bin/atos"];
    [ls setArguments:args];
    [ls launch];
    [ls release];
}

@end
