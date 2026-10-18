/*****************************************************************************
 * VLCMacOSXTestDefaults.m: in-memory preferences for macOS UI tests
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCMacOSXTestDefaults.h"

@interface VLCMacOSXTestDefaults ()
@property (nonatomic, strong) NSMutableDictionary<NSString *, id> *values;
@end

@implementation VLCMacOSXTestDefaults

- (instancetype)init
{
    self = [super initWithSuiteName:@"org.videolan.VLCMacOSXTests"];
    if (self) {
        _values = NSMutableDictionary.dictionary;
    }
    return self;
}

- (id)objectForKey:(NSString *)key
{
    return self.values[key];
}

- (NSArray<NSString *> *)stringArrayForKey:(NSString *)key
{
    id value = [self objectForKey:key];
    if (![value isKindOfClass:NSArray.class]) {
        return nil;
    }
    for (id element in value) {
        if (![element isKindOfClass:NSString.class]) {
            return nil;
        }
    }
    return value;
}

- (NSString *)stringForKey:(NSString *)key
{
    id value = [self objectForKey:key];
    return [value isKindOfClass:NSString.class] ? value : nil;
}

- (BOOL)boolForKey:(NSString *)key
{
    return [[self objectForKey:key] boolValue];
}

- (NSInteger)integerForKey:(NSString *)key
{
    return [[self objectForKey:key] integerValue];
}

- (float)floatForKey:(NSString *)key
{
    return [[self objectForKey:key] floatValue];
}

- (double)doubleForKey:(NSString *)key
{
    return [[self objectForKey:key] doubleValue];
}

- (void)setObject:(id)value forKey:(NSString *)key
{
    self.values[key] = value;
}

- (void)removeObjectForKey:(NSString *)key
{
    [self.values removeObjectForKey:key];
}

- (void)registerDefaults:(NSDictionary<NSString *, id> *)values
{
    for (NSString *key in values) {
        if (self.values[key] == nil) {
            self.values[key] = values[key];
        }
    }
}

@end
