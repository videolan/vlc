/*****************************************************************************
 * NSTableCellView+VLCAdditions.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "NSTableCellView+VLCAdditions.h"

#import "extensions/NSTextField+VLCAdditions.h"

@implementation NSTableCellView (VLCAdditions)

+ (instancetype)tableCellViewWithIdentifier:(NSString *)identifier showingString:(NSString *)string
{
    NSTableCellView * const cellView = [[NSTableCellView alloc] initWithFrame:NSZeroRect];
    cellView.identifier = identifier;

    NSTextField * const textField = [NSTextField defaultLabelWithString:string];
    textField.translatesAutoresizingMaskIntoConstraints = NO;
    cellView.textField = textField;
    [cellView addSubview:textField];
    [cellView.centerYAnchor constraintEqualToAnchor:textField.centerYAnchor].active = YES;
    [cellView.leadingAnchor constraintEqualToAnchor:textField.leadingAnchor].active = YES;
    [cellView.trailingAnchor constraintEqualToAnchor:textField.trailingAnchor].active = YES;

    return cellView;
}

@end
