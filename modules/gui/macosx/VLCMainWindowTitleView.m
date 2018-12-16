/*****************************************************************************
 * MainWindowTitle.m: MacOS X interface module
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

#import <vlc_common.h>
#import "VLCMain.h"
#import "VLCMainWindowTitleView.h"
#import "VLCCoreInteraction.h"
#import "CompatibilityFixes.h"
#import <SystemConfiguration/SystemConfiguration.h> // for the revealInFinder clone

/*****************************************************************************
 * VLCMainWindowTitleView
 *
 * this is our title bar, which can do anything a title should do
 * it relies on the VLCWindowButtonCell to display the correct traffic light
 * states, since we can't capture the mouse-moved events here correctly
 *****************************************************************************/

@interface VLCMainWindowTitleView()
{
    NSImage *_redImage;
    NSImage *_redHoverImage;
    NSImage *_redOnClickImage;
    NSImage * _yellowImage;
    NSImage * _yellowHoverImage;
    NSImage * _yellowOnClickImage;
    NSImage * _greenImage;
    NSImage * _greenHoverImage;
    NSImage * _greenOnClickImage;
    // yosemite fullscreen images
    NSImage * _fullscreenImage;
    NSImage * _fullscreenHoverImage;
    NSImage * _fullscreenOnClickImage;
    // old native fullscreen images
    NSImage * _oldFullscreenImage;
    NSImage * _oldFullscreenHoverImage;
    NSImage * _oldFullscreenOnClickImage;

    NSDictionary * _windowTitleAttributesDictionary;

    BOOL b_nativeFullscreenMode;

    // state to determine correct image for green bubble
    BOOL b_alt_pressed;
    BOOL b_mouse_over;
}
@end

@implementation VLCMainWindowTitleView

- (id)init
{
    self = [super init];

    if (self) {
        _windowTitleAttributesDictionary = [NSDictionary dictionaryWithObjectsAndKeys: [NSColor whiteColor], NSForegroundColorAttributeName, [NSFont titleBarFontOfSize:12.0], NSFontAttributeName, nil];
    }

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
}

- (void)awakeFromNib
{
    b_nativeFullscreenMode = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");

    if (!b_nativeFullscreenMode || OSX_YOSEMITE_AND_HIGHER) {
        [_fullscreenButton setHidden: YES];
    }

    [self setAutoresizesSubviews: YES];
    [self setImagesLeft:imageFromRes(@"topbar-dark-left") middle: imageFromRes(@"topbar-dark-center-fill") right:imageFromRes(@"topbar-dark-right")];

    [self loadButtonIcons];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(controlTintChanged:) name: NSControlTintDidChangeNotification object: nil];
}

- (void)controlTintChanged:(NSNotification *)notification
{
    [self loadButtonIcons];

    [_redButton setNeedsDisplay];
    [_yellowButton setNeedsDisplay];
    [_greenButton setNeedsDisplay];
}

- (void)informModifierPressed:(BOOL)b_is_altkey;
{
    BOOL b_state_changed = b_alt_pressed != b_is_altkey;

    b_alt_pressed = b_is_altkey;

    if (b_state_changed) {
        [self updateGreenButton];
    }
}

- (NSImage *)getButtonImage:(NSString *)o_id
{
    NSString *o_name = @"";
    if (OSX_YOSEMITE_AND_HIGHER) {
        o_name = @"yosemite-";
    } else { // OSX_LION_AND_HIGHER, OSX_MOUNTAIN_LION_AND_HIGHER, OSX_MAVERICKS_AND_HIGHER
        o_name = @"lion-";
    }

    o_name = [o_name stringByAppendingString:o_id];

    if ([NSColor currentControlTint] != NSBlueControlTint) {
        o_name = [o_name stringByAppendingString:@"-graphite"];
    }

    return [NSImage imageNamed:o_name];
}

