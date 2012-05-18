/*****************************************************************************
 * ConvertAndSave.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "ConvertAndSave.h"

@implementation VLCConvertAndSave
static VLCConvertAndSave *_o_sharedInstance = nil;

+ (VLCConvertAndSave *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    // i18n, bla
}

- (void)toggleWindow
{
    [o_window makeKeyAndOrderFront: nil];
}

- (IBAction)windowButtonAction:(id)sender
{
}

- (IBAction)openMedia:(id)sender
{
}

- (IBAction)profileSelection:(id)sender
{
}

- (IBAction)customizeProfile:(id)sender
{
}

- (IBAction)chooseDestination:(id)sender
{
}

@end
