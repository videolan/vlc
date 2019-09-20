/*****************************************************************************
 * HelpWindowController.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#import "extensions/NSString+Helpers.h"

@interface VLCErrorWindowController()
{
    NSMutableArray *_errors;
    NSMutableArray *_icons;
}
@end

@implementation VLCErrorWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"ErrorPanel"];
    if (self) {
        /* init data sources */
        _errors = [[NSMutableArray alloc] init];
        _icons = [[NSMutableArray alloc] init];
    }

    return self;
}

- (void)windowDidLoad
{
    /* init strings */
    [self.window setTitle: _NS("Errors and Warnings")];
    [self.cleanupButton setTitle: _NS("Clean up")];

    if ([[NSApplication sharedApplication] userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft) {
        [self.errorTable moveColumn:0 toColumn:1];
    }
}

- (void)addError:(NSString *)title withMsg:(NSString *)message;
{
    /* format our string as desired */
    NSMutableAttributedString * ourError;
    ourError = [[NSMutableAttributedString alloc] initWithString:
                [NSString stringWithFormat:@"%@\n%@", title, message]
                                                      attributes:@{NSFontAttributeName : [NSFont systemFontOfSize:11]}];
    [ourError addAttribute: NSFontAttributeName
                     value: [NSFont boldSystemFontOfSize:11]
                     range: NSMakeRange(0, [title length])];
    [_errors addObject: ourError];

    [_icons addObject: [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kAlertStopIcon)]];

    [self.errorTable reloadData];
}

-(IBAction)cleanupTable:(id)sender
{
    [_errors removeAllObjects];
    [_icons removeAllObjects];
    [self.errorTable reloadData];
}

/*----------------------------------------------------------------------------
 * data source methods
 *---------------------------------------------------------------------------*/
- (NSInteger)numberOfRowsInTableView:(NSTableView *)theDataTable
{
    return [_errors count];
}

- (id)tableView:(NSTableView *)theDataTable objectValueForTableColumn:(NSTableColumn *)theTableColumn
            row: (NSInteger)row
{
    if ([[theTableColumn identifier] isEqualToString: @"error_msg"])
        return [_errors objectAtIndex:row];

    if ([[theTableColumn identifier] isEqualToString: @"icon"])
        return [_icons objectAtIndex:row];

    return @"";
}

@end
