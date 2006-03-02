/*****************************************************************************
 * update.m: MacOS X Check-For-Update window
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Felix KŸhne <fkuehne@users.sf.net>
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
 * Note: the code used to communicate with VLC's core was inspired by 
 * ../wxwidgets/dialogs/updatevlc.cpp, written by Antoine Cellerier. 
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import "update.h"
#import "intf.h"


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
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    /* get up */
    p_intf = VLCIntf;

    /* clean the interface */
    [o_fld_releaseNote setString: @""];
    
    [self initStrings];
}

- (void)dealloc
{
    if( o_urlOfBinary )
        [o_urlOfBinary release];

    [super dealloc];
}

- (void)initStrings
{
    /* translate strings to the user's language */
    [o_update_window setTitle: _NS("Check for Update")];
    [o_btn_DownloadNow setTitle: _NS("Download now")];
    [o_btn_okay setTitle: _NS("OK")];
}

- (void)showUpdateWindow
{
    /* show the window and check for a potential update */
    [o_fld_status setStringValue: _NS("Checking for Update...")];
    [o_fld_currentVersionAndSize setStringValue: @""];
    [o_fld_releaseNote setString: @""];

    [o_update_window center];
    [o_update_window displayIfNeeded];
    [o_update_window makeKeyAndOrderFront:nil];

    [o_bar_checking startAnimation: self];
    [self checkForUpdate];
    [o_bar_checking stopAnimation: self];
}

- (IBAction)download:(id)sender
{
    /* provide a save dialogue */
    SEL sel = @selector(getLocationForSaving:returnCode:contextInfo:);
    NSSavePanel * saveFilePanel = [[NSSavePanel alloc] init];
    
    [saveFilePanel setRequiredFileType: @"dmg"];
    [saveFilePanel setCanSelectHiddenExtension: YES];
    [saveFilePanel setCanCreateDirectories: YES];
    [saveFilePanel beginSheetForDirectory:nil file: \
        [[o_urlOfBinary componentsSeparatedByString:@"/"] lastObject] \
        modalForWindow: o_update_window modalDelegate:self didEndSelector:sel \
        contextInfo:nil];
}

- (void)getLocationForSaving: (NSSavePanel *)sheet returnCode: \
    (int)returnCode contextInfo: (void *)contextInfo
{
    if (returnCode == NSOKButton)
    {
        /* perform download and pass the selected path */
        [self performDownload: [sheet filename]];
    }
    [sheet release];
}

- (IBAction)okay:(id)sender
{
    /* just close the window */
    [o_update_window close];
}

