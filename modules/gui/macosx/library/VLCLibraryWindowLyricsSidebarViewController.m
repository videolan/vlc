/*****************************************************************************
 * VLCLibraryWindowLyricsSidebarViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryWindowLyricsSidebarViewController.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"
#import "library/VLCInputItem.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayQueueController.h"

@interface VLCLibraryWindowLyricsSidebarViewController () <NSTableViewDataSource, NSTableViewDelegate>

@property (readwrite) NSArray<NSDictionary<NSString *, id> *> *lyricsEntries;
@property (readwrite) NSInteger currentLyricIndex;

@end

@implementation VLCLibraryWindowLyricsSidebarViewController

@synthesize counterLabel = _counterLabel;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow
                                nibName:@"VLCLibraryWindowLyricsView"];
    if (self) {
        _lyricsEntries = @[];
        _currentLyricIndex = -1;
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    self.tableView.headerView = nil;
    self.tableView.backgroundColor = [NSColor clearColor];
    self.tableView.selectionHighlightStyle = NSTableViewSelectionHighlightStyleNone;

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(playerMetadataChanged:)
                               name:VLCPlayerMetadataChangedForCurrentMedia
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerMetadataChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerTimeChanged:)
                               name:VLCPlayerTimeAndPositionChanged
                             object:nil];

    [self updateLyricsData];
}

- (void)playerMetadataChanged:(NSNotification *)notification
{
    [self updateLyricsData];
}

- (void)playerTimeChanged:(NSNotification *)notification
{
    [self updateCurrentLyric];
}

- (void)updateLyricsData
{
    VLCPlayerController * const playerController = VLCMain.sharedInstance.playQueueController.playerController;
    VLCInputItem * const item = playerController.currentMedia;

    NSString *syltData = nil;
    if (item) {
        syltData = [item extraMetaForKey:@"sylt-data"];
    }

    if (!syltData || syltData.length == 0) {
        self.lyricsEntries = @[];
        self.currentLyricIndex = -1;
        [self.tableView reloadData];
        return;
    }

    NSMutableArray * const entries = [NSMutableArray array];
    NSArray<NSString *> * const rawEntries = [syltData componentsSeparatedByString:@"\x1E"];
    for (NSString * const rawEntry in rawEntries) {
        if (rawEntry.length == 0) continue;
        NSArray<NSString *> * const parts = [rawEntry componentsSeparatedByString:@"\x1F"];
        if (parts.count >= 2) {
            NSDictionary<NSString *, id> * const entry = @{
                @"time": @([parts[0] longLongValue]),
                @"text": parts[1]
            };
            [entries addObject:entry];
        }
    }

    self.lyricsEntries = [entries copy];
    self.currentLyricIndex = -1;
    [self.tableView reloadData];

    [self updateCurrentLyric];
}

- (void)updateCurrentLyric
{
    if (self.lyricsEntries.count == 0) return;

    VLCPlayerController * const playerController = VLCMain.sharedInstance.playQueueController.playerController;
    const vlc_tick_t currentTime = playerController.time;
    const long long currentTimeMs = MS_FROM_VLC_TICK(currentTime);

    // 1. Check if we are still on the same lyric (most frequent case)
    if (self.currentLyricIndex >= 0 && self.currentLyricIndex < self.lyricsEntries.count) {
        const long long startTime = [self.lyricsEntries[self.currentLyricIndex][@"time"] longLongValue];
        const BOOL isLast = (self.currentLyricIndex == self.lyricsEntries.count - 1);
        const long long nextStartTime = isLast ? LLONG_MAX : [self.lyricsEntries[self.currentLyricIndex + 1][@"time"] longLongValue];

        if (currentTimeMs >= startTime && currentTimeMs < nextStartTime) {
            return; // No change needed
        }

        // 2. Check if it's simply the next one (normal linear playback transition)
        if (!isLast && currentTimeMs >= nextStartTime) {
            const NSInteger nextIndex = self.currentLyricIndex + 1;
            const BOOL isNextLast = (nextIndex == self.lyricsEntries.count - 1);
            const long long nextNextStartTime = isNextLast ? LLONG_MAX : [self.lyricsEntries[nextIndex + 1][@"time"] longLongValue];

            if (currentTimeMs < nextNextStartTime) {
                [self updateToLyricIndex:nextIndex];
                return;
            }
        }
    }

    // 3. Fallback to search (for seeks, jumps, or initial playback)
    NSInteger foundIndex = -1;
    for (NSInteger i = 0; i < self.lyricsEntries.count; i++) {
        if ([self.lyricsEntries[i][@"time"] longLongValue] <= currentTimeMs) {
            foundIndex = i;
        } else {
            break;
        }
    }

    if (foundIndex != self.currentLyricIndex) {
        [self updateToLyricIndex:foundIndex];
    }
}

- (void)updateToLyricIndex:(NSInteger)newIndex
{
    NSMutableIndexSet * const indicesToUpdate = [NSMutableIndexSet indexSet];
    if (self.currentLyricIndex != -1 && self.currentLyricIndex < self.lyricsEntries.count) {
        [indicesToUpdate addIndex:self.currentLyricIndex];
    }

    self.currentLyricIndex = newIndex;

    if (self.currentLyricIndex != -1 && self.currentLyricIndex < self.lyricsEntries.count) {
        [indicesToUpdate addIndex:self.currentLyricIndex];
    }

    if (indicesToUpdate.count > 0) {
        [self.tableView noteHeightOfRowsWithIndexesChanged:indicesToUpdate];
        [self.tableView reloadDataForRowIndexes:indicesToUpdate
                                  columnIndexes:[NSIndexSet indexSetWithIndex:0]];
    }

    if (self.currentLyricIndex != -1) {
        [self.tableView scrollRowToVisible:self.currentLyricIndex];
    }
}

#pragma mark - Table View Data Source

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return self.lyricsEntries.count;
}

#pragma mark - Table View Delegate

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    NSTableCellView * const cellView = [tableView makeViewWithIdentifier:@"LyricCellIdentifier" owner:self];
    if (row >= self.lyricsEntries.count) return cellView;

    NSString * const text = self.lyricsEntries[row][@"text"];
    cellView.textField.stringValue = text;

    if (row == self.currentLyricIndex) {
        cellView.textField.font = [NSFont boldSystemFontOfSize:22.0];
        cellView.textField.textColor = [NSColor labelColor];
    } else {
        cellView.textField.font = [NSFont systemFontOfSize:16.0];
        cellView.textField.textColor = [NSColor secondaryLabelColor];
    }

    return cellView;
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    return (row == self.currentLyricIndex) ? 60.0 : 40.0;
}

- (IBAction)tableViewAction:(id)sender
{
    const NSInteger clickedRow = self.tableView.clickedRow;
    if (clickedRow < 0 || clickedRow >= self.lyricsEntries.count) {
        return;
    }

    const long long lyricTimeMs = [self.lyricsEntries[clickedRow][@"time"] longLongValue];
    const vlc_tick_t vlcTime = VLC_TICK_FROM_MS(lyricTimeMs);

    VLCPlayerController * const playerController = VLCMain.sharedInstance.playQueueController.playerController;
    [playerController setTimePrecise:vlcTime];
}

- (NSString *)title
{
    return _NS("Lyrics");
}

- (BOOL)supportsItemCount
{
    return NO;
}

@end