- (void)loadButtonIcons
{
    _redImage = [self getButtonImage:@"window-close"];
    _redHoverImage = [self getButtonImage:@"window-close-over"];
    _redOnClickImage = [self getButtonImage:@"window-close-on"];
    _yellowImage = [self getButtonImage:@"window-minimize"];
    _yellowHoverImage = [self getButtonImage:@"window-minimize-over"];
    _yellowOnClickImage = [self getButtonImage:@"window-minimize-on"];
    _greenImage = [self getButtonImage:@"window-zoom"];
    _greenHoverImage = [self getButtonImage:@"window-zoom-over"];
    _greenOnClickImage = [self getButtonImage:@"window-zoom-on"];

    // these files are only available in the yosemite variant
    if (OSX_YOSEMITE_AND_HIGHER) {
        _fullscreenImage = [self getButtonImage:@"window-fullscreen"];
        _fullscreenHoverImage = [self getButtonImage:@"window-fullscreen-over"];
        _fullscreenOnClickImage = [self getButtonImage:@"window-fullscreen-on"];
    }

    // old native fullscreen images are not available in graphite style
    // thus they are loaded directly here
    _oldFullscreenImage = [NSImage imageNamed:@"lion-window-fullscreen"];
    _oldFullscreenOnClickImage = [NSImage imageNamed:@"lion-window-fullscreen-on"];
    _oldFullscreenHoverImage = [NSImage imageNamed:@"lion-window-fullscreen-over"];

    [_redButton setImage: _redImage];
    [_redButton setAlternateImage: _redHoverImage];
    [[_redButton cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[_redButton cell] setTag: 0];
    [_yellowButton setImage: _yellowImage];
    [_yellowButton setAlternateImage: _yellowHoverImage];
    [[_yellowButton cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[_yellowButton cell] setTag: 1];

    [self updateGreenButton];
    [[_greenButton cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[_greenButton cell] setTag: 2];

    [_fullscreenButton setImage: _oldFullscreenImage];
    [_fullscreenButton setAlternateImage: _oldFullscreenHoverImage];
    [[_fullscreenButton cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[_fullscreenButton cell] setTag: 3];
}

- (void)updateGreenButton
{
    // default image for old version, or if native fullscreen is
    // disabled on yosemite, or if alt key is pressed
    if (!OSX_YOSEMITE_AND_HIGHER || !b_nativeFullscreenMode || b_alt_pressed) {

        if (b_mouse_over) {
            [_greenButton setImage: _greenHoverImage];
            [_greenButton setAlternateImage: _greenOnClickImage];
        } else {
            [_greenButton setImage: _greenImage];
            [_greenButton setAlternateImage: _greenOnClickImage];
        }
    } else {

        if (b_mouse_over) {
            [_greenButton setImage: _fullscreenHoverImage];
            [_greenButton setAlternateImage: _fullscreenOnClickImage];
        } else {
            [_greenButton setImage: _fullscreenImage];
            [_greenButton setAlternateImage: _fullscreenOnClickImage];
        }
    }
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (IBAction)buttonAction:(id)sender
{
    if (sender == _redButton)
        [[self window] performClose: sender];
    else if (sender == _yellowButton)
        [[self window] miniaturize: sender];
    else if (sender == _greenButton) {
        if (OSX_YOSEMITE_AND_HIGHER && b_nativeFullscreenMode && !b_alt_pressed) {
            [[self window] toggleFullScreen:self];
        } else {
            [[self window] performZoom: sender];
        }
    } else if (sender == _fullscreenButton) {
        // same action as native fs button
        [[self window] toggleFullScreen:self];

    } else
        msg_Err(getIntf(), "unknown button action sender");

    [self setWindowButtonOver: NO];
    [self setWindowFullscreenButtonOver: NO];
}

- (void)setWindowTitle:(NSString *)title
{
    NSMutableAttributedString *attributedTitleString = [[NSMutableAttributedString alloc] initWithString:title attributes: _windowTitleAttributesDictionary];
    NSUInteger i_titleLength = [title length];

    [attributedTitleString setAlignment: NSCenterTextAlignment range:NSMakeRange(0, i_titleLength)];
    [_titleLabel setAttributedStringValue:attributedTitleString];
}

- (void)setWindowButtonOver:(BOOL)b_value
{
    b_mouse_over = b_value;
    if (b_value) {
        [_redButton setImage: _redHoverImage];
        [_yellowButton setImage: _yellowHoverImage];
    } else {
        [_redButton setImage: _redImage];
        [_yellowButton setImage: _yellowImage];
    }

    [self updateGreenButton];
}

- (void)setWindowFullscreenButtonOver:(BOOL)b_value
{
    if (b_value)
        [_fullscreenButton setImage: _oldFullscreenHoverImage];
    else
        [_fullscreenButton setImage: _oldFullscreenImage];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint ml = [self convertPoint: [event locationInWindow] fromView: self];
    if (([[self window] frame].size.height - ml.y) <= 22. && [event clickCount] == 2) {
        //Get settings from "System Preferences" >  "Appearance" > "Double-click on windows title bar to minimize"
        NSString *const MDAppleMiniaturizeOnDoubleClickKey = @"AppleMiniaturizeOnDoubleClick";
        NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
        [userDefaults addSuiteNamed:NSGlobalDomain];

        if ([[userDefaults objectForKey:MDAppleMiniaturizeOnDoubleClickKey] boolValue])
            [[self window] miniaturize:self];
    }

    [super mouseDown: event];
}

- (NSButton*)closeButton
{
    return _redButton;
}

- (NSButton*)minimizeButton
{
    return _yellowButton;
}

- (NSButton*)zoomButton
{
    return _greenButton;
}

@end

/*****************************************************************************
 * VLCWindowButtonCell
 *
 * since the title bar cannot fetch these mouse events (the more top-level
 * NSButton is unable fetch them as well), we are using a subclass of the
 * button cell to do so. It's set in the nib for the respective objects.
 *****************************************************************************/

@implementation VLCWindowButtonCell

- (void)mouseEntered:(NSEvent *)theEvent
{
    if ([self tag] == 3)
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowFullscreenButtonOver: YES];
    else
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowButtonOver: YES];
}

- (void)mouseExited:(NSEvent *)theEvent
{
    if ([self tag] == 3)
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowFullscreenButtonOver: NO];
    else
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowButtonOver: NO];
}

/* accessibility stuff */
- (NSArray*)accessibilityAttributeNames {
    NSArray *theAttributeNames = [super accessibilityAttributeNames];
    id theControlView = [self controlView];
    return ([theControlView respondsToSelector: @selector(extendedAccessibilityAttributeNames:)] ? [theControlView extendedAccessibilityAttributeNames: theAttributeNames] : theAttributeNames); // ask the cell's control view (i.e., the button) for additional attribute values
}

- (id)accessibilityAttributeValue: (NSString*)theAttributeName {
    id theControlView = [self controlView];
    if ([theControlView respondsToSelector: @selector(extendedAccessibilityAttributeValue:)]) {
        id theValue = [theControlView extendedAccessibilityAttributeValue: theAttributeName];
        if (theValue) {
            return theValue; // if this is an extended attribute value we added, return that -- otherwise, fall back to super's implementation
        }
    }
    return [super accessibilityAttributeValue: theAttributeName];
}

- (BOOL)accessibilityIsAttributeSettable: (NSString*)theAttributeName {
    id theControlView = [self controlView];
    if ([theControlView respondsToSelector: @selector(extendedAccessibilityIsAttributeSettable:)]) {
        NSNumber *theValue = [theControlView extendedAccessibilityIsAttributeSettable: theAttributeName];
        if (theValue)
            return [theValue boolValue]; // same basic strategy we use in -accessibilityAttributeValue:
    }
    return [super accessibilityIsAttributeSettable: theAttributeName];
}

@end


/*****************************************************************************
 * custom window buttons to support the accessibility stuff
 *****************************************************************************/

@implementation VLCCustomWindowButtonPrototype
+ (Class)cellClass {
    return [VLCWindowButtonCell class];
}

- (NSArray*)extendedAccessibilityAttributeNames: (NSArray*)theAttributeNames {
    return ([theAttributeNames containsObject: NSAccessibilitySubroleAttribute] ? theAttributeNames : [theAttributeNames arrayByAddingObject: NSAccessibilitySubroleAttribute]); // run-of-the-mill button cells don't usually have a Subrole attribute, so we add that attribute
}

- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName {
    return nil;
}

- (NSNumber*)extendedAccessibilityIsAttributeSettable: (NSString*)theAttributeName {
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? [NSNumber numberWithBool:NO] : nil); // make the Subrole attribute we added non-settable
}

