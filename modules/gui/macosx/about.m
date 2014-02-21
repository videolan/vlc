/*****************************************************************************
 * about.m: MacOS X About Panel
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
 * $Id$
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
#import "intf.h"
#import "about.h"
#import <vlc_intf_strings.h>
#import <vlc_about.h>
#import "CompatibilityFixes.h"

/* this is a bit weird, but we should be confident that there will be more than
 * one arch to support again one day */
#define PLATFORM "Intel 64bit"

/*****************************************************************************
 * VLAboutBox implementation
 *****************************************************************************/
@implementation VLAboutBox

static VLAboutBox *_o_sharedInstance = nil;

+ (VLAboutBox *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance)
        [self dealloc];
    else
        _o_sharedInstance = [super init];

    return _o_sharedInstance;
}

- (void) dealloc
{
    [o_authors release];
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [super dealloc];
}

- (void)awakeFromNib
{
    if (!OSX_SNOW_LEOPARD)
        [o_about_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
}

/*****************************************************************************
* VLC About Window
*****************************************************************************/

- (void)showAbout
{
    if (! b_isSetUp) {
        /* Get the localized info dictionary (InfoPlist.strings) */
        NSDictionary *o_local_dict;
        o_local_dict = [[NSBundle mainBundle] localizedInfoDictionary];

        /* Setup the copyright field */
        [o_copyright_field setStringValue: [o_local_dict objectForKey:@"NSHumanReadableCopyright"]];

        /* l10n */
        [o_about_window setTitle: _NS("About VLC media player")];
        NSDictionary *stringAttributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName, [NSColor colorWithCalibratedRed:0. green:0.3411 blue:0.6824 alpha:1.], NSForegroundColorAttributeName, [NSFont systemFontOfSize:13], NSFontAttributeName, nil];
        NSAttributedString *attrStr;
        attrStr = [[NSAttributedString alloc] initWithString:_NS("Credits") attributes:stringAttributes];
        [o_credits_btn setAttributedTitle:attrStr];
        [attrStr release];
        attrStr = [[NSAttributedString alloc] initWithString:_NS("License") attributes:stringAttributes];
        [o_gpl_btn setAttributedTitle:attrStr];
        [attrStr release];
        attrStr = [[NSAttributedString alloc] initWithString:_NS("Authors") attributes:stringAttributes];
        [o_authors_btn setAttributedTitle:attrStr];
        [attrStr release];
        [o_trademarks_txt setStringValue:_NS("VLC media player and VideoLAN are trademarks of the VideoLAN Association.")];

        /* setup the creator / revision field */
        NSString *compiler;
#ifdef __clang__
        compiler = [NSString stringWithFormat:@"clang %s", __clang_version__];
#else
        compiler = [NSString stringWithFormat:@"llvm-gcc %s", __VERSION__];
#endif
        [o_revision_field setStringValue: [NSString stringWithFormat: _NS("Compiled by %s with %@"), VLC_CompileBy(), compiler]];

        /* Setup the nameversion field */
        [o_name_version_field setStringValue: [NSString stringWithFormat:@"Version %s (%s)", VERSION_MESSAGE, PLATFORM]];

        NSMutableArray *tmpArray = [NSMutableArray arrayWithArray: [[NSString stringWithUTF8String:psz_authors] componentsSeparatedByString:@"\n\n"]];
        NSUInteger count = [tmpArray count];
        for (NSUInteger i = 0; i < count; i++) {
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@"\n" withString:@", "]];
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@", -" withString:@"\n-" options:0 range:NSRangeFromString(@"0 30")]];
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@"-, " withString:@"-\n" options:0 range:NSRangeFromString(@"0 30")]];
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@","]]];
        }
        o_authors = [tmpArray componentsJoinedByString:@"\n\n"];
        [o_authors retain];

        /* setup join us! */
        NSString *joinus = [NSString stringWithUTF8String:_(""
                                                            "<p>VLC media player is a free and open source media player, encoder, and "
                                                            "streamer made by the volunteers of the <a href=\"http://www.videolan.org/"
                                                            "\"><span style=\" text-decoration: underline; color:#0057ae;\">VideoLAN</"
                                                            "span></a> community.</p><p>VLC uses its internal codecs, works on "
                                                            "essentially every popular platform, and can read almost all files, CDs, "
                                                            "DVDs, network streams, capture cards and other media formats!</p><p><a href="
                                                            "\"http://www.videolan.org/contribute/\"><span style=\" text-decoration: "
                                                            "underline; color:#0057ae;\">Help and join us!</span></a>")];
        NSAttributedString *joinus_readytorender = [[NSAttributedString alloc] initWithHTML:[joinus dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES] options:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:NSUTF8StringEncoding] forKey:NSCharacterEncodingDocumentOption] documentAttributes:NULL];
        [o_joinus_txt setAllowsEditingTextAttributes: YES];
        [o_joinus_txt setSelectable: YES];
        [o_joinus_txt setAttributedStringValue:joinus_readytorender];

        [joinus_readytorender release];
        [o_credits_textview setString: @""];

        /* Setup the window */
        [o_credits_textview setDrawsBackground: NO];
        [o_credits_scrollview setDrawsBackground: NO];
        [o_about_window setExcludedFromWindowsMenu:YES];
        [o_about_window setMenu:nil];
        [o_about_window center];
        [o_about_window setBackgroundColor: [NSColor colorWithCalibratedWhite:.96 alpha:1.]];

        if (config_GetInt(VLCIntf, "macosx-icon-change")) {
            /* After day 354 of the year, the usual VLC cone is replaced by another cone
             * wearing a Father Xmas hat.
             * Note: this icon doesn't represent an endorsement of The Coca-Cola Company.
             */
            NSCalendar *gregorian =
            [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
            NSUInteger dayOfYear = [gregorian ordinalityOfUnit:NSDayCalendarUnit inUnit:NSYearCalendarUnit forDate:[NSDate date]];
            [gregorian release];

            if (dayOfYear >= 354)
                [o_icon_view setImage: [NSImage imageNamed:@"vlc-xmas"]];
        }

        b_isSetUp = YES;
    }

    /* Show the window */
    b_restart = YES;
    [o_credits_scrollview setHidden:YES];
    [o_credits_textview setHidden:YES];
    [o_joinus_txt setHidden:NO];
    [o_copyright_field setHidden:NO];
    [o_revision_field setHidden:NO];
    [o_name_version_field setHidden:NO];
    [o_credits_textview scrollPoint:NSMakePoint(0, 0)];
    [o_about_window makeKeyAndOrderFront: nil];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    o_scroll_timer = [NSTimer scheduledTimerWithTimeInterval: 1/6
                                                      target:self
                                                    selector:@selector(scrollCredits:)
                                                    userInfo:nil
                                                     repeats:YES];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    [o_scroll_timer invalidate];
}

