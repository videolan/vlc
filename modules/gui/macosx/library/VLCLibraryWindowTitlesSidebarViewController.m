/*****************************************************************************
 * VLCLibraryWindowTitlesSidebarViewController.m: MacOS X interface module
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

#import "VLCLibraryWindowTitlesSidebarViewController.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryDataTypes.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayerTitle.h"
#import "playqueue/VLCPlayQueueController.h"

@interface VLCLibraryWindowTitlesSidebarViewController ()

@property (readwrite) NSUInteger internalItemCount;

@end

@implementation VLCLibraryWindowTitlesSidebarViewController

@synthesize counterLabel = _counterLabel;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    return [super initWithLibraryWindow:libraryWindow
                                nibName:@"VLCLibraryWindowTitlesView"];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _titlesArrayController = [[NSArrayController alloc] init];

    [self.tableView deselectAll:self];
    [self.tableView bind:NSContentBinding
                toObject:self.titlesArrayController
             withKeyPath:@"arrangedObjects"
                 options:nil];
    [self.tableView bind:NSSelectionIndexesBinding
                toObject:self.titlesArrayController
             withKeyPath:@"selectionIndexes"
                 options:nil];
    [self.tableView bind:NSSortDescriptorsBinding
                toObject:self.titlesArrayController
             withKeyPath:@"sortDescriptors"
                 options:nil];

    [self updateTitleList];
    [self updateTitleSelection];

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(titleListChanged:)
                               name:VLCPlayerTitleListChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(titleSelectionChanged:)
                               name:VLCPlayerTitleSelectionChanged
                             object:nil];
}

- (NSString *)title
{
    return _NS("Titles");
}

- (BOOL)supportsItemCount
{
    return YES;
}

- (void)setCounterLabel:(NSTextField *)counterLabel
{
    _counterLabel = counterLabel;
    self.counterLabel.stringValue = [NSString stringWithFormat:@"%lu", self.internalItemCount];
}

- (void)titleListChanged:(NSNotification *)notification
{
    [self updateTitleList];
}

- (void)updateTitleList
{
    VLCPlayerController * const playerController =
        VLCMain.sharedInstance.playQueueController.playerController;
    const size_t titleCount = playerController.numberOfTitlesOfCurrentMedia;
    self.internalItemCount = titleCount;
    self.titlesArrayController.content = playerController.titlesOfCurrentMedia;
    self.counterLabel.stringValue = [NSString stringWithFormat:@"%lu", titleCount];
}

- (IBAction)tableViewAction:(id)sender
{
    VLCPlayerTitle * const selectedTitle =
        self.titlesArrayController.selectedObjects.firstObject;
    if (selectedTitle == nil) {
        return;
    }

    [VLCMain.sharedInstance.playQueueController.playerController setSelectedTitleIndex:selectedTitle.index];
}

- (void)titleSelectionChanged:(NSNotification *)notification
{
    [self updateTitleSelection];
}

- (void)updateTitleSelection
{
    VLCPlayerController * const playerController =
        VLCMain.sharedInstance.playQueueController.playerController;
    const NSUInteger selectedTitleIndex = playerController.selectedTitleIndex;
    NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:selectedTitleIndex];
    [self.tableView selectRowIndexes:indexSet byExtendingSelection:NO];
}

# pragma mark - NSTableView delegation

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    if ([tableColumn.identifier isEqualToString:@"VLCLibraryWindowTitlesTableViewNameColumnIdentifier"]) {
        NSTableCellView * const cellView =
            [tableView makeViewWithIdentifier:@"VLCLibraryWindowTitlesTableViewNameCellIdentifier"
                                        owner:self];
        [cellView.textField bind:NSValueBinding
                            toObject:cellView
                         withKeyPath:@"objectValue.name"
                             options:nil];
        return cellView;
    } else if ([tableColumn.identifier isEqualToString:@"VLCLibraryWindowTitlesTableViewLengthColumnIdentifier"]) {
        NSTableCellView * const cellView =
            [tableView makeViewWithIdentifier:@"VLCLibraryWindowTitlesTableViewLengthCellIdentifier"
                                        owner:self];
        [cellView.textField bind:NSValueBinding
                            toObject:cellView
                         withKeyPath:@"objectValue.lengthString"
                             options:nil];
        return cellView;
    }

    NSAssert(NO, @"Provided cell view for titles table view should be valid!");
    return nil;
}

@end
