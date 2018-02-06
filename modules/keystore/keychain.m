/*****************************************************************************
 * keychain.m: Darwin Keychain keystore module
 *****************************************************************************
 * Copyright © 2016 VLC authors, VideoLAN and VideoLabs
 *
 * Author: Felix Paul Kühne <fkuehne # videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_keystore.h>

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Cocoa/Cocoa.h>

static int Open(vlc_object_t *);

static const int sync_list[] =
{ 0, 1, 2 };
static const char *const sync_list_text[] = {
    N_("Yes"), N_("No"), N_("Any")
};

static const int accessibility_list[] =
{ 0, 1, 2, 3, 4, 5, 6, 7 };
static const char *const accessibility_list_text[] = {
    N_("System default"),
    N_("After first unlock"),
    N_("After first unlock, on this device only"),
    N_("Always"),
    N_("When passcode set, on this device only"),
    N_("Always, on this device only"),
    N_("When unlocked"),
    N_("When unlocked, on this device only")
};

#define SYNC_ITEMS_TEXT N_("Synchronize stored items")
#define SYNC_ITEMS_LONGTEXT N_("Synchronizes stored items via iCloud Keychain if enabled in the user domain.")

#define ACCESSIBILITY_TYPE_TEXT N_("Accessibility type for all future passwords saved to the Keychain")

#define ACCESS_GROUP_TEXT N_("Keychain access group")
#define ACCESS_GROUP_LONGTEXT N_("Keychain access group as defined by the app entitlements.")

/* VLC can be compiled against older SDKs (like before OS X 10.10)
 * but newer features should still be available.
 * Hence, re-define things as needed */
#ifndef kSecAttrSynchronizable
#define kSecAttrSynchronizable CFSTR("sync")
#endif

#ifndef kSecAttrSynchronizableAny
#define kSecAttrSynchronizableAny CFSTR("syna")
#endif

#ifndef kSecAttrAccessGroup
#define kSecAttrAccessGroup CFSTR("agrp")
#endif

#ifndef kSecAttrAccessibleAfterFirstUnlock
#define kSecAttrAccessibleAfterFirstUnlock CFSTR("ck")
#endif

#ifndef kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
#define kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly CFSTR("cku")
#endif

#ifndef kSecAttrAccessibleAlways
#define kSecAttrAccessibleAlways CFSTR("dk")
#endif

#ifndef kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly
#define kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly CFSTR("akpu")
#endif

#ifndef kSecAttrAccessibleAlwaysThisDeviceOnly
#define kSecAttrAccessibleAlwaysThisDeviceOnly CFSTR("dku")
#endif

#ifndef kSecAttrAccessibleWhenUnlocked
#define kSecAttrAccessibleWhenUnlocked CFSTR("ak")
#endif

#ifndef kSecAttrAccessibleWhenUnlockedThisDeviceOnly
#define kSecAttrAccessibleWhenUnlockedThisDeviceOnly CFSTR("aku")
#endif

