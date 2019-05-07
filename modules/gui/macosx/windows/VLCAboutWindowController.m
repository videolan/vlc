/*****************************************************************************
 * VLCAboutWindowController.m
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Paul Kühne <fkuehne -at- videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import "VLCAboutWindowController.h"

#import <vlc_intf_strings.h>
#import <vlc_about.h>

#import "extensions/NSString+Helpers.h"
#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"

#import "views/VLCScrollingClipView.h"


/* this is a bit weird, but we should be confident that there will be more than
 * one arch to support again one day */
#define PLATFORM "Intel 64bit"

@interface VLCAboutWindowController ()
{
    NSString *_authorsString;
}
@end

@implementation VLCAboutWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"About"];
    if (self) {
        [self setWindowFrameAutosaveName:@"about"];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
}

/*****************************************************************************
* VLC About Window
*****************************************************************************/

- (void)showAbout
{
    [self window];

    /* Show the window */
    [o_credits_scrollview setHidden:YES];
    [o_credits_textview setHidden:YES];
    [o_joinus_txt setHidden:NO];
    [o_copyright_field setHidden:NO];
    [o_revision_field setHidden:NO];
    [o_name_version_field setHidden:NO];
    [o_credits_textview scrollPoint:NSMakePoint(0, 0)];

    [self showWindow:nil];
}

