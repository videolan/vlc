/*****************************************************************************
 * VLCLibraryHomeViewBrowseCarouselItemView.m: MacOS X interface module
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

#import "VLCLibraryHomeViewBrowseCarouselItemView.h"

#import "VLCLibraryHomeViewBrowseCarouselItemCell.h"
#import "extensions/NSString+Helpers.h"
#import "views/VLCCarouselView.h"
#import "views/VLCUIUnits.h"

@interface VLCLibraryHomeViewBrowseCarouselItemView ()

@property (nonatomic, readwrite, copy) NSString *subtitle;

@end

@implementation VLCLibraryHomeViewBrowseCarouselItemView

+ (Class)cellClass
{
    return VLCLibraryHomeViewBrowseCarouselItemCell.class;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setButtonType:NSButtonTypeMomentaryPushIn];
        self.bordered = NO;
        self.focusRingType = NSFocusRingTypeExterior;
        self.mrl = @"";
    }
    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSSize)intrinsicContentSize
{
    return VLCUIUnits.libraryCompactCardSize;
}

- (void)mouseDown:(NSEvent *)event
{
    if (!self.isEnabled) {
        return;
    }
    VLCCarouselView * const carouselView = self.carouselView;
    if (carouselView == nil) {
        [super mouseDown:event];
        return;
    }
    [self highlight:YES];
    [carouselView mouseDown:event];
    [self highlight:NO];
}

- (void)setSubtitle:(NSString *)subtitle
{
    _subtitle = [subtitle copy];
    VLCLibraryHomeViewBrowseCarouselItemCell * const cell =
        (VLCLibraryHomeViewBrowseCarouselItemCell *)self.cell;
    cell.subtitle = _subtitle;
}

- (void)setMrl:(NSString *)mrl
{
    _mrl = [mrl copy];
    VLCLibraryHomeViewBrowseCarouselItemCell * const cell =
        (VLCLibraryHomeViewBrowseCarouselItemCell *)self.cell;

    NSURL * const url = [mrl hasPrefix:@"/"]
        ? [NSURL fileURLWithPath:mrl]
        : [NSURL URLWithString:mrl];
    NSString * const scheme = url.scheme.lowercaseString;
    NSString *symbolName = @"folder";
    NSImageName fallbackImageName = NSImageNameFolder;
    cell.iconTint = NSColor.secondaryLabelColor;
    self.subtitle = _NS("Folder");
    self.title = url.lastPathComponent.length > 0
        ? url.lastPathComponent
        : (url.host.length > 0 ? url.host : mrl);

    if (url.isFileURL) {
        NSDictionary<NSURLResourceKey, id> * const resourceValues =
            [url resourceValuesForKeys:@[NSURLLocalizedNameKey,
                                         NSURLIsVolumeKey,
                                         NSURLIsDirectoryKey,
                                         NSURLIsUbiquitousItemKey,
                                         NSURLVolumeIsLocalKey,
                                         NSURLVolumeIsInternalKey,
                                         NSURLVolumeLocalizedNameKey]
                                 error:nil];
        NSString * const localizedName = resourceValues[NSURLLocalizedNameKey];
        if (localizedName.length > 0) {
            self.title = localizedName;
        }
        if ([resourceValues[NSURLIsUbiquitousItemKey] boolValue]) {
            self.subtitle = _NS("Cloud");
            symbolName = @"cloud";
            cell.iconTint = NSColor.systemBlueColor;
        } else if ([resourceValues[NSURLIsVolumeKey] boolValue]) {
            NSString * const volumeName = resourceValues[NSURLVolumeLocalizedNameKey];
            if (volumeName.length > 0) {
                self.title = volumeName;
            }
            if (resourceValues[NSURLVolumeIsLocalKey] &&
                ![resourceValues[NSURLVolumeIsLocalKey] boolValue]) {
                self.subtitle = _NS("Network location");
                symbolName = @"server.rack";
                fallbackImageName = NSImageNameNetwork;
                cell.iconTint = NSColor.systemGreenColor;
            } else if (resourceValues[NSURLVolumeIsInternalKey] &&
                       ![resourceValues[NSURLVolumeIsInternalKey] boolValue]) {
                self.subtitle = _NS("External drive");
                symbolName = @"externaldrive";
                fallbackImageName = NSImageNameComputer;
                cell.iconTint = NSColor.systemBlueColor;
            } else {
                self.subtitle = _NS("Local disk");
                symbolName = @"internaldrive";
                fallbackImageName = NSImageNameComputer;
            }
        } else if (resourceValues[NSURLIsDirectoryKey] &&
                   ![resourceValues[NSURLIsDirectoryKey] boolValue]) {
            self.subtitle = _NS("File");
            symbolName = @"doc";
            fallbackImageName = NSImageNameMultipleDocuments;
        }
    } else if ([scheme isEqualToString:@"dvd"] ||
               [scheme isEqualToString:@"dvdnav"] ||
               [scheme isEqualToString:@"cdda"] ||
               [scheme isEqualToString:@"vcd"] ||
               [scheme isEqualToString:@"bluray"]) {
        self.subtitle = [scheme isEqualToString:@"cdda"]
            ? _NS("Audio CD")
            : ([scheme isEqualToString:@"dvd"] || [scheme isEqualToString:@"dvdnav"]
                ? _NS("DVD")
                : _NS("Disc"));
        symbolName = @"opticaldisc";
        fallbackImageName = NSImageNameFolderBurnable;
        cell.iconTint = NSColor.systemOrangeColor;
    } else if (url.scheme.length > 0) {
        self.subtitle = _NS("Network location");
        symbolName = @"server.rack";
        fallbackImageName = NSImageNameNetwork;
        cell.iconTint = NSColor.systemGreenColor;
    }

    NSImage *icon = nil;
    if (@available(macOS 11.0, *)) {
        icon = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:nil];
    }
    self.image = icon ?: [NSImage imageNamed:fallbackImageName];
    self.toolTip = url.isFileURL ? url.path : mrl;
    self.accessibilityLabel = self.title;
    self.accessibilityHelp = [NSString stringWithFormat:@"%@ — %@", self.subtitle, self.toolTip];
    self.needsDisplay = YES;
}

@end
