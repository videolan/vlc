/*****************************************************************************
 * update.m: MacOS X Check-For-Update window
 *****************************************************************************
 * Copyright © 2005-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Kühne <fkuehne@users.sf.net>
 *          Rafaël Carré <funman@videolanorg>
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

#import "update.h"

#ifdef UPDATE_CHECK

/*****************************************************************************
 * Preamble
 *****************************************************************************/

static NSString * kPrefUpdateOnStartup = @"UpdateOnStartup";
static NSString * kPrefUpdateLastTimeChecked = @"UpdateLastTimeChecked";

/*****************************************************************************
 * VLCExtended implementation
 *****************************************************************************/

@implementation VLCUpdate

static VLCUpdate *_o_sharedInstance = nil;

+ (VLCUpdate *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if( _o_sharedInstance ) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
        b_checked = false;

        /* clean the interface */
        [o_fld_releaseNote setString: @""];
        [o_fld_currentVersion setString: @""];
        /* translate strings to the user's language */
        [o_update_window setTitle: _NS("Check for Updates")];
        [o_btn_DownloadNow setTitle: _NS("Download now")];
        [o_btn_okay setTitle: _NS("OK")];
        [o_chk_updateOnStartup setTitle: _NS("Automatically check for updates")];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    /* we don't use - (BOOL)shouldCheckUpdateOnStartup because we don't want
     * the Alert panel to pop up at this time */
    [o_chk_updateOnStartup setState: [[NSUserDefaults standardUserDefaults] boolForKey: kPrefUpdateOnStartup]];
}

- (void)setShouldCheckUpdate: (BOOL)check
{
    [[NSUserDefaults standardUserDefaults] setBool: check forKey: kPrefUpdateOnStartup];
    [o_chk_updateOnStartup setState: check];
}

- (BOOL)shouldCheckForUpdate
{
    NSDate *o_last_update;
    NSDate *o_next_update;
 
    if( ![[NSUserDefaults standardUserDefaults] objectForKey: kPrefUpdateOnStartup] )
    {
        /* We don't have any preferences stored, ask the user. */
        int res = NSRunInformationalAlertPanel( _NS("Do you want VLC to check for updates automatically?"),
              _NS("You can change this option in VLC's update window later on."), _NS("Yes"), _NS("No"), nil );
        [self setShouldCheckUpdate: res];
    }

    if( ![[NSUserDefaults standardUserDefaults] boolForKey: kPrefUpdateOnStartup] )
        return NO;

    o_last_update = [[NSUserDefaults standardUserDefaults] objectForKey: kPrefUpdateLastTimeChecked];
    if( !o_last_update )
        return YES;

    o_next_update = [[[NSDate alloc] initWithTimeInterval: 60*60*24*2 /* every two days */ sinceDate: o_last_update] autorelease];
    if( !o_next_update )
        return YES;

    return [o_next_update compare: [NSDate date]] == NSOrderedAscending;
}

- (void)showUpdateWindow
{
    /* show the window and check for a potential update */
    [o_update_window center];
    [o_update_window displayIfNeeded];
    [o_update_window makeKeyAndOrderFront:nil];

    if( !b_checked )
    {
        [o_bar_checking startAnimation: self];
        [self checkForUpdate];
        b_checked = true;
        [o_bar_checking stopAnimation: self];
    }
}

- (IBAction)download:(id)sender
{
    /* provide a save dialogue */
    SEL sel = @selector(getLocationForSaving:returnCode:contextInfo:);
    NSSavePanel * saveFilePanel = [[NSSavePanel alloc] init];

    [saveFilePanel setRequiredFileType: @"dmg"];
    [saveFilePanel setCanSelectHiddenExtension: YES];
    [saveFilePanel setCanCreateDirectories: YES];
    [saveFilePanel beginSheetForDirectory:nil file:
        [[[NSString stringWithUTF8String: p_u->release.psz_url] componentsSeparatedByString:@"/"] lastObject]
                           modalForWindow: o_update_window 
                            modalDelegate:self
                           didEndSelector:sel
                              contextInfo:nil];
}

- (void)getLocationForSaving: (NSSavePanel *)sheet 
                  returnCode: (int)returnCode 
                 contextInfo: (void *)contextInfo
{
    if( returnCode == NSOKButton )
    {
        /* perform download and pass the selected path */
        [self performDownload: [sheet filename]];
    }
    [sheet release];
}

- (IBAction)okay:(id)sender
{
    /* just hides the window */
    [o_update_window orderOut: self];
}

- (IBAction)changeCheckUpdateOnStartup:(id)sender
{
    [self setShouldCheckUpdate: [sender state]];
}

- (void)setUpToDate:(BOOL)uptodate
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    if( uptodate )
    {
        [o_fld_releaseNote setString: @""];
        [o_fld_currentVersion setStringValue: @""];
        [o_fld_status setStringValue: _NS("This version of VLC is the latest available.")];
        [o_btn_DownloadNow setEnabled: NO];
    }
    else
    {
        [o_fld_releaseNote setString: [NSString stringWithUTF8String: (p_u->release.psz_desc ? p_u->release.psz_desc : "" )]];
        [o_fld_status setStringValue: _NS("This version of VLC is outdated.")];
        [o_fld_currentVersion setStringValue: [NSString stringWithFormat:
            _NS("The current release is %d.%d.%d%c."), p_u->release.i_major,
            p_u->release.i_minor, p_u->release.i_revision, p_u->release.extra]];
        [o_btn_DownloadNow setEnabled: YES];
        /* Make sure the update window is showed in case we have something */
        [o_update_window center];
        [o_update_window displayIfNeeded];
        [o_update_window makeKeyAndOrderFront: self];
    }

    [pool release];
}

static void updateCallback( void * p_data, bool b_success )
{
    [(id)p_data setUpToDate: !b_success || !update_NeedUpgrade( ((VLCUpdate*)p_data)->p_u )];
}

- (void)checkForUpdate
{
    p_u = update_New( VLCIntf );
    if( !p_u )
        return;
    update_Check( p_u, updateCallback, self );

    [[NSUserDefaults standardUserDefaults] setObject: [NSDate date] forKey: kPrefUpdateLastTimeChecked];
}

- (void)performDownload:(NSString *)path
{
    update_Download( p_u, [path UTF8String] );
    [o_btn_DownloadNow setEnabled: NO];
    [o_update_window orderOut: self];
    update_Delete( p_u );
}

@end

#endif
