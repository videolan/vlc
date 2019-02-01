//
//  VLCHUDTableView.m
//  BGHUDAppKit
//
//  Created by BinaryGod on 6/17/08.
//
//  Copyright (c) 2008, Tim Davis (BinaryMethod.com, binary.god@gmail.com)
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//
//		Redistributions of source code must retain the above copyright notice, this
//	list of conditions and the following disclaimer.
//
//		Redistributions in binary form must reproduce the above copyright notice,
//	this list of conditions and the following disclaimer in the documentation and/or
//	other materials provided with the distribution.
//
//		Neither the name of the BinaryMethod.com nor the names of its contributors
//	may be used to endorse or promote products derived from this software without
//	specific prior written permission.
//
//	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
//	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//	IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
//	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
//	OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//	POSSIBILITY OF SUCH DAMAGE.

#import "VLCHUDTableView.h"

#import "views/VLCHUDTableCornerView.h"

@interface NSTableView (private)
- (void)_sendDelegateWillDisplayCell:(id)cell forColumn:(id)column row:(NSInteger)row;
@end

@implementation VLCHUDTableView

#pragma mark Drawing Functions

- (instancetype)initWithCoder:(NSCoder *)decoder
{

    self = [super initWithCoder: decoder];

    if (self) {
        _tableBackgroundColor = [NSColor colorWithCalibratedRed:0 green:0 blue:0 alpha:0];
        _cellHighlightColor = [NSColor colorWithDeviceRed:0.549f green:0.561f blue:0.588f alpha:1];
        _cellEditingFillColor = [NSColor colorWithDeviceRed:0.141f green:0.141f blue:0.141f alpha:0.5f];
        _cellAlternatingRowColors = @[[NSColor colorWithCalibratedWhite:0.16f alpha:0.86f],
                                      [NSColor colorWithCalibratedWhite:0.15f alpha:0.8f]];
        _cellTextColor = [NSColor whiteColor];
        _cellSelectedTextColor = [NSColor blackColor];
        _strokeColor = [NSColor colorWithDeviceRed:0.749f green:0.761f blue:0.788f alpha:1.0f];
        _highlightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:0.5f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:0.5f]];
        _normalGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.5f]
                                                        endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.5f]];


        [self setBackgroundColor:_tableBackgroundColor];
        [self setFocusRingType:NSFocusRingTypeNone];
    }

    return self;
}

- (id)_alternatingRowBackgroundColors {
    return _cellAlternatingRowColors;
}

- (id)_highlightColorForCell:(id)cell {
    return _cellHighlightColor;
}

- (void)_sendDelegateWillDisplayCell:(id)cell forColumn:(id)column row:(NSInteger)row {

    [super _sendDelegateWillDisplayCell:cell forColumn:column row:row];

    [[self currentEditor] setBackgroundColor:_cellEditingFillColor];
    [[self currentEditor] setTextColor:_cellTextColor];

    if([[self selectedRowIndexes] containsIndex: row]) {

        if([cell respondsToSelector: @selector(setTextColor:)]) {
            [cell setTextColor:_cellSelectedTextColor];
        }
    } else {

        if ([cell respondsToSelector:@selector(setTextColor:)]) {
            [cell setTextColor:_cellTextColor];
        }
    }
}

- (void)awakeFromNib {
    [self setCornerView: [[VLCHUDTableCornerView alloc] init]];
}

@end