- (void)scrollCredits:(NSTimer *)timer
{
    if (b_restart) {
        /* Reset the starttime */
        i_start = [NSDate timeIntervalSinceReferenceDate] + 4.0;
        f_current = 0;
        f_end = [o_credits_textview bounds].size.height - [o_credits_scrollview bounds].size.height;
        b_restart = NO;
    }

    if ([NSDate timeIntervalSinceReferenceDate] >= i_start) {
        /* Increment the scroll position */
        f_current += 0.005;

        /* Scroll to the position */
        [o_credits_textview scrollPoint:NSMakePoint(0, f_current)];

        /* If at end, restart at the top */
        if (f_current >= f_end) {
            /* f_end may be wrong on first run, so don't trust it too much */
            if (f_end == [o_credits_textview bounds].size.height - [o_credits_scrollview bounds].size.height) {
                sleep(2);
                b_restart = YES;
                [o_credits_textview scrollPoint:NSMakePoint(0, 0)];
            } else
                f_end = [o_credits_textview bounds].size.height - [o_credits_scrollview bounds].size.height;
        }
    }
}

- (IBAction)buttonAction:(id)sender
{
    [o_credits_scrollview setHidden:NO];
    [o_credits_textview setHidden:NO];
    [o_joinus_txt setHidden:YES];
    [o_copyright_field setHidden:YES];
    [o_revision_field setHidden:YES];
    [o_name_version_field setHidden:YES];

    if (sender == o_authors_btn)
        [o_credits_textview setString:o_authors];
    else if (sender == o_credits_btn)
        [o_credits_textview setString:[[NSString stringWithUTF8String:psz_thanks] stringByReplacingOccurrencesOfString:@"\n" withString:@" " options:0 range:NSRangeFromString(@"680 2")]];
    else
        [o_credits_textview setString:[NSString stringWithUTF8String:psz_license]];

    [o_credits_textview scrollPoint:NSMakePoint(0, 0)];
    b_restart = YES;
}

/*****************************************************************************
* VLC GPL Window, action called from the about window and the help menu
*****************************************************************************/

- (void)showGPL
{
    [self showAbout];
    [self buttonAction:nil];
}

/*****************************************************************************
* VLC Generic Help Window
*****************************************************************************/

- (void)showHelp
{
    [o_help_window setTitle: _NS("VLC media player Help")];
    [o_help_fwd_btn setToolTip: _NS("Next")];
    [o_help_bwd_btn setToolTip: _NS("Previous")];
    [o_help_home_btn setToolTip: _NS("Index")];

    [o_help_window makeKeyAndOrderFront: self];

    [[o_help_web_view mainFrame] loadHTMLString: _NS(I_LONGHELP)
                                        baseURL: [NSURL URLWithString:@"http://videolan.org"]];
}

- (IBAction)helpGoHome:(id)sender
{
    [[o_help_web_view mainFrame] loadHTMLString: _NS(I_LONGHELP)
                                        baseURL: [NSURL URLWithString:@"http://videolan.org"]];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    /* delegate to update button states (we're the frameLoadDelegate for our help's webview)« */
    [o_help_fwd_btn setEnabled: [o_help_web_view canGoForward]];
    [o_help_bwd_btn setEnabled: [o_help_web_view canGoBack]];
}

@end