vlc_module_begin()
    set_shortname(N_("Keychain keystore"))
    set_description(N_("Keystore for iOS, Mac OS X and tvOS"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    add_integer("keychain-synchronize", 1, SYNC_ITEMS_TEXT, SYNC_ITEMS_LONGTEXT, true)
    change_integer_list(sync_list, sync_list_text)
    add_integer("keychain-accessibility-type", 0, ACCESSIBILITY_TYPE_TEXT, ACCESSIBILITY_TYPE_TEXT, true)
    change_integer_list(accessibility_list, accessibility_list_text)
    add_string("keychain-access-group", NULL, ACCESS_GROUP_TEXT, ACCESS_GROUP_LONGTEXT, true)
    set_capability("keystore", 100)
    set_callbacks(Open, NULL)
vlc_module_end ()

static NSMutableDictionary * CreateQuery(vlc_keystore *p_keystore)
{
    NSMutableDictionary *dictionary = [NSMutableDictionary dictionaryWithCapacity:3];
    [dictionary setObject:(__bridge id)kSecClassInternetPassword forKey:(__bridge id)kSecClass];

    [dictionary setObject:@"VLC-Password-Service" forKey:(__bridge id)kSecAttrService];

    const char * psz_access_group = var_InheritString(p_keystore, "keychain-access-group");
    if (psz_access_group) {
        [dictionary setObject:[NSString stringWithUTF8String:psz_access_group] forKey:(__bridge id)kSecAttrAccessGroup];
    }

    id syncValue;
    int syncMode = var_InheritInteger(p_keystore, "keychain-synchronize");

    if (syncMode == 2) {
        syncValue = (__bridge id)kSecAttrSynchronizableAny;
    } else if (syncMode == 0) {
        syncValue = @(YES);
    } else {
        syncValue = @(NO);
    }

    [dictionary setObject:syncValue forKey:(__bridge id)(kSecAttrSynchronizable)];

    return dictionary;
}

static NSString * ErrorForStatus(OSStatus status)
{
    NSString *message = nil;

    switch (status) {
#if TARGET_OS_IPHONE
        case errSecUnimplemented: {
            message = @"Query unimplemented";
            break;
        }
        case errSecParam: {
            message = @"Faulty parameter";
            break;
        }
        case errSecAllocate: {
            message = @"Allocation failure";
            break;
        }
        case errSecNotAvailable: {
            message = @"Query not available";
            break;
        }
        case errSecDuplicateItem: {
            message = @"Duplicated item";
            break;
        }
        case errSecItemNotFound: {
            message = @"Item not found";
            break;
        }
        case errSecInteractionNotAllowed: {
            message = @"Interaction not allowed";
            break;
        }
        case errSecDecode: {
            message = @"Decoding failure";
            break;
        }
        case errSecAuthFailed: {
            message = @"Authentication failure";
            break;
        }
        case -34018: {
            message = @"iCloud Keychain failure";
            break;
        }
        default: {
            message = @"Unknown generic error";
        }
#else
        default:
            message = (__bridge_transfer NSString *)SecCopyErrorMessageString(status, NULL);
#endif
    }

    return message;
}

#define OSX_MAVERICKS (NSAppKitVersionNumber >= 1265)
extern const CFStringRef kSecAttrAccessible;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
static void SetAccessibilityForQuery(vlc_keystore *p_keystore,
                                     NSMutableDictionary *query)
{
    if (!OSX_MAVERICKS)
	return;

    int accessibilityType = var_InheritInteger(p_keystore, "keychain-accessibility-type");
    switch (accessibilityType) {
        case 1:
            [query setObject:(__bridge id)kSecAttrAccessibleAfterFirstUnlock forKey:(__bridge id)kSecAttrAccessible];
            break;
        case 2:
            [query setObject:(__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly forKey:(__bridge id)kSecAttrAccessible];
            break;
        case 3:
            [query setObject:(__bridge id)kSecAttrAccessibleAlways forKey:(__bridge id)kSecAttrAccessible];
            break;
        case 4:
            [query setObject:(__bridge id)kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly forKey:(__bridge id)kSecAttrAccessible];
            break;
        case 5:
            [query setObject:(__bridge id)kSecAttrAccessibleAlwaysThisDeviceOnly forKey:(__bridge id)kSecAttrAccessible];
            break;
        case 6:
            [query setObject:(__bridge id)kSecAttrAccessibleWhenUnlocked forKey:(__bridge id)kSecAttrAccessible];
            break;
        case 7:
            [query setObject:(__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly forKey:(__bridge id)kSecAttrAccessible];
            break;
        default:
            break;
    }
}
#pragma clang diagnostic pop

static void SetAttributesForQuery(const char *const ppsz_values[KEY_MAX], NSMutableDictionary *query, const char *psz_label)
{
    const char *psz_protocol = ppsz_values[KEY_PROTOCOL];
    const char *psz_user = ppsz_values[KEY_USER];
    const char *psz_server = ppsz_values[KEY_SERVER];
    const char *psz_path = ppsz_values[KEY_PATH];
    const char *psz_port = ppsz_values[KEY_PORT];

    if (psz_label) {
        [query setObject:[NSString stringWithUTF8String:psz_label] forKey:(__bridge id)kSecAttrLabel];
    }
    if (psz_protocol) {
        [query setObject:[NSString stringWithUTF8String:psz_protocol] forKey:(__bridge id)kSecAttrProtocol];
    }
    if (psz_user) {
        [query setObject:[NSString stringWithUTF8String:psz_user] forKey:(__bridge id)kSecAttrAccount];
    }
    if (psz_server) {
        [query setObject:[NSString stringWithUTF8String:psz_server] forKey:(__bridge id)kSecAttrServer];
    }
    if (psz_path) {
        [query setObject:[NSString stringWithUTF8String:psz_path] forKey:(__bridge id)kSecAttrPath];
    }
    if (psz_port) {
        [query setObject:[NSString stringWithUTF8String:psz_port] forKey:(__bridge id)kSecAttrPort];
    }
}

static int CopyEntryValues(const char * ppsz_dst[KEY_MAX], const char *const ppsz_src[KEY_MAX])
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (ppsz_src[i])
        {
            ppsz_dst[i] = strdup(ppsz_src[i]);
            if (!ppsz_dst[i])
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static int Store(vlc_keystore *p_keystore,
                 const char *const ppsz_values[KEY_MAX],
                 const uint8_t *p_secret,
                 size_t i_secret_len,
                 const char *psz_label)
{
    OSStatus status;

    if (!ppsz_values[KEY_PROTOCOL] || !p_secret) {
        return VLC_EGENERIC;
    }

    NSMutableDictionary *query = nil;
    NSMutableDictionary *searchQuery = CreateQuery(p_keystore);

    /* set attributes */
    SetAttributesForQuery(ppsz_values, searchQuery, psz_label);

    /* search */
    status = SecItemCopyMatching((__bridge CFDictionaryRef)searchQuery, nil);

    /* create storage unit */
    NSData *secretData = [[NSString stringWithFormat:@"%s", p_secret] dataUsingEncoding:NSUTF8StringEncoding];

    if (status == errSecSuccess) {
        /* item already existed in keychain, let's update */
        query = [[NSMutableDictionary alloc] init];

        /* just set the secret data */
        [query setObject:secretData forKey:(__bridge id)kSecValueData];

        status = SecItemUpdate((__bridge CFDictionaryRef)(searchQuery), (__bridge CFDictionaryRef)(query));
    } else if (status == errSecItemNotFound) {
        /* item not found, let's create! */
        query = CreateQuery(p_keystore);

        /* set attributes */
        SetAttributesForQuery(ppsz_values, query, psz_label);

        /* set accessibility */
        SetAccessibilityForQuery(p_keystore, query);

        /* set secret data */
        [query setObject:secretData forKey:(__bridge id)kSecValueData];

        status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
    }
    if (status != errSecSuccess) {
        msg_Err(p_keystore, "Storage failed (%i: '%s')", status, [ErrorForStatus(status) UTF8String]);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static unsigned int Find(vlc_keystore *p_keystore,
                         const char *const ppsz_values[KEY_MAX],
                         vlc_keystore_entry **pp_entries)
{
    CFTypeRef result = NULL;

    NSMutableDictionary *query = CreateQuery(p_keystore);
    [query setObject:@(YES) forKey:(__bridge id)kSecReturnRef];
    [query setObject:(__bridge id)kSecMatchLimitAll forKey:(__bridge id)kSecMatchLimit];

    /* set attributes */
    SetAttributesForQuery(ppsz_values, query, NULL);

    /* search */
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if (status != errSecSuccess) {
        msg_Warn(p_keystore, "lookup failed (%i: '%s')", status, [ErrorForStatus(status) UTF8String]);
        return 0;
    }

    NSArray *listOfResults = (__bridge_transfer NSArray *)result;

    NSUInteger count = listOfResults.count;

    vlc_keystore_entry *p_entries = calloc(count,
                                           sizeof(vlc_keystore_entry));
    if (!p_entries)
        return 0;

    for (NSUInteger i = 0; i < count; i++) {
        vlc_keystore_entry *p_entry = &p_entries[i];
        if (CopyEntryValues((const char **)p_entry->ppsz_values, (const char *const*)ppsz_values) != VLC_SUCCESS) {
            vlc_keystore_release_entries(p_entries, 1);
            return 0;
        }

        SecKeychainItemRef itemRef = (__bridge SecKeychainItemRef)([listOfResults objectAtIndex:i]);

        SecKeychainAttributeInfo attrInfo;
        attrInfo.count = 0;

#ifndef NDEBUG
        attrInfo.count = 1;
        UInt32 tags[1] = {kSecAccountItemAttr}; //, kSecAccountItemAttr, kSecServerItemAttr, kSecPortItemAttr, kSecProtocolItemAttr, kSecPathItemAttr};
        attrInfo.tag = tags;
        attrInfo.format = NULL;
#endif

        SecKeychainAttributeList *attrList = NULL;

        UInt32 dataLength;
        void * data;

        status = SecKeychainItemCopyAttributesAndData(itemRef, &attrInfo, NULL, &attrList, &dataLength, &data);

        if (status != noErr) {
            msg_Err(p_keystore, "Lookup error: %i (%s)", status, [ErrorForStatus(status) UTF8String]);
            vlc_keystore_release_entries(p_entries, count);
            return 0;
        }

#ifndef NDEBUG
        for (unsigned x = 0; x < attrList->count; x++) {
            SecKeychainAttribute *attr = &attrList->attr[i];
            switch (attr->tag) {
                case kSecLabelItemAttr:
                    NSLog(@"label %@", [[NSString alloc] initWithBytes:attr->data length:attr->length encoding:NSUTF8StringEncoding]);
                    break;
                case kSecAccountItemAttr:
                    NSLog(@"account %@", [[NSString alloc] initWithBytes:attr->data length:attr->length encoding:NSUTF8StringEncoding]);
                    break;
                default:
                    break;
            }
        }
#endif

        /* we need to do some padding here, as string is expected to be 0 terminated */
        uint8_t *retData = calloc(1, dataLength + 1);
        memcpy(retData, data, dataLength);

        vlc_keystore_entry_set_secret(p_entry, retData, dataLength + 1);

        free(retData);
        SecKeychainItemFreeAttributesAndData(attrList, data);
    }

    *pp_entries = p_entries;

    return count;
}

static unsigned int Remove(vlc_keystore *p_keystore,
                           const char *const ppsz_values[KEY_MAX])
{
    OSStatus status;

    NSMutableDictionary *query = CreateQuery(p_keystore);

    SetAttributesForQuery(ppsz_values, query, NULL);

    CFTypeRef result = NULL;
    [query setObject:@(YES) forKey:(__bridge id)kSecReturnRef];
    [query setObject:(__bridge id)kSecMatchLimitAll forKey:(__bridge id)kSecMatchLimit];

    BOOL failed = NO;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    NSUInteger matchCount = 0;

    if (status == errSecSuccess) {
        NSArray *matches = (__bridge_transfer NSArray *)result;
        matchCount = matches.count;

        for (NSUInteger x = 0; x < matchCount; x++) {
            status = SecKeychainItemDelete((__bridge SecKeychainItemRef _Nonnull)([matches objectAtIndex:x]));
            if (status != noErr) {
                msg_Err(p_keystore, "Deletion error %i (%s)", status , [ErrorForStatus(status) UTF8String]);
                failed = YES;
            }
        }
    } else {
        msg_Err(p_keystore, "Lookup error for deletion %i (%s)", status, [ErrorForStatus(status) UTF8String]);
        return VLC_EGENERIC;
    }

    if (failed)
        return VLC_EGENERIC;

    return matchCount;
}

static int Open(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;

    p_keystore->p_sys = NULL;
    p_keystore->pf_store = Store;
    p_keystore->pf_find = Find;
    p_keystore->pf_remove = Remove;

    return VLC_SUCCESS;
}