- (void)checkForUpdate
{
    p_u = update_New( p_intf );
    update_Check( p_u, VLC_FALSE );
    update_iterator_t *p_uit = update_iterator_New( p_u );
    BOOL releaseChecked = NO;
    int x = 0;
    NSString * pathToReleaseNote;
    pathToReleaseNote = [NSString stringWithFormat: \
        @"/tmp/vlc_releasenote_%d.tmp", mdate()]; /*[[NSCalendarDate calendarDate] \
        descriptionWithCalendarFormat: @"%m-%d-%y--%I.%M.%S.%F"]];*/
    NSLog( pathToReleaseNote );
    
    if( p_uit )
    {
        p_uit->i_rs = UPDATE_RELEASE_STATUS_NEWER;
        p_uit->i_t = UPDATE_FILE_TYPE_ALL;
        update_iterator_Action( p_uit, UPDATE_MIRROR );
        
        while( update_iterator_Action( p_uit, UPDATE_FILE) != UPDATE_FAIL )
        {
            /* if the announced item is of the type "binary", keep it and display
             * its details to the user. Do similar stuff on "info". Do both 
             * only if the file is announced as stable */
            if( p_uit->release.i_type == UPDATE_RELEASE_TYPE_STABLE )
            {
                if( p_uit->file.i_type == UPDATE_FILE_TYPE_INFO )
                {
                    [o_fld_releaseNote setString: \
                        [NSString stringWithUTF8String: \
                        (p_uit->file.psz_description)]];
                    /* download our release note
                     * We will read the temp file after this loop */
                    update_download( p_uit, (char *)[pathToReleaseNote UTF8String] );
                }
                else if( p_uit->file.i_type == UPDATE_FILE_TYPE_BINARY )
                {
                    msg_Dbg( p_intf, "binary found, version = %s, " \
                        "url=%s, size=%i MB", p_uit->release.psz_version, \
                        p_uit->file.psz_url, \
                        (int)((p_uit->file.l_size / 1024) / 1024) );
                    [o_fld_currentVersionAndSize setStringValue: [NSString \
                        stringWithFormat: \
                        _NS("The current release is %s (%i MB to download)."), \
                        p_uit->release.psz_version, ((p_uit->file.l_size \
                        / 1024) / 1024)]];
                        
                    if( o_urlOfBinary )
                        [o_urlOfBinary release];
                    o_urlOfBinary = [[NSString alloc] initWithUTF8String: \
                        p_uit->file.psz_url];
                }
                if( p_uit->release.i_status == UPDATE_RELEASE_STATUS_NEWER &&
                    !releaseChecked )
                {
                    /* our version is outdated, let the user download the new
                     * release */
                    [o_fld_status setStringValue: _NS("Your version of VLC " \
                        "is outdated.")];
                    [o_btn_DownloadNow setEnabled: YES];
                    msg_Dbg( p_intf, "this version of VLC is outdated" );
                    /* put the mirror information */
                    msg_Dbg( p_intf, "used mirror: %s, %s [%s]", \
                            p_uit->mirror.psz_name, p_uit->mirror.psz_location,\
                            p_uit->mirror.psz_type );
                    /* make sure that we perform this check only once */
                    releaseChecked = YES;
                }
                else if(! releaseChecked )
                {
                    [o_fld_status setStringValue: _NS("Your version of VLC " \
                        "is up-to-date.")];
                    [o_btn_DownloadNow setEnabled: NO];
                    msg_Dbg( p_intf, "current version is up-to-date" );
                    releaseChecked = YES;
                }
            }
            x += 1;
        }

        update_iterator_Delete( p_uit );
        
        /* wait for our download, since it is done by another thread
         * this does usually take 300000 to 500000 ms */
        int i = 0;
        while( [[NSFileManager defaultManager] fileExistsAtPath: pathToReleaseNote] == NO )
        {
            msleep( 100000 );
            i += 1;
            if( i == 600 )
            {
                /* if this takes more than 1 min, exit */
                msg_Warn( p_intf, "download took more than a minute, exiting" );
                return;
            }
        }
        msg_Dbg( p_intf, "waited %i ms for the release notes", (i * 100000) );
        msleep( 500000 );

        /* let's open our cached release note and display it
         * we can't use NSString stringWithContentsOfFile:encoding:error: 
         * since it is Tiger only */
        NSString * releaseNote = [[NSString alloc] initWithData: \
            [NSData dataWithContentsOfFile: pathToReleaseNote] \
            encoding: NSISOLatin1StringEncoding];
        if( releaseNote )
            [o_fld_releaseNote setString: releaseNote];

        /* delete the file since it isn't needed anymore */
        BOOL myBOOL = NO;
        myBOOL = [[NSFileManager defaultManager] removeFileAtPath: \
            pathToReleaseNote handler: nil];
    }
}

- (void)performDownload:(NSString *)path
{
    update_iterator_t *p_uit = update_iterator_New( p_u );
    if( p_uit )
    {
        update_iterator_Action( p_uit, UPDATE_MIRROR );

        while( update_iterator_Action( p_uit, UPDATE_FILE) != UPDATE_FAIL )
        {
            if( p_uit->release.i_type == UPDATE_RELEASE_TYPE_STABLE &&
                p_uit->release.i_status == UPDATE_RELEASE_STATUS_NEWER &&
                p_uit->file.i_type == UPDATE_FILE_TYPE_BINARY )
            {
                /* put the mirror information */
                msg_Dbg( p_intf, "used mirror: %s, %s [%s]", \
                    p_uit->mirror.psz_name, p_uit->mirror.psz_location, \
                    p_uit->mirror.psz_type );

                /* that's our binary */
                update_download( p_uit, (char *)[path UTF8String] );
            }
        }
        
        update_iterator_Delete( p_uit );
    }

    [o_update_window close];
}

@end
