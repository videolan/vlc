/*****************************************************************************
 * about.m: MacOS X About Panel
 *****************************************************************************
 * Copyright (C) 2001-2012 VLC authors and VideoLAN
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

#ifdef __x86_64__
#define PLATFORM "Intel 64bit"
#elif __i386__
#define PLATFORM "Intel 32bit"
#else
#define PLATFORM "PowerPC 32bit"
#endif

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
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void) dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [o_color_backdrop release];
    [super dealloc];
}

- (void)awakeFromNib
{
    if (OSX_LION)
        [o_about_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    /* add a colored backdrop to get a white window background */
    o_color_backdrop = [[VLAboutColoredBackdrop alloc] initWithFrame: [[o_about_window contentView] frame]];
    [[o_about_window contentView] addSubview: o_color_backdrop positioned: NSWindowBelow relativeTo: nil];
}

/*****************************************************************************
* VLC About Window
*****************************************************************************/

- (void)showAbout
{
    if(! b_isSetUp )
    {
        /* Get the localized info dictionary (InfoPlist.strings) */
        NSDictionary *o_local_dict;
        o_local_dict = [[NSBundle mainBundle] localizedInfoDictionary];

        /* Setup the copyright field */
        [o_copyright_field setStringValue: [o_local_dict objectForKey:@"NSHumanReadableCopyright"]];

        /* Set the box title */
        [o_about_window setTitle: _NS("About VLC media player")];

        /* setup the creator / revision field */
        NSString *compiler;
#ifdef __clang__
        compiler = [NSString stringWithFormat:@"clang %s", __clang_version__];
#elif __llvm__
        compiler = [NSString stringWithFormat:@"llvm-gcc %s", __VERSION__];
#else
        compiler = [NSString stringWithFormat:@"gcc %s", __VERSION__];
#endif
        [o_revision_field setStringValue: [NSString stringWithFormat: _NS("Compiled by %@ with %@"), [NSString stringWithUTF8String:VLC_CompileBy()], compiler]];

        /* Setup the nameversion field */
        [o_name_version_field setStringValue: [NSString stringWithFormat:@"Version %s (%s)", VERSION_MESSAGE, PLATFORM]];

        NSMutableArray *tmpArray = [NSMutableArray arrayWithArray: [[NSString stringWithUTF8String: psz_authors]componentsSeparatedByString:@"\n\n"]];
        NSUInteger count = [tmpArray count];
        for( NSUInteger i = 0; i < count; i++ )
        {
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@"\n" withString:@", "]];
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@", -" withString:@"\n-" options:0 range:NSRangeFromString(@"0 30")]];
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByReplacingOccurrencesOfString:@"-, " withString:@"-\n" options:0 range:NSRangeFromString(@"0 30")]];
            [tmpArray replaceObjectAtIndex:i withObject:[[tmpArray objectAtIndex:i]stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@","]]];
        }
        NSString *authors = [tmpArray componentsJoinedByString:@"\n\n"];

        /* setup the authors and thanks field */
        [o_credits_textview setString: [NSString stringWithFormat: @"%@\n\n\n\n\n\n%@\n\n%@\n\n",
                                        [_NS(INTF_ABOUT_MSG) stringByReplacingOccurrencesOfString:@"\n" withString:@" "],
                                        authors,
                                        [[NSString stringWithUTF8String: psz_thanks] stringByReplacingOccurrencesOfString:@"\n" withString:@" " options:0 range:NSRangeFromString(@"680 2")]]];

        /* Setup the window */
        [o_credits_textview setDrawsBackground: NO];
        [o_credits_scrollview setDrawsBackground: NO];
        [o_about_window setExcludedFromWindowsMenu:YES];
        [o_about_window setMenu:nil];
        [o_about_window center];
        [o_gpl_btn setTitle: _NS("License")];

        b_isSetUp = YES;
    }

    /* Show the window */
    b_restart = YES;
    [o_credits_textview scrollPoint:NSMakePoint( 0, 0 )];
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
    if( b_restart )
    {
        /* Reset the starttime */
        i_start = [NSDate timeIntervalSinceReferenceDate] + 4.0;
        f_current = 0;
        f_end = [o_credits_textview bounds].size.height - [o_credits_scrollview bounds].size.height;
        b_restart = NO;
    }

    if( [NSDate timeIntervalSinceReferenceDate] >= i_start )
    {
        /* Increment the scroll position */
        f_current += 0.005;

        /* Scroll to the position */
        [o_credits_textview scrollPoint:NSMakePoint( 0, f_current )];

        /* If at end, restart at the top */
        if( f_current >= f_end )
        {
            /* f_end may be wrong on first run, so don't trust it too much */
            if( f_end == [o_credits_textview bounds].size.height - [o_credits_scrollview bounds].size.height )
            {
                b_restart = YES;
                [o_credits_textview scrollPoint:NSMakePoint( 0, 0 )];
            }
            else
                f_end = [o_credits_textview bounds].size.height - [o_credits_scrollview bounds].size.height;
        }
    }
}

/*****************************************************************************
* VLC GPL Window, action called from the about window and the help menu
*****************************************************************************/

- (IBAction)showGPL:(id)sender
{
    [o_gpl_window setTitle: _NS("License")];
    [o_gpl_field setString: [NSString stringWithUTF8String: psz_license]];

    [o_gpl_window center];
    [o_gpl_window makeKeyAndOrderFront: sender];
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

@implementation VLAboutColoredBackdrop

- (void)drawRect:(NSRect)rect {
    [[NSColor whiteColor] setFill];
    NSRectFill(rect);
}

@end
