/*****************************************************************************
 * CrashReporter.h: Mac OS X interface crash reporter
 *****************************************************************************
 * Copyright (C) 2009-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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

#import "CrashReporter.h"
#import "intf.h"
#import <AddressBook/AddressBook.h>

@implementation VLCCrashReporter

#pragma mark - init

static VLCCrashReporter *_sharedMainInstance = nil;

+ (VLCCrashReporter *)sharedInstance
{
    return _sharedMainInstance ? _sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if (_sharedMainInstance)
        [self dealloc];
    else
        _sharedMainInstance = [super init];

    return _sharedMainInstance;
}


- (void)awakeFromNib
{
    [_crashrep_send_btn setTitle: _NS("Send")];
    [_crashrep_dontSend_btn setTitle: _NS("Don't Send")];
    [_crashrep_title_txt setStringValue: _NS("VLC crashed previously")];
    [_crashrep_win setTitle: _NS("VLC crashed previously")];
    [_crashrep_desc_txt setStringValue: _NS("Do you want to send details on the crash to VLC's development team?\n\nIf you want, you can enter a few lines on what you did before VLC crashed along with other helpful information: a link to download a sample file, a URL of a network stream, ...")];
    [_crashrep_includeEmail_ckb setTitle: _NS("I agree to be possibly contacted about this bugreport.")];
    [_crashrep_includeEmail_txt setStringValue: _NS("Only your default E-Mail address will be submitted, including no further information.")];
    [_crashrep_dontaskagain_ckb setTitle: _NS("Don't ask again")];
}

- (void)dealloc
{
    [_crashLogURLConnection cancel];
    [_crashLogURLConnection release];

    [super dealloc];
}

#pragma mark - inter-object services

- (NSString *)_latestCrashLogPathPreviouslySeen:(BOOL)previouslySeen
{
    NSString * crashReporter;
    if (OSX_MOUNTAIN_LION || OSX_MAVERICKS)
        crashReporter = [@"~/Library/Logs/DiagnosticReports" stringByExpandingTildeInPath];
    else
        crashReporter = [@"~/Library/Logs/CrashReporter" stringByExpandingTildeInPath];
    NSDirectoryEnumerator *direnum = [[NSFileManager defaultManager] enumeratorAtPath:crashReporter];
    NSString *fname;
    NSString * latestLog = nil;
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    int year  = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportYear"] : 0;
    int month = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportMonth"]: 0;
    int day   = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportDay"]  : 0;
    int hours = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportHours"]: 0;

    while (fname = [direnum nextObject]) {
        [direnum skipDescendents];
        if ([fname hasPrefix:@"VLC"] && [fname hasSuffix:@"crash"]) {
            NSArray * compo = [fname componentsSeparatedByString:@"_"];
            if ([compo count] < 3)
                continue;
            compo = [[compo objectAtIndex:1] componentsSeparatedByString:@"-"];
            if ([compo count] < 4)
                continue;

            // Dooh. ugly.
            if (year < [[compo objectAtIndex:0] intValue] ||
                (year ==[[compo objectAtIndex:0] intValue] &&
                 (month < [[compo objectAtIndex:1] intValue] ||
                  (month ==[[compo objectAtIndex:1] intValue] &&
                   (day   < [[compo objectAtIndex:2] intValue] ||
                    (day   ==[[compo objectAtIndex:2] intValue] &&
                     hours < [[compo objectAtIndex:3] intValue])))))) {
                        year  = [[compo objectAtIndex:0] intValue];
                        month = [[compo objectAtIndex:1] intValue];
                        day   = [[compo objectAtIndex:2] intValue];
                        hours = [[compo objectAtIndex:3] intValue];
                        latestLog = [crashReporter stringByAppendingPathComponent:fname];
                    }
        }
    }

    if (!(latestLog && [[NSFileManager defaultManager] fileExistsAtPath:latestLog]))
        return nil;

    if (!previouslySeen) {
        [defaults setInteger:year  forKey:@"LatestCrashReportYear"];
        [defaults setInteger:month forKey:@"LatestCrashReportMonth"];
        [defaults setInteger:day   forKey:@"LatestCrashReportDay"];
        [defaults setInteger:hours forKey:@"LatestCrashReportHours"];
    }
    return latestLog;
}

- (NSString *)_latestCrashLogPath
{
    return [self _latestCrashLogPathPreviouslySeen:YES];
}

- (void)showDialogAndSendLogIfDesired
{
    // This pref key doesn't exists? this VLC is an upgrade, and this crash log come from previous version
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    BOOL areCrashLogsTooOld = ![defaults integerForKey:@"LatestCrashReportYear"];
    NSString * latestLog = [self _latestCrashLogPathPreviouslySeen:NO];
    if (latestLog && !areCrashLogsTooOld) {
        if ([defaults integerForKey:@"AlwaysSendCrashReports"] > 0)
            [self _sendCrashLog:[NSString stringWithContentsOfFile: [self _latestCrashLogPath] encoding: NSUTF8StringEncoding error: NULL] withUserComment: [_crashrep_fld string]];
        else if ([defaults integerForKey:@"AlwaysSendCrashReports"] == 0) {
            [NSBundle loadNibNamed:@"CrashReporter" owner:self];
            [NSApp runModalForWindow:_crashrep_win];
        } else {
            if ([self.delegate respondsToSelector: @selector(reporterFinishedAction:)])
                [self.delegate reporterFinishedAction: self];
        }
    }
}

#pragma mark - UI interaction

- (IBAction)buttonAction:(id)sender
{
    [NSApp stopModal];
    [_crashrep_win orderOut: sender];
    if (sender == _crashrep_send_btn) {
        [self _sendCrashLog:[NSString stringWithContentsOfFile: [self _latestCrashLogPath] encoding: NSUTF8StringEncoding error: NULL] withUserComment: [_crashrep_fld string]];
        if ([_crashrep_dontaskagain_ckb state])
            [[NSUserDefaults standardUserDefaults] setInteger:1 forKey:@"AlwaysSendCrashReports"];
    } else {
        if ([_crashrep_dontaskagain_ckb state])
            [[NSUserDefaults standardUserDefaults] setInteger:-1 forKey:@"AlwaysSendCrashReports"];
        if ([self.delegate respondsToSelector: @selector(reporterFinishedAction:)])
            [self.delegate reporterFinishedAction: self];
    }
}

#pragma mark - network handling

- (void)_sendCrashLog:(NSString *)crashLog withUserComment:(NSString *)userComment
{
    NSString *urlStr = @"http://crash.videolan.org/crashlog/sendcrashreport.php";
    NSURL *url = [NSURL URLWithString:urlStr];

    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    [req setHTTPMethod:@"POST"];

    NSString * email;
    if ([_crashrep_includeEmail_ckb state] == NSOnState) {
        ABPerson * contact = [[ABAddressBook sharedAddressBook] me];
        ABMultiValue *emails = [contact valueForProperty:kABEmailProperty];
        email = [emails valueAtIndex:[emails indexForIdentifier:
                                      [emails primaryIdentifier]]];
    }
    else
        email = [NSString string];

    NSString *postBody;
    postBody = [NSString stringWithFormat:@"CrashLog=%@&Comment=%@&Email=%@\r\n",
                [crashLog stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
                [userComment stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
                [email stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];

    [req setHTTPBody:[postBody dataUsingEncoding:NSUTF8StringEncoding]];

    /* Released from delegate */
    _crashLogURLConnection = [[NSURLConnection alloc] initWithRequest:req delegate:self];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    msg_Dbg(VLCIntf, "crash report successfully sent");
    [_crashLogURLConnection release];
    _crashLogURLConnection = nil;

    if ([self.delegate respondsToSelector: @selector(reporterFinishedAction:)])
        [self.delegate reporterFinishedAction: self];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    msg_Warn(VLCIntf, "Error when sending the crash report: %s (%li)", [[error localizedDescription] UTF8String], [error code]);
    [_crashLogURLConnection release];
    _crashLogURLConnection = nil;

    if ([self.delegate respondsToSelector: @selector(reporterFinishedAction:)])
        [self.delegate reporterFinishedAction: self];
}

@end
