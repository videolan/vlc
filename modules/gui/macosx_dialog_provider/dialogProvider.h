/*****************************************************************************
 * dialogProvider.h: Minimal Dialog Provider for Mac OS X
 *****************************************************************************
 * Copyright (C) 2009-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan dot>
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

#import <Cocoa/Cocoa.h>

#import <vlc_interface.h>

#import "VLCLoginPanel.h"
#import "VLCProgressPanel.h"

@interface VLCDialogDisplayer : NSObject
{
    intf_thread_t *p_intf;
    VLCProgressPanel *_currentProgressBarPanel;
}
- (intf_thread_t *)intf;
- (void)setIntf:(intf_thread_t *)p_mainintf;
- (void)globalNotificationReceived: (NSNotification *)theNotification;

+ (NSDictionary *)dictionaryForDialog:(const char *)title :(const char *)message :(const char *)yes :(const char *)no :(const char *)cancel;

- (void)displayError:(NSDictionary *)dialog;
- (void)displayCritical:(NSDictionary *)dialog;
- (NSNumber *)displayQuestion:(NSDictionary *)dialog;
- (NSDictionary *)displayLogin:(NSDictionary *)dialog;

- (void)displayProgressBar:(NSDictionary *)dict;
- (void)updateProgressPanel:(NSDictionary *)dict;
- (void)destroyProgressPanel;
- (NSNumber *)checkProgressPanel;

- (void)updateExtensionDialog:(NSValue *)extensionDialog;

- (id)resultFromSelectorOnMainThread:(SEL)sel withObject:(id)object;
@end
