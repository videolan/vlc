/*****************************************************************************
 * VLCTime.h: VLC.framework VLCTime implementation
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

#import <VLC/VLCTime.h>

@implementation VLCTime
- (id)initWithNumber:(NSNumber *)aNumber
{
    if (self = [super init])
    {
        value = [aNumber copy];
    }
    return self;
}

- (void)dealloc
{
    [value release];
    [super dealloc];
}

- (NSNumber *)numberRepresentation
{
    [[value copy] autorelease];
}

- (NSString *)stringRepresentation
{
    int hours = [value intValue] / (3600*1000);
    int minutes = ([value intValue] - hours * 3600) / (60*1000);
    int seconds = ([value intValue] - hours * 3600 * 1000 - minutes * 60 * 1000)/1000;

    return [NSString stringWithFormat:@"%02d:%02d:%02d", hours, minutes, seconds];
}
@end
