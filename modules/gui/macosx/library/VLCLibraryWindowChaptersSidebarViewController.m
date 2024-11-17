/*****************************************************************************
 * VLCLibraryWindowChaptersSidebarViewController.m: MacOS X interface module
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

#import "VLCLibraryWindowChaptersSidebarViewController.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryDataTypes.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayerChapter.h"
#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayQueueController.h"

@interface VLCLibraryWindowChaptersSidebarViewController ()

@property (readwrite) NSUInteger internalItemCount;

@end

@implementation VLCLibraryWindowChaptersSidebarViewController

@synthesize counterLabel = _counterLabel;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    return [super initWithLibraryWindow:libraryWindow
                                nibName:@"VLCLibraryWindowChaptersView"];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _chaptersArrayController = [[NSArrayController alloc] init];

    [self.tableView deselectAll:self];
    [self.tableView bind:NSContentBinding
                toObject:self.chaptersArrayController
             withKeyPath:@"arrangedObjects" 
                 options:nil];
    [self.tableView bind:NSSelectionIndexesBinding 
                toObject:self.chaptersArrayController
             withKeyPath:@"selectionIndexes"
                 options:nil];
    [self.tableView bind:NSSortDescriptorsBinding 
                toObject:self.chaptersArrayController
             withKeyPath:@"sortDescriptors"
                 options:nil];

    [self updateChapterList];
    [self updateSelectedChapter];
    
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(titleListChanged:)
                               name:VLCPlayerTitleListChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(chapterSelectionChanged:)
                               name:VLCPlayerChapterSelectionChanged
                             object:nil];
}

- (NSString *)title
{
    return _NS("Chapters");
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
    [self updateChapterList];
}

- (void)updateChapterList
{
    VLCPlayerController * const playerController =
        VLCMain.sharedInstance.playQueueController.playerController;

    const struct vlc_player_title * const title = playerController.selectedTitle;
    if (title == NULL) {
        self.chaptersArrayController.content = @[];
        return;
    }
    const struct vlc_player_chapter * const pp_chapters = title->chapters;
    const size_t chapterCount = title->chapter_count;

    self.internalItemCount = chapterCount;
    NSMutableArray * const chapters = [NSMutableArray arrayWithCapacity:chapterCount];
    for (size_t i = 0; i < chapterCount; i++) {
        struct vlc_player_chapter p_chapter = pp_chapters[i];
        VLCPlayerChapter * const chapter = [[VLCPlayerChapter alloc] initWithChapter:&p_chapter];
        [chapters addObject:chapter];
    }
    self.chaptersArrayController.content = chapters.copy;
    self.counterLabel.stringValue = [NSString stringWithFormat:@"%lu", chapterCount];
}

- (IBAction)tableViewAction:(id)sender
{
    VLCPlayerChapter * const selectedChapter =
        self.chaptersArrayController.selectedObjects.firstObject;
    if (selectedChapter == nil) {
        return;
    }

    [VLCMain.sharedInstance.playQueueController.playerController setTimeFast:selectedChapter.time];
}

- (void)chapterSelectionChanged:(NSNotification *)notification
{
    [self updateSelectedChapter];
}

- (void)updateSelectedChapter
{
    const NSUInteger selectedChapterIndex =
        VLCMain.sharedInstance.playQueueController.playerController.selectedChapterIndex;
    NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:selectedChapterIndex];
    [self.tableView selectRowIndexes:indexSet byExtendingSelection:NO];
}

# pragma mark - NSTableView delegation

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    if ([tableColumn.identifier isEqualToString:@"VLCLibraryWindowChaptersTableViewNameColumnIdentifier"]) {
        NSTableCellView * const cellView =
            [tableView makeViewWithIdentifier:@"VLCLibraryWindowChaptersTableViewNameCellIdentifier"
                                        owner:self];
        [cellView.textField bind:NSValueBinding
                            toObject:cellView
                         withKeyPath:@"objectValue.name"
                             options:nil];
        return cellView;
    } else if ([tableColumn.identifier isEqualToString:@"VLCLibraryWindowChaptersTableViewTimeColumnIdentifier"]) {
        NSTableCellView * const cellView =
            [tableView makeViewWithIdentifier:@"VLCLibraryWindowChaptersTableViewTimeCellIdentifier"
                                        owner:self];
        [cellView.textField bind:NSValueBinding
                            toObject:cellView
                         withKeyPath:@"objectValue.timeString"
                             options:nil];
        return cellView;
    }

    NSAssert(NO, @"Provided cell view for chapters table view should be valid!");
    return nil;
}

@end
