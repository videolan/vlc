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
{
    NSUInteger _resourceValuesRequest;
}

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

- (NSString *)subtitle
{
    VLCLibraryHomeViewBrowseCarouselItemCell * const cell =
        (VLCLibraryHomeViewBrowseCarouselItemCell *)self.cell;
    return cell.subtitle;
}

- (void)setMrl:(NSString *)mrl
{
    _mrl = [mrl copy];
    const NSUInteger request = ++_resourceValuesRequest;
    NSURL * const url = [mrl hasPrefix:@"/"]
        ? [NSURL fileURLWithPath:mrl isDirectory:[mrl hasSuffix:@"/"]]
        : [NSURL URLWithString:mrl];
    [self updateContentWithURL:url resourceValues:nil];
    if (!url.isFileURL) {
        return;
    }

    __weak typeof(self) weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        NSDictionary<NSURLResourceKey, id> * const resourceValues =
            [url resourceValuesForKeys:@[NSURLLocalizedNameKey,
                                         NSURLIsVolumeKey,
                                         NSURLIsDirectoryKey,
                                         NSURLIsUbiquitousItemKey,
                                         NSURLVolumeIsLocalKey,
                                         NSURLVolumeIsInternalKey,
                                         NSURLVolumeLocalizedNameKey]
                                 error:nil];
        if (resourceValues == nil) {
            return;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            typeof(self) strongSelf = weakSelf;
            if (strongSelf == nil || strongSelf->_resourceValuesRequest != request) {
                return;
            }
            [strongSelf updateContentWithURL:url resourceValues:resourceValues];
        });
    });
}

- (void)updateContentWithURL:(NSURL *)url
              resourceValues:(NSDictionary<NSURLResourceKey, id> *)resourceValues
{
    VLCLibraryHomeViewBrowseCarouselItemCell * const cell =
        (VLCLibraryHomeViewBrowseCarouselItemCell *)self.cell;
    NSString * const scheme = url.scheme.lowercaseString;
    NSString *symbolName = @"folder";
    NSImageName fallbackImageName = NSImageNameFolder;
    cell.iconTint = NSColor.secondaryLabelColor;
    cell.subtitle = _NS("Folder");
    self.title = url.lastPathComponent.length > 0
        ? url.lastPathComponent
        : (url.host.length > 0 ? url.host : self.mrl);

    if (url.isFileURL) {
        NSString * const localizedName = resourceValues[NSURLLocalizedNameKey];
        if (localizedName.length > 0) {
            self.title = localizedName;
        }
        if ([resourceValues[NSURLIsUbiquitousItemKey] boolValue]) {
            cell.subtitle = _NS("Cloud");
            symbolName = @"cloud";
            cell.iconTint = NSColor.systemBlueColor;
        } else if ([resourceValues[NSURLIsVolumeKey] boolValue]) {
            NSString * const volumeName = resourceValues[NSURLVolumeLocalizedNameKey];
            if (volumeName.length > 0) {
                self.title = volumeName;
            }
            if (resourceValues[NSURLVolumeIsLocalKey] &&
                ![resourceValues[NSURLVolumeIsLocalKey] boolValue]) {
                cell.subtitle = _NS("Network location");
                symbolName = @"server.rack";
                fallbackImageName = NSImageNameNetwork;
                cell.iconTint = NSColor.systemGreenColor;
            } else if (resourceValues[NSURLVolumeIsInternalKey] &&
                       ![resourceValues[NSURLVolumeIsInternalKey] boolValue]) {
                cell.subtitle = _NS("External drive");
                symbolName = @"externaldrive";
                fallbackImageName = NSImageNameComputer;
                cell.iconTint = NSColor.systemBlueColor;
            } else {
                cell.subtitle = _NS("Local disk");
                symbolName = @"internaldrive";
                fallbackImageName = NSImageNameComputer;
            }
        } else if (resourceValues[NSURLIsDirectoryKey] &&
                   ![resourceValues[NSURLIsDirectoryKey] boolValue]) {
            cell.subtitle = _NS("File");
            symbolName = @"doc";
            fallbackImageName = NSImageNameMultipleDocuments;
        }
    } else if ([scheme isEqualToString:@"dvd"] ||
               [scheme isEqualToString:@"dvdnav"] ||
               [scheme isEqualToString:@"cdda"] ||
               [scheme isEqualToString:@"vcd"] ||
               [scheme isEqualToString:@"bluray"]) {
        cell.subtitle = [scheme isEqualToString:@"cdda"]
            ? _NS("Audio CD")
            : ([scheme isEqualToString:@"dvd"] || [scheme isEqualToString:@"dvdnav"]
                ? _NS("DVD")
                : _NS("Disc"));
        symbolName = @"opticaldisc";
        fallbackImageName = NSImageNameFolderBurnable;
        cell.iconTint = NSColor.systemOrangeColor;
    } else if (url.scheme.length > 0) {
        cell.subtitle = _NS("Network location");
        symbolName = @"server.rack";
        fallbackImageName = NSImageNameNetwork;
        cell.iconTint = NSColor.systemGreenColor;
    }

    NSImage *icon = nil;
    if (@available(macOS 11.0, *)) {
        icon = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:nil];
    }
    self.image = icon ?: [NSImage imageNamed:fallbackImageName];
    self.toolTip = url.isFileURL ? url.path : self.mrl;
    self.accessibilityLabel = self.title;
    self.accessibilityHelp = [NSString stringWithFormat:@"%@ — %@", self.subtitle, self.toolTip];
    self.needsDisplay = YES;
}

@end
