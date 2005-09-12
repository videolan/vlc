/*****************************************************************************
 * about.m: MacOS X About Panel
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "intf.h"
#include "about.h"

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

- (void)showPanel
{    
    if (!o_credits_path)
    {
        NSString *o_name;
        NSString *o_version;
        NSString *o_thanks_path;
		
        /* Get the info dictionary (Info.plist) */
        o_info_dict = [[NSBundle mainBundle] infoDictionary];
        
        /* Get the localized info dictionary (InfoPlist.strings) */
        localInfoBundle = CFBundleGetMainBundle();
        o_local_dict = (NSDictionary *)
                        CFBundleGetLocalInfoDictionary( localInfoBundle );
        
        /* Setup the name field */
        o_name = [o_local_dict objectForKey:@"CFBundleName"];
        
        /* Set the about box title */
        [o_about_window setTitle:_NS("About VLC media player")];
        
        /* Setup the version field */
        o_version = [o_info_dict objectForKey:@"CFBundleVersion"];
        
        /* setup the creator / revision field */
        [o_revision_field setStringValue: [NSString stringWithFormat: \
            _NS("Compiled by %s, based on SVN revision %s"), VLC_CompileBy(), \
            VLC_Changeset()]];
    
        /* Setup the nameversion field */
        o_name_version = [NSString stringWithFormat:@"%@ - Version %@", o_name, o_version];
        [o_name_version_field setStringValue: o_name_version];
        
        /* Setup our credits */
        o_credits_path = [[NSBundle mainBundle] pathForResource:@"AUTHORS" ofType:nil];
        o_credits = [[NSString alloc] initWithData: [NSData dataWithContentsOfFile: o_credits_path ] encoding:NSWindowsCP1252StringEncoding];
        
        /* Parse the authors string */
        NSMutableString *o_outString = [NSMutableString stringWithFormat: @"%@\n\n", _NS(INTF_ABOUT_MSG)];
        NSScanner *o_scan_credits = [NSScanner scannerWithString: o_credits];
        NSCharacterSet *o_stopSet = [NSCharacterSet characterSetWithCharactersInString:@"\n\r"];
        
        while( ![o_scan_credits isAtEnd] )
        {
            NSString *o_person;
            NSScanner *o_scan_person;
            
            [o_scan_credits scanUpToString:@"N:" intoString: nil];
            [o_scan_credits scanString:@"N:" intoString: nil];
            [o_scan_credits scanUpToString:@"N:" intoString: &o_person];
            o_scan_person = [NSScanner scannerWithString: o_person];
            
            NSString *o_name;
            NSString *o_email;
            NSMutableString *o_jobs = [NSMutableString string];
            NSString *o_next;

            [o_scan_person scanUpToCharactersFromSet: o_stopSet intoString: &o_name];
            [o_scan_person scanString:@"E:" intoString: nil];
            [o_scan_person scanUpToCharactersFromSet: o_stopSet intoString: &o_email];
            [o_scan_person scanUpToString:@"D:" intoString: &o_next];
            [o_scan_person scanUpToString:@":" intoString: &o_next];
            [o_scan_person scanString:@":" intoString: nil];
    
            while ( [o_next characterAtIndex:[o_next length] - 1] == 'D' )
            {
                NSString *o_job;
                [o_scan_person scanUpToCharactersFromSet: o_stopSet intoString: &o_job ];
                [o_jobs appendFormat: @"%@, ", o_job];
                [o_scan_person scanUpToString:@":" intoString: &o_next];
                [o_scan_person scanString:@":" intoString: nil];
            }
            
            [o_outString appendFormat: @"%@ <%@>\n%@\n\n", o_name, o_email, o_jobs];
        }
        
        /* Parse the thanks string */
        o_thanks_path = [[NSBundle mainBundle] pathForResource:@"THANKS" ofType:nil];
        o_thanks = [[NSString alloc] initWithData: [NSData dataWithContentsOfFile: 
                        o_thanks_path ] encoding:NSWindowsCP1252StringEncoding];
        
        NSScanner *o_scan_thanks = [NSScanner scannerWithString: o_thanks];
        [o_scan_thanks scanUpToCharactersFromSet: o_stopSet intoString: nil];
        
        while( ![o_scan_thanks isAtEnd] )
        {
            NSString *o_person;
            NSString *o_job;
            
            [o_scan_thanks scanUpToString:@" - " intoString: &o_person];
            [o_scan_thanks scanString:@" - " intoString: nil];
            [o_scan_thanks scanUpToCharactersFromSet: o_stopSet intoString: &o_job];
            [o_outString appendFormat: @"%@\n%@\n\n", o_person, o_job];
        }
        [o_credits_textview setString:o_outString];
        
        /* Setup the copyright field */
        o_copyright = [o_local_dict objectForKey:@"NSHumanReadableCopyright"];
        [o_copyright_field setStringValue:o_copyright];

        /* Setup the window */
        [o_credits_textview setDrawsBackground: NO];
        [o_credits_scrollview setDrawsBackground: NO];
        [o_about_window setExcludedFromWindowsMenu:YES];
        [o_about_window setMenu:nil];
        [o_about_window center];
    }
    
    /* Show the window */
    b_restart = YES;
    [o_about_window makeKeyAndOrderFront:nil];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    o_scroll_timer = [NSTimer scheduledTimerWithTimeInterval:1/6
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
    if (b_restart)
    {
        /* Reset the starttime */
        i_start = [NSDate timeIntervalSinceReferenceDate] + 3.0;
        f_current = 0;
        f_end = [o_credits_textview bounds].size.height - [o_credits_scrollview bounds].size.height;
        b_restart = NO;
    }

    if ([NSDate timeIntervalSinceReferenceDate] >= i_start)
    {
        /* Scroll to the position */
        [o_credits_textview scrollPoint:NSMakePoint( 0, f_current )];
        
        /* Increment the scroll position */
        f_current += 0.005;
        
        /* If at end, restart at the top */
        if ( f_current >= f_end )
        {
            b_restart = YES;
        }
    }
}

@end
