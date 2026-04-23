/*****************************************************************************
 * VLCConnectToServerDialog.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan.org>
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

#import "VLCConnectToServerDialog.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryWindow.h"
#import "main/VLCMain.h"

#import <vlc_modules.h>

static NSString * const VLCConnectToServerRecentsDefaultsKey = @"VLCConnectToServerRecents";
static const NSUInteger VLCConnectToServerMaxRecents = 8;

@interface VLCConnectToServerDialog () <NSComboBoxDataSource, NSComboBoxDelegate, NSControlTextEditingDelegate>
{
    NSComboBox *_addressField;
    NSButton *_connectButton;
    NSMutableArray<NSString *> *_recents;
}
@end

@implementation VLCConnectToServerDialog

- (instancetype)init
{
    self = [super init];
    if (self) {
        NSArray<NSString *> * const stored =
            [NSUserDefaults.standardUserDefaults stringArrayForKey:VLCConnectToServerRecentsDefaultsKey];
        _recents = stored ? [stored mutableCopy] : [NSMutableArray array];
    }
    return self;
}

- (NSView *)buildAccessoryView
{
    NSView * const content = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 420, 56)];

    _addressField = [[NSComboBox alloc] initWithFrame:NSZeroRect];
    _addressField.translatesAutoresizingMaskIntoConstraints = NO;
    _addressField.placeholderString = @"smb://user@server.example.com/";
    _addressField.usesDataSource = YES;
    _addressField.dataSource = self;
    _addressField.delegate = self;
    _addressField.completes = YES;
    [content addSubview:_addressField];

    NSTextField * const hint = [NSTextField labelWithString:[self supportedSchemesHint]];
    hint.translatesAutoresizingMaskIntoConstraints = NO;
    hint.textColor = NSColor.secondaryLabelColor;
    hint.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    [content addSubview:hint];

    [NSLayoutConstraint activateConstraints:@[
        [_addressField.topAnchor constraintEqualToAnchor:content.topAnchor],
        [_addressField.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [_addressField.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],

        [hint.topAnchor constraintEqualToAnchor:_addressField.bottomAnchor constant:6],
        [hint.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [hint.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
        [hint.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
    ]];

    return content;
}

- (NSString *)supportedSchemesHint
{
    NSMutableArray<NSString *> * const schemes = [NSMutableArray array];
    if (module_exists("webdav"))
        [schemes addObjectsFromArray:@[@"webdav", @"webdavs"]];
    if (module_exists("smb2"))
        [schemes addObject:@"smb"];
    if (module_exists("ftp"))
        [schemes addObjectsFromArray:@[@"ftp", @"ftps", @"ftpes"]];
    if (module_exists("sftp"))
        [schemes addObject:@"sftp"];
    if (module_exists("nfs"))
        [schemes addObject:@"nfs"];

    return [NSString stringWithFormat:_NS("Supported protocols: %@"),
            [schemes componentsJoinedByString:@", "]];
}

- (void)show
{
    NSAlert * const alert = [[NSAlert alloc] init];
    alert.messageText = _NS("Connect to Server");
    alert.informativeText = _NS("Enter a server address to browse.");
    _connectButton = [alert addButtonWithTitle:_NS("Connect")];
    _connectButton.enabled = NO;
    [alert addButtonWithTitle:_NS("Cancel")];
    alert.accessoryView = [self buildAccessoryView];
    alert.window.initialFirstResponder = _addressField;

    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;

    NSString * const mrl = [self currentMrl];
    if (mrl == nil)
        return;

    [self rememberRecent:mrl];
    [VLCMain.sharedInstance.libraryWindow browseFolderByMrl:mrl];
}

#pragma mark - combo box data source (recent servers)

- (NSInteger)numberOfItemsInComboBox:(NSComboBox *)comboBox
{
    return _recents.count;
}

- (id)comboBox:(NSComboBox *)comboBox objectValueForItemAtIndex:(NSInteger)index
{
    return _recents[index];
}

- (NSString *)comboBox:(NSComboBox *)comboBox completedString:(NSString *)string
{
    for (NSString * const entry in _recents) {
        if ([entry.lowercaseString hasPrefix:string.lowercaseString])
            return entry;
    }
    return nil;
}

#pragma mark - validation

- (void)controlTextDidChange:(NSNotification *)notification
{
    [self updateConnectEnabled];
}

- (void)comboBoxSelectionDidChange:(NSNotification *)notification
{
    NSComboBox * const box = notification.object;
    const NSInteger index = box.indexOfSelectedItem;
    if (index >= 0 && index < (NSInteger)_recents.count)
        box.stringValue = _recents[index];
    [self updateConnectEnabled];
}

- (void)updateConnectEnabled
{
    _connectButton.enabled = [self currentMrl] != nil;
}

- (nullable NSString *)currentMrl
{
    NSString * const input = [_addressField.stringValue stringByTrimmingCharactersInSet:
                              NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (input.length == 0)
        return nil;

    NSURL * const url = [NSURL URLWithString:input];
    if (url.scheme.length == 0 || url.host.length == 0)
        return nil;

    return input;
}

#pragma mark - recent list

- (void)rememberRecent:(NSString *)mrl
{
    [_recents removeObject:mrl];
    [_recents insertObject:mrl atIndex:0];
    while (_recents.count > VLCConnectToServerMaxRecents)
        [_recents removeLastObject];

    [NSUserDefaults.standardUserDefaults setObject:_recents
                                            forKey:VLCConnectToServerRecentsDefaultsKey];
}

@end
