/*****************************************************************************
* VLCCustomWindowButton.m: MacOS X interface module
*****************************************************************************
* Copyright (C) 2011-2019 VLC authors and VideoLAN
*
* Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
*          David Fuhrmann <dfuhrmann at videolan dot org>
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

#import "VLCCustomWindowButton.h"

@interface VLCCustomWindowButtonPrototype()

@property (readwrite, retain) NSImage *buttonImage;
@property (readwrite, retain) NSImage *hoverButtonImage;
@property (readwrite, retain) NSImage *onClickButtonImage;

- (void)loadButtonIcons;
- (NSArray *)extendedAccessibilityAttributeNames:(NSArray *)theAttributeNames;
- (id)extendedAccessibilityAttributeValue:(NSString *)theAttributeName;
- (NSNumber *)extendedAccessibilityIsAttributeSettable:(NSString *)theAttributeName;

@end

@implementation VLCWindowButtonCell

- (NSArray *)accessibilityAttributeNames
{
    NSArray *theAttributeNames = [super accessibilityAttributeNames];
    id theControlView = [self controlView];
    return ([theControlView respondsToSelector: @selector(extendedAccessibilityAttributeNames:)] ? [theControlView extendedAccessibilityAttributeNames: theAttributeNames] : theAttributeNames); // ask the cell's control view (i.e., the button) for additional attribute values
}

- (id)accessibilityAttributeValue:(NSString *)theAttributeName
{
    id theControlView = [self controlView];
    if ([theControlView respondsToSelector: @selector(extendedAccessibilityAttributeValue:)]) {
        id theValue = [theControlView extendedAccessibilityAttributeValue: theAttributeName];
        if (theValue) {
            return theValue; // if this is an extended attribute value we added, return that -- otherwise, fall back to super's implementation
        }
    }
    return [super accessibilityAttributeValue: theAttributeName];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)theAttributeName
{
    id theControlView = [self controlView];
    if ([theControlView respondsToSelector: @selector(extendedAccessibilityIsAttributeSettable:)]) {
        NSNumber *theValue = [theControlView extendedAccessibilityIsAttributeSettable: theAttributeName];
        if (theValue)
            return [theValue boolValue]; // same basic strategy we use in -accessibilityAttributeValue:
    }
    return [super accessibilityIsAttributeSettable: theAttributeName];
}

@end

@implementation VLCCustomWindowButtonPrototype

+ (Class)cellClass
{
    return [VLCWindowButtonCell class];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self commonInitializer];
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self commonInitializer];
    }
    return self;
}

- (void)commonInitializer
{
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(controlTintChanged:)
                                                 name:NSControlTintDidChangeNotification
                                               object:nil];
    [self loadButtonIcons];
    [self setTarget:self];
    [self setAction:@selector(performDefaultButtonAction:)];
}

- (NSImage *)getButtonImage:(NSString *)imageName
{
    if ([NSColor currentControlTint] != NSBlueControlTint) {
        imageName = [imageName stringByAppendingString:@"-graphite"];
    }

    return [NSImage imageNamed:imageName];
}

- (void)controlTintChanged:(NSNotification *)notification
{
    [self loadButtonIcons];
    [self setNeedsDisplay];
}

- (void)loadButtonIcons
{
    [self setImage:self.buttonImage];
    [self setAlternateImage:self.hoverButtonImage];
}

- (void)mouseEntered:(NSEvent *)event
{
    [self setImage:self.hoverButtonImage];
    [super mouseEntered:event];
}

- (void)mouseExited:(NSEvent *)event
{
    [self setImage:self.buttonImage];
    [super mouseExited:event];
}

- (void)performDefaultButtonAction:(id)sender
{
}

- (NSArray *)extendedAccessibilityAttributeNames:(NSArray *)theAttributeNames {
    return ([theAttributeNames containsObject: NSAccessibilitySubroleAttribute] ? theAttributeNames : [theAttributeNames arrayByAddingObject: NSAccessibilitySubroleAttribute]); // run-of-the-mill button cells don't usually have a Subrole attribute, so we add that attribute
}

- (id)extendedAccessibilityAttributeValue:(NSString *)theAttributeName {
    return nil;
}

- (NSNumber *)extendedAccessibilityIsAttributeSettable:(NSString *)theAttributeName {
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? [NSNumber numberWithBool:NO] : nil); // make the Subrole attribute we added non-settable
}

- (void)accessibilityPerformAction:(NSString *)theActionName {
    if ([theActionName isEqualToString: NSAccessibilityPressAction]) {
        if ([self isEnabled])
            [self performClick: nil];
    } else
        [super accessibilityPerformAction: theActionName];
}

@end

@implementation VLCCustomWindowCloseButton

- (id)extendedAccessibilityAttributeValue:(NSString *)theAttributeName
{
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityCloseButtonAttribute : nil);
}

- (void)loadButtonIcons
{
    self.buttonImage = [self getButtonImage:@"window-close"];
    self.hoverButtonImage = [self getButtonImage:@"window-close-over"];
    self.onClickButtonImage = [self getButtonImage:@"window-close-on"];

    [super loadButtonIcons];
}

- (void)performDefaultButtonAction:(id)sender
{
    if (self.window.styleMask & NSWindowStyleMaskClosable) {
        [[self window] performClose:sender];
    } else {
        [[self window] close];
    }
}

@end

@implementation VLCCustomWindowMinimizeButton

- (id)extendedAccessibilityAttributeValue:(NSString *)theAttributeName
{
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityMinimizeButtonAttribute : nil);
}

- (void)loadButtonIcons
{
    self.buttonImage = [self getButtonImage:@"window-minimize"];
    self.hoverButtonImage = [self getButtonImage:@"window-minimize-over"];
    self.onClickButtonImage = [self getButtonImage:@"window-minimize-on"];

    [super loadButtonIcons];
}

- (void)performDefaultButtonAction:(id)sender
{
    [[self window] miniaturize: sender];
}

@end

@implementation VLCCustomWindowZoomButton

- (id)extendedAccessibilityAttributeValue:(NSString *)theAttributeName
{
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityZoomButtonAttribute : nil);
}

- (void)loadButtonIcons
{
    self.buttonImage = [self getButtonImage:@"window-zoom"];
    self.hoverButtonImage = [self getButtonImage:@"window-zoom-over"];
    self.onClickButtonImage = [self getButtonImage:@"window-zoom-on"];

    [super loadButtonIcons];
}

- (void)performDefaultButtonAction:(id)sender
{
    [[self window] performZoom: sender];
}

@end

@implementation VLCCustomWindowFullscreenButton

- (id)extendedAccessibilityAttributeValue:(NSString *)theAttributeName
{
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityFullScreenButtonAttribute : nil);
}

- (void)loadButtonIcons
{
    self.buttonImage = [self getButtonImage:@"window-fullscreen"];
    self.hoverButtonImage = [self getButtonImage:@"window-fullscreen-over"];
    self.onClickButtonImage = [self getButtonImage:@"window-fullscreen-on"];

    [super loadButtonIcons];
}

- (void)performDefaultButtonAction:(id)sender
{
    [[self window] toggleFullScreen:self];
}

@end