- (void)accessibilityPerformAction: (NSString*)theActionName {
    if ([theActionName isEqualToString: NSAccessibilityPressAction]) {
        if ([self isEnabled])
            [self performClick: nil];
    } else
        [super accessibilityPerformAction: theActionName];
}

@end

@implementation VLCCustomWindowCloseButton
- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName {
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityCloseButtonAttribute : nil);
}

@end


@implementation VLCCustomWindowMinimizeButton
- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName {
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityMinimizeButtonAttribute : nil);
}

@end


@implementation VLCCustomWindowZoomButton
- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName {
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityZoomButtonAttribute : nil);
}

@end


@implementation VLCCustomWindowFullscreenButton
- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName {
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityFullScreenButtonAttribute : nil);
}
@end


@interface VLCWindowTitleTextField()
{
    NSMenu *_contextMenu;
}
@end

@implementation VLCWindowTitleTextField

- (void)showRightClickMenuWithEvent:(NSEvent *)o_event
{
    NSURL * representedURL = [[self window] representedURL];
    if (!representedURL)
        return;

    NSArray * pathComponents;
    pathComponents = [representedURL pathComponents];

    if (!pathComponents)
        return;

    _contextMenu = [[NSMenu alloc] initWithTitle: [[NSFileManager defaultManager] displayNameAtPath: [representedURL path]]];

    NSUInteger count = [pathComponents count];
    NSImage * icon;
    NSMenuItem * currentItem;
    NSMutableString * currentPath;
    NSSize iconSize = NSMakeSize(16., 16.);
    for (NSUInteger i = count - 1; i > 0; i--) {
        currentPath = [NSMutableString stringWithCapacity:1024];
        for (NSUInteger y = 0; y < i; y++)
            [currentPath appendFormat: @"/%@", [pathComponents objectAtIndex:y + 1]];

        [_contextMenu addItemWithTitle: [[NSFileManager defaultManager] displayNameAtPath: currentPath] action:@selector(revealInFinder:) keyEquivalent:@""];
        currentItem = [_contextMenu itemAtIndex:[_contextMenu numberOfItems] - 1];
        [currentItem setTarget: self];

        icon = [[NSWorkspace sharedWorkspace] iconForFile:currentPath];
        [icon setSize: iconSize];
        [currentItem setImage: icon];
    }

    if ([[pathComponents objectAtIndex:1] isEqualToString:@"Volumes"]) {
        /* we don't want to show the Volumes item, since the Cocoa does it neither */
        currentItem = [_contextMenu itemWithTitle:[[NSFileManager defaultManager] displayNameAtPath: @"/Volumes"]];
        if (currentItem)
            [_contextMenu removeItem: currentItem];
    } else {
        /* we're on the boot drive, so add it since it isn't part of the components */
        [_contextMenu addItemWithTitle: [[NSFileManager defaultManager] displayNameAtPath:@"/"] action:@selector(revealInFinder:) keyEquivalent:@""];
        currentItem = [_contextMenu itemAtIndex: [_contextMenu numberOfItems] - 1];
        icon = [[NSWorkspace sharedWorkspace] iconForFile:@"/"];
        [icon setSize: iconSize];
        [currentItem setImage: icon];
        [currentItem setTarget: self];
    }

    /* add the computer item */
    [_contextMenu addItemWithTitle:(NSString*)CFBridgingRelease(SCDynamicStoreCopyComputerName(NULL, NULL)) action:@selector(revealInFinder:) keyEquivalent:@""];
    currentItem = [_contextMenu itemAtIndex: [_contextMenu numberOfItems] - 1];
    icon = [NSImage imageNamed: NSImageNameComputer];
    [icon setSize: iconSize];
    [currentItem setImage: icon];
    [currentItem setTarget: self];

    // center the context menu similar to the white interface
    CGFloat menuWidth = [_contextMenu size].width;
    NSRect windowFrame = [[self window] frame];
    NSPoint point;

    CGFloat fullButtonWidth = 0.;
    if([[VLCMain sharedInstance] nativeFullscreenMode])
        fullButtonWidth = 20.;

    // assumes 60 px for the window buttons
    point.x = (windowFrame.size.width - 60. - fullButtonWidth) / 2. - menuWidth / 2. + 60. - 20.;
    point.y = windowFrame.size.height + 1.;
    if (point.x < 0)
        point.x = 10;

    NSEvent *fakeMouseEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                                 location:point
                                            modifierFlags:0
                                                timestamp:0
                                             windowNumber:[[self window] windowNumber]
                                                  context:nil
                                              eventNumber:0
                                               clickCount:0
                                                 pressure:0];
    [NSMenu popUpContextMenu: _contextMenu withEvent: fakeMouseEvent forView: [self superview]];
}

