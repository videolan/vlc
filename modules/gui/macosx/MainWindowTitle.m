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
#import "intf.h"
#import "MainWindowTitle.h"
#import "CoreInteraction.h"
#import "CompatibilityFixes.h"
#import <SystemConfiguration/SystemConfiguration.h> // for the revealInFinder clone

/*****************************************************************************
 * VLCMainWindowTitleView
 *
 * this is our title bar, which can do anything a title should do
 * it relies on the VLCWindowButtonCell to display the correct traffic light
 * states, since we can't capture the mouse-moved events here correctly
 *****************************************************************************/

@implementation VLCMainWindowTitleView
- (id)init
{
    o_window_title_attributes_dict = [[NSDictionary dictionaryWithObjectsAndKeys: [NSColor whiteColor], NSForegroundColorAttributeName, [NSFont titleBarFontOfSize:12.0], NSFontAttributeName, nil] retain];

    return [super init];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [o_red_img release];
    [o_red_over_img release];
    [o_red_on_img release];
    [o_yellow_img release];
    [o_yellow_over_img release];
    [o_yellow_on_img release];
    [o_green_img release];
    [o_green_over_img release];
    [o_green_on_img release];

    [o_window_title_shadow release];
    [o_window_title_attributes_dict release];

    [super dealloc];
}

- (void)awakeFromNib
{
    [self setAutoresizesSubviews: YES];
    [self setImagesLeft:[NSImage imageNamed:@"topbar-dark-left"] middle: [NSImage imageNamed:@"topbar-dark-center-fill"] right:[NSImage imageNamed:@"topbar-dark-right"]];

    [self loadButtonIcons];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(controlTintChanged:) name: NSControlTintDidChangeNotification object: nil];
}

- (void)controlTintChanged:(NSNotification *)notification
{
    [o_red_img release];
    [o_red_over_img release];
    [o_red_on_img release];
    [o_yellow_img release];
    [o_yellow_over_img release];
    [o_yellow_on_img release];
    [o_green_img release];
    [o_green_over_img release];
    [o_green_on_img release];

    [self loadButtonIcons];

    [o_red_btn setNeedsDisplay];
    [o_yellow_btn setNeedsDisplay];
    [o_green_btn setNeedsDisplay];
}

- (void)loadButtonIcons
{
    if (!OSX_SNOW_LEOPARD) {
        if ([NSColor currentControlTint] == NSBlueControlTint)
        {
            o_red_img = [[NSImage imageNamed:@"lion-window-close"] retain];
            o_red_over_img = [[NSImage imageNamed:@"lion-window-close-over"] retain];
            o_red_on_img = [[NSImage imageNamed:@"lion-window-close-on"] retain];
            o_yellow_img = [[NSImage imageNamed:@"lion-window-minimize"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"lion-window-minimize-over"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"lion-window-minimize-on"] retain];
            o_green_img = [[NSImage imageNamed:@"lion-window-zoom"] retain];
            o_green_over_img = [[NSImage imageNamed:@"lion-window-zoom-over"] retain];
            o_green_on_img = [[NSImage imageNamed:@"lion-window-zoom-on"] retain];
        } else {
            o_red_img = [[NSImage imageNamed:@"lion-window-close-graphite"] retain];
            o_red_over_img = [[NSImage imageNamed:@"lion-window-close-over-graphite"] retain];
            o_red_on_img = [[NSImage imageNamed:@"lion-window-close-on-graphite"] retain];
            o_yellow_img = [[NSImage imageNamed:@"lion-window-minimize-graphite"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"lion-window-minimize-over-graphite"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"lion-window-minimize-on-graphite"] retain];
            o_green_img = [[NSImage imageNamed:@"lion-window-zoom-graphite"] retain];
            o_green_over_img = [[NSImage imageNamed:@"lion-window-zoom-over-graphite"] retain];
            o_green_on_img = [[NSImage imageNamed:@"lion-window-zoom-on-graphite"] retain];
        }
    } else {
        if ([NSColor currentControlTint] == NSBlueControlTint)
        {
            o_red_img = [[NSImage imageNamed:@"snowleo-window-close"] retain];
            o_red_over_img = [[NSImage imageNamed:@"snowleo-window-close-over"] retain];
            o_red_on_img = [[NSImage imageNamed:@"snowleo-window-close-on"] retain];
            o_yellow_img = [[NSImage imageNamed:@"snowleo-window-minimize"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"snowleo-window-minimize-over"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"snowleo-window-minimize-on"] retain];
            o_green_img = [[NSImage imageNamed:@"snowleo-window-zoom"] retain];
            o_green_over_img = [[NSImage imageNamed:@"snowleo-window-zoom-over"] retain];
            o_green_on_img = [[NSImage imageNamed:@"snowleo-window-zoom-on"] retain];
        } else {
            o_red_img = [[NSImage imageNamed:@"snowleo-window-close-graphite"] retain];
            o_red_over_img = [[NSImage imageNamed:@"snowleo-window-close-over-graphite"] retain];
            o_red_on_img = [[NSImage imageNamed:@"snowleo-window-close-on-graphite"] retain];
            o_yellow_img = [[NSImage imageNamed:@"snowleo-window-minimize-graphite"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"snowleo-window-minimize-over-graphite"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"snowleo-window-minimize-on-graphite"] retain];
            o_green_img = [[NSImage imageNamed:@"snowleo-window-zoom-graphite"] retain];
            o_green_over_img = [[NSImage imageNamed:@"snowleo-window-zoom-over-graphite"] retain];
            o_green_on_img = [[NSImage imageNamed:@"snowleo-window-zoom-on-graphite"] retain];
        }
    }

    [o_red_btn setImage: o_red_img];
    [o_red_btn setAlternateImage: o_red_on_img];
    [[o_red_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_red_btn cell] setTag: 0];
    [o_yellow_btn setImage: o_yellow_img];
    [o_yellow_btn setAlternateImage: o_yellow_on_img];
    [[o_yellow_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_yellow_btn cell] setTag: 1];
    [o_green_btn setImage: o_green_img];
    [o_green_btn setAlternateImage: o_green_on_img];
    [[o_green_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_green_btn cell] setTag: 2];
    [o_fullscreen_btn setImage: [NSImage imageNamed:@"window-fullscreen"]];
    [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"window-fullscreen-on"]];
    [[o_fullscreen_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_fullscreen_btn cell] setTag: 3];
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (IBAction)buttonAction:(id)sender
{
    if (sender == o_red_btn)
        [[self window] performClose: sender];
    else if (sender == o_yellow_btn)
        [[self window] miniaturize: sender];
    else if (sender == o_green_btn)
        [[self window] performZoom: sender];
    else if (sender == o_fullscreen_btn) {
        // same action as native fs button
        [[self window] toggleFullScreen:self];

    } else
        msg_Err(VLCIntf, "unknown button action sender");

    [self setWindowButtonOver: NO];
    [self setWindowFullscreenButtonOver: NO];
}