- (void)windowDidLoad
{
    [[self window] setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    /* Get the localized info dictionary (InfoPlist.strings) */
    NSDictionary *localizedInfoDict = [[NSBundle mainBundle] localizedInfoDictionary];

    /* Setup the copyright field */
    [o_copyright_field setStringValue: [localizedInfoDict objectForKey:@"NSHumanReadableCopyright"]];

    /* l10n */
    [[self window] setTitle: _NS("About VLC media player")];
    NSDictionary *stringAttributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName, [NSColor secondaryLabelColor], NSForegroundColorAttributeName, [NSFont systemFontOfSize:13], NSFontAttributeName, nil];
    NSAttributedString *attrStr;
    attrStr = [[NSAttributedString alloc] initWithString:_NS("Credits") attributes:stringAttributes];
    [o_credits_btn setAttributedTitle:attrStr];
    attrStr = [[NSAttributedString alloc] initWithString:_NS("License") attributes:stringAttributes];
    [o_gpl_btn setAttributedTitle:attrStr];
    attrStr = [[NSAttributedString alloc] initWithString:_NS("Authors") attributes:stringAttributes];
    [o_authors_btn setAttributedTitle:attrStr];
    [o_trademarks_txt setStringValue:_NS("VLC media player and VideoLAN are trademarks of the VideoLAN Association.")];

    /* setup the creator / revision field */
    NSString *compiler;
#ifdef __clang__
    compiler = [NSString stringWithFormat:@"clang %s", __clang_version__];
#else
    compiler = [NSString stringWithFormat:@"llvm-gcc %s", __VERSION__];
#endif
    [o_revision_field setStringValue: [NSString stringWithFormat:@"Compiled by %s with %@ (%s %s)", VLC_CompileBy(), compiler, __DATE__, __TIME__]];

    /* Setup the nameversion field */
    [o_name_version_field setStringValue: [NSString stringWithFormat:@"Version %s (%s)", VERSION_MESSAGE, PLATFORM]];

    NSMutableArray *tmpArray = [NSMutableArray arrayWithArray: [toNSStr(psz_authors) componentsSeparatedByString:@"\n\n"]];
    NSUInteger count = [tmpArray count];
    for (NSUInteger i = 0; i < count; i++) {
        [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@"\n" withString:@", "]];
        [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@", -" withString:@"\n-" options:0 range:NSRangeFromString(@"0 30")]];
        [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@"-, " withString:@"-\n" options:0 range:NSRangeFromString(@"0 30")]];
        [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@","]]];
    }
    _authorsString = [tmpArray componentsJoinedByString:@"\n\n"];

    /* setup join us! */
    NSString *joinus = toNSStr(_(""
                                 "<p>VLC media player is a free and open source media player, encoder, and "
                                 "streamer made by the volunteers of the <a href=\"https://www.videolan.org/"
                                 "\"><span style=\" text-decoration: underline; color:#0057ae;\">VideoLAN</"
                                 "span></a> community.</p><p>VLC uses its internal codecs, works on "
                                 "essentially every popular platform, and can read almost all files, CDs, "
                                 "DVDs, network streams, capture cards and other media formats!</p><p><a href="
                                 "\"https://www.videolan.org/contribute/\"><span style=\" text-decoration: "
                                 "underline; color:#0057ae;\">Help and join us!</span></a>"));

    NSString *joinUsWithStyle = [NSString stringWithFormat:@"<div style=\"text-align:left;font-family: -apple-system, Helvetica Neue;\">%@</div>", joinus];
    NSMutableAttributedString *joinus_readytorender = [[NSMutableAttributedString alloc] initWithHTML:[joinUsWithStyle dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES]
                                                                                              options:@{NSCharacterEncodingDocumentOption : [NSNumber numberWithInt:NSUTF8StringEncoding]}
                                                                                   documentAttributes:NULL];
    [joinus_readytorender setAttributes:@{NSForegroundColorAttributeName : [NSColor secondaryLabelColor],
                                          NSFontAttributeName : [NSFont systemFontOfSize:12.]}
                                  range:NSMakeRange(0, joinus_readytorender.length)];
    [o_joinus_txt setAllowsEditingTextAttributes: YES];
    [o_joinus_txt setSelectable: YES];
    [o_joinus_txt setAttributedStringValue:joinus_readytorender];

    [o_credits_textview setString: @""];

    /* Setup the window */
    [o_credits_textview setDrawsBackground: NO];
    [o_credits_scrollview setDrawsBackground: NO];
    [[self window] setExcludedFromWindowsMenu:YES];
    [[self window] setMenu:nil];

    if (config_GetInt("macosx-icon-change")) {
        /* After day 354 of the year, the usual VLC cone is replaced by another cone
         * wearing a Father Xmas hat.
         * Note: this icon doesn't represent an endorsement of The Coca-Cola Company.
         */
        NSCalendar *gregorian =
        [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
        NSUInteger dayOfYear = [gregorian ordinalityOfUnit:NSCalendarUnitDay inUnit:NSCalendarUnitYear forDate:[NSDate date]];

        if (dayOfYear >= 354)
            [o_icon_view setImage: [NSImage imageNamed:@"VLC-Xmas"]];
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    [(VLCScrollingClipView *)[o_credits_scrollview contentView] startScrolling];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    [(VLCScrollingClipView *)[o_credits_scrollview contentView] stopScrolling];
}

- (IBAction)buttonAction:(id)sender
{
    [o_credits_scrollview setHidden:NO];
    [o_credits_textview setHidden:NO];
    [o_joinus_txt setHidden:YES];
    [o_copyright_field setHidden:YES];
    [o_revision_field setHidden:YES];
    [o_name_version_field setHidden:YES];

    NSString *stringToDisplay;
    if (sender == o_authors_btn)
        stringToDisplay = _authorsString;
    else if (sender == o_credits_btn)
        stringToDisplay = [toNSStr(psz_thanks) stringByReplacingOccurrencesOfString:@"\n" withString:@" "
                                                                            options:0 range:NSRangeFromString(@"680 2")];
    else
        stringToDisplay = toNSStr(psz_license);

    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:stringToDisplay
                                                                           attributes:@{NSForegroundColorAttributeName : [NSColor secondaryLabelColor],
                                                                                        NSFontAttributeName : [NSFont systemFontOfSize:12.]}];
    [[o_credits_textview textStorage] setAttributedString:attributedString];

    VLCScrollingClipView *scrollView = (VLCScrollingClipView *)[o_credits_scrollview contentView];
    [scrollView resetScrolling];
    [scrollView startScrolling];
}

/*****************************************************************************
* VLC GPL Window, action called from the about window and the help menu
*****************************************************************************/

- (void)showGPL
{
    [self showAbout];
    [self buttonAction:nil];
}

@end