- (IBAction)revealInFinder:(id)sender
{
    NSUInteger count = [_contextMenu numberOfItems];
    NSUInteger selectedItem = [_contextMenu indexOfItem: sender];

    if (selectedItem == count - 1) { // the fake computer item
        [[NSWorkspace sharedWorkspace] selectFile: @"/" inFileViewerRootedAtPath: @""];
        return;
    }

    NSURL * representedURL = [[self window] representedURL];
    if (! representedURL)
        return;

    if (selectedItem == 0) { // the actual file, let's save time
        [[NSWorkspace sharedWorkspace] selectFile: [representedURL path] inFileViewerRootedAtPath: [representedURL path]];
        return;
    }

    NSArray * pathComponents;
    pathComponents = [representedURL pathComponents];
    if (!pathComponents)
        return;

    NSMutableString * currentPath;
    currentPath = [NSMutableString stringWithCapacity:1024];
    selectedItem = count - selectedItem;

    /* fix for non-startup volumes */
    if ([[pathComponents objectAtIndex:1] isEqualToString:@"Volumes"])
        selectedItem += 1;

    for (NSUInteger y = 1; y < selectedItem; y++)
        [currentPath appendFormat: @"/%@", [pathComponents objectAtIndex:y]];

    [[NSWorkspace sharedWorkspace] selectFile: currentPath inFileViewerRootedAtPath: currentPath];
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    if ([o_event type] == NSRightMouseDown)
        [self showRightClickMenuWithEvent:o_event];

    [super mouseDown: o_event];
}

@end