- (void)setWindowTitle:(NSString *)title
{
    if (!o_window_title_shadow) {
        o_window_title_shadow = [[NSShadow alloc] init];
        [o_window_title_shadow setShadowColor:[NSColor colorWithCalibratedWhite:1.0 alpha:0.5]];
        [o_window_title_shadow setShadowOffset:NSMakeSize(0.0, -1.5)];
        [o_window_title_shadow setShadowBlurRadius:0.5];
        [o_window_title_shadow retain];
    }

    NSMutableAttributedString *o_attributed_title = [[NSMutableAttributedString alloc] initWithString:title attributes: o_window_title_attributes_dict];
    NSUInteger i_titleLength = [title length];

    [o_attributed_title addAttribute:NSShadowAttributeName value:o_window_title_shadow range:NSMakeRange(0, i_titleLength)];
    [o_attributed_title setAlignment: NSCenterTextAlignment range:NSMakeRange(0, i_titleLength)];
    [o_title_lbl setAttributedStringValue:o_attributed_title];
    [o_attributed_title release];
}

- (void)setFullscreenButtonHidden:(BOOL)b_value
{
    [o_fullscreen_btn setHidden: b_value];
}

- (void)setWindowButtonOver:(BOOL)b_value
{
    if (b_value) {
        [o_red_btn setImage: o_red_over_img];
        [o_yellow_btn setImage: o_yellow_over_img];
        [o_green_btn setImage: o_green_over_img];
    } else {
        [o_red_btn setImage: o_red_img];
        [o_yellow_btn setImage: o_yellow_img];
        [o_green_btn setImage: o_green_img];
    }
}

- (void)setWindowFullscreenButtonOver:(BOOL)b_value
{
    if (b_value)
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"window-fullscreen-over"]];
    else
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"window-fullscreen"]];
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
    return o_red_btn;
}

- (NSButton*)minimizeButton
{
    return o_yellow_btn;
}

