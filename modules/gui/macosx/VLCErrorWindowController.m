/*****************************************************************************
 * HelpWindowController.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Paul Kühne <fkuehne -at- videolan.org>
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

#import "VLCErrorWindowController.h"

#import "VLCStringUtility.h"

@implementation VLCErrorWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"ErrorPanel"];
    if (self) {
        /* init data sources */
        o_errors = [[NSMutableArray alloc] init];
        o_icons = [[NSMutableArray alloc] init];
    }

    return self;
}

- (void)windowDidLoad
{
    /* init strings */
    [[self window] setTitle: _NS("Errors and Warnings")];
    [o_cleanup_button setTitle: _NS("Clean up")];
}

-(void)addError: (NSString *)o_error withMsg:(NSString *)o_msg
{
    /* format our string as desired */
    NSMutableAttributedString * ourError;
    ourError = [[NSMutableAttributedString alloc] initWithString:
                [NSString stringWithFormat:@"%@\n%@", o_error, o_msg]
                                                      attributes:
                [NSDictionary dictionaryWithObject: [NSFont systemFontOfSize:11] forKey: NSFontAttributeName]];
    [ourError
     addAttribute: NSFontAttributeName
     value: [NSFont boldSystemFontOfSize:11]
     range: NSMakeRange(0, [o_error length])];
    [o_errors addObject: ourError];

    [o_icons addObject: [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kAlertStopIcon)]];

    [o_error_table reloadData];
}

-(IBAction)cleanupTable:(id)sender
{
    [o_errors removeAllObjects];
    [o_icons removeAllObjects];
    [o_error_table reloadData];
}

/*----------------------------------------------------------------------------
 * data source methods
 *---------------------------------------------------------------------------*/
- (NSInteger)numberOfRowsInTableView:(NSTableView *)theDataTable
{
    return [o_errors count];
}

- (id)tableView:(NSTableView *)theDataTable objectValueForTableColumn:
(NSTableColumn *)theTableColumn row: (NSInteger)row
{
    if ([[theTableColumn identifier] isEqualToString: @"error_msg"])
        return [o_errors objectAtIndex:row];

    if ([[theTableColumn identifier] isEqualToString: @"icon"])
        return [o_icons objectAtIndex:row];

    return @"";
}

@end
