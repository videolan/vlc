/*****************************************************************************
 * MainWindowTitle.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 Felix Paul Kühne
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

#import <Cocoa/Cocoa.h>
#import "misc.h"

/*****************************************************************************
 * VLCMainWindowTitleView
 *****************************************************************************/

@class VLCCustomWindowCloseButton;
@class VLCCustomWindowMinimizeButton;
@class VLCCustomWindowZoomButton;
@class VLCCustomWindowFullscreenButton;
@class VLCWindowTitleTextField;

@interface VLCMainWindowTitleView : VLCThreePartImageView

@property (readwrite, strong) IBOutlet VLCCustomWindowCloseButton *redButton;
@property (readwrite, strong) IBOutlet VLCCustomWindowMinimizeButton *yellowButton;
@property (readwrite, strong) IBOutlet VLCCustomWindowZoomButton *greenButton;
@property (readwrite, strong) IBOutlet VLCCustomWindowFullscreenButton *fullscreenButton;
@property (readwrite, strong) IBOutlet VLCWindowTitleTextField *titleLabel;

@property (readonly) NSButton * closeButton;
@property (readonly) NSButton * minimizeButton;
@property (readonly) NSButton * zoomButton;

- (void)informModifierPressed:(BOOL)b_is_altkey;
- (void)loadButtonIcons;
- (IBAction)buttonAction:(id)sender;
- (void)setWindowTitle:(NSString *)title;
- (void)setWindowButtonOver:(BOOL)b_value;
- (void)setWindowFullscreenButtonOver:(BOOL)b_value;

@end

@interface VLCWindowButtonCell : NSButtonCell

@end

@interface VLCCustomWindowButtonPrototype : NSButton

- (NSArray*)extendedAccessibilityAttributeNames: (NSArray*)theAttributeNames;
- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName;
- (NSNumber*)extendedAccessibilityIsAttributeSettable: (NSString*)theAttributeName;

@end

@interface VLCCustomWindowCloseButton : VLCCustomWindowButtonPrototype

@end


@interface VLCCustomWindowMinimizeButton : VLCCustomWindowButtonPrototype

@end


@interface VLCCustomWindowZoomButton : VLCCustomWindowButtonPrototype

@end

@interface VLCCustomWindowFullscreenButton : VLCCustomWindowButtonPrototype

@end

@interface VLCWindowTitleTextField : NSTextField

@end