- (NSButton*)zoomButton
{
    return o_green_btn;
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
 * VLCResizeControl
 *
 * For Leopard and Snow Leopard, we need to emulate the resize control on the
 * bottom right of the window, since it is gone by using the borderless window
 * mask. A proper fix would be Lion-only.
 *****************************************************************************/

@implementation VLCResizeControl

- (void)mouseDown:(NSEvent *)theEvent {
    BOOL keepOn = YES;

    while (keepOn) {
        theEvent = [[self window] nextEventMatchingMask: NSLeftMouseUpMask |
                    NSLeftMouseDraggedMask];

        switch ([theEvent type]) {
            case NSLeftMouseDragged:
            {
                NSRect windowFrame = [[self window] frame];
                CGFloat deltaX, deltaY, oldOriginY;
                deltaX = [theEvent deltaX];
                deltaY = [theEvent deltaY];
                oldOriginY = windowFrame.origin.y;

                windowFrame.origin.y = (oldOriginY + windowFrame.size.height) - (windowFrame.size.height + deltaY);
                windowFrame.size.width += deltaX;
                windowFrame.size.height += deltaY;

                NSSize winMinSize = [self window].minSize;
                if (windowFrame.size.width < winMinSize.width)
                    windowFrame.size.width = winMinSize.width;

                if (windowFrame.size.height < winMinSize.height) {
                    windowFrame.size.height = winMinSize.height;
                    windowFrame.origin.y = oldOriginY;
                }

                [[self window] setFrame: windowFrame display: YES animate: NO];
                break;
            }
                break;
            case NSLeftMouseUp:
                keepOn = NO;
                break;
            default:
                /* Ignore any other kind of event. */
                break;
        }

    };

    return;
}

@end

/*****************************************************************************
 * VLCColorView
 *
 * since we are using a clear window color when using the black window
 * style, some filling is needed behind the video and some other elements
 *****************************************************************************/

@implementation VLCColorView

- (void)drawRect:(NSRect)rect {
    [[NSColor blackColor] setFill];
    NSRectFill(rect);
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
#ifdef MAC_OS_X_VERSION_10_7
- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName {
    return ([theAttributeName isEqualToString: NSAccessibilitySubroleAttribute] ? NSAccessibilityFullScreenButtonAttribute : nil);
}
#endif

@end


@implementation VLCWindowTitleTextField

- (void)dealloc
{
    if (contextMenu)
        [contextMenu release];

    [super dealloc];
}

- (void)showRightClickMenuWithEvent:(NSEvent *)o_event
{
    if (contextMenu)
        [contextMenu release];

    NSURL * representedURL = [[self window] representedURL];
    if (! representedURL)
        return;

    NSArray * pathComponents;
    pathComponents = [representedURL pathComponents];

    if (!pathComponents)
        return;

    contextMenu = [[NSMenu alloc] initWithTitle: [[NSFileManager defaultManager] displayNameAtPath: [representedURL path]]];

    NSUInteger count = [pathComponents count];
    NSImage * icon;
    NSMenuItem * currentItem;
    NSMutableString * currentPath;
    NSSize iconSize = NSMakeSize(16., 16.);
    for (NSUInteger i = count - 1; i > 0; i--) {
        currentPath = [NSMutableString stringWithCapacity:1024];
        for (NSUInteger y = 0; y < i; y++)
            [currentPath appendFormat: @"/%@", [pathComponents objectAtIndex:y + 1]];

        [contextMenu addItemWithTitle: [[NSFileManager defaultManager] displayNameAtPath: currentPath] action:@selector(revealInFinder:) keyEquivalent:@""];
        currentItem = [contextMenu itemAtIndex:[contextMenu numberOfItems] - 1];
        [currentItem setTarget: self];

        icon = [[NSWorkspace sharedWorkspace] iconForFile:currentPath];
        [icon setSize: iconSize];
        [currentItem setImage: icon];
    }

    if ([[pathComponents objectAtIndex:1] isEqualToString:@"Volumes"]) {
        /* we don't want to show the Volumes item, since the Cocoa does it neither */
        currentItem = [contextMenu itemWithTitle:[[NSFileManager defaultManager] displayNameAtPath: @"/Volumes"]];
        if (currentItem)
            [contextMenu removeItem: currentItem];
    } else {
        /* we're on the boot drive, so add it since it isn't part of the components */
        [contextMenu addItemWithTitle: [[NSFileManager defaultManager] displayNameAtPath:@"/"] action:@selector(revealInFinder:) keyEquivalent:@""];
        currentItem = [contextMenu itemAtIndex: [contextMenu numberOfItems] - 1];
        icon = [[NSWorkspace sharedWorkspace] iconForFile:@"/"];
        [icon setSize: iconSize];
        [currentItem setImage: icon];
        [currentItem setTarget: self];
    }

    /* add the computer item */
    [contextMenu addItemWithTitle: [(NSString*)SCDynamicStoreCopyComputerName(NULL, NULL) autorelease] action:@selector(revealInFinder:) keyEquivalent:@""];
    currentItem = [contextMenu itemAtIndex: [contextMenu numberOfItems] - 1];
    icon = [NSImage imageNamed: NSImageNameComputer];
    [icon setSize: iconSize];
    [currentItem setImage: icon];
    [currentItem setTarget: self];

    [NSMenu popUpContextMenu: contextMenu withEvent: o_event forView: [self superview]];
}

- (IBAction)revealInFinder:(id)sender
{
    NSUInteger count = [contextMenu numberOfItems];
    NSUInteger selectedItem = [contextMenu indexOfItem: sender];

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
