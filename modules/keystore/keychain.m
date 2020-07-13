/*****************************************************************************
 * keychain.m: Darwin Keychain keystore module
 *****************************************************************************
 * Copyright © 2016, 2018 VLC authors, VideoLAN and VideoLabs
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

#include "list_util.h"

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <Security/Security.h>

// Marker to recognize changed format in vlc 4: secret does not have \0 cut off anymore.
int kVlc4Creator = 'vlc4';

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


vlc_module_begin()
    set_shortname(N_("Keychain keystore"))
    set_description(N_("Keystore for iOS, macOS and tvOS"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    add_integer("keychain-synchronize", 1, SYNC_ITEMS_TEXT, SYNC_ITEMS_LONGTEXT, true)
    change_integer_list(sync_list, sync_list_text)
    add_integer("keychain-accessibility-type", 0, ACCESSIBILITY_TYPE_TEXT, ACCESSIBILITY_TYPE_TEXT, true)
    change_integer_list(accessibility_list, accessibility_list_text)
    add_string("keychain-access-group", NULL, ACCESS_GROUP_TEXT, ACCESS_GROUP_LONGTEXT, true)
    set_capability("keystore", 100)
    set_callback(Open)
vlc_module_end ()

static NSMutableDictionary * CreateQuery(vlc_keystore *p_keystore)
{
    NSMutableDictionary *dictionary = [NSMutableDictionary dictionaryWithCapacity:3];
    [dictionary setObject:(__bridge id)kSecClassInternetPassword forKey:(__bridge id)kSecClass];

    /* kSecAttrService is only valid for kSecClassGenericPassword but not for
     * kSecClassInternetPassword. Nevertheless, it is used on macOS for the
     * wrong password type. It has always worked for now, and changing it would
     * need to handle a password migration. Using this attribute on iOS cause a
     * errSecNoSuchAttr error. */
#if !TARGET_OS_IPHONE
    [dictionary setObject:@"VLC-Password-Service" forKey:(__bridge id)kSecAttrService];
#endif

    char * psz_access_group = var_InheritString(p_keystore, "keychain-access-group");
    if (psz_access_group) {
        [dictionary setObject:[NSString stringWithUTF8String:psz_access_group] forKey:(__bridge id)kSecAttrAccessGroup];
    }
    free(psz_access_group);

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


static void SetAccessibilityForQuery(vlc_keystore *p_keystore,
                                     NSMutableDictionary *query)
{
    int accessibilityType = var_InheritInteger(p_keystore, "keychain-accessibility-type");
    CFStringRef secattr;
    switch (accessibilityType) {
        case 1:
            secattr = kSecAttrAccessibleAfterFirstUnlock;
            break;
        case 2:
            secattr = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
            break;
        case 3:
            secattr = kSecAttrAccessibleAlways;
            break;
        case 4:
            secattr = kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly;
            break;
        case 5:
            secattr = kSecAttrAccessibleAlwaysThisDeviceOnly;
            break;
        case 6:
            secattr = kSecAttrAccessibleWhenUnlocked;
            break;
        case 7:
            secattr = kSecAttrAccessibleWhenUnlockedThisDeviceOnly;
            break;
        default:
            secattr = nil;
            break;
    }
    if (secattr != nil)
        [query setObject:(__bridge id)secattr forKey:(__bridge id)kSecAttrAccessible];
}

struct vlc2secattr
{
    const char *vlc;
    const CFStringRef secattr;
};

static int vlc2secattr_cmp(const void *key_, const void *entry_)
{
    const struct vlc2secattr *entry = entry_;
    const char *key = key_;
    return strcasecmp(key, entry->vlc);
}

static int SetAttributesForQuery(const char *const ppsz_values[KEY_MAX], NSMutableDictionary *query, const char *psz_label)
{
    const char *psz_protocol = ppsz_values[KEY_PROTOCOL];
    const char *psz_user = ppsz_values[KEY_USER];
    const char *psz_server = ppsz_values[KEY_SERVER];
    const char *psz_path = ppsz_values[KEY_PATH];
    const char *psz_port = ppsz_values[KEY_PORT];
    const char *psz_realm = ppsz_values[KEY_REALM];
    const char *psz_authtype = ppsz_values[KEY_AUTHTYPE];

    if (psz_label) {
        [query setObject:[NSString stringWithUTF8String:psz_label] forKey:(__bridge id)kSecAttrLabel];
    }
    if (psz_protocol) {
        const struct vlc2secattr tab[] =
        { /* /!\ Alphabetical order /!\ */
            { "ftp", kSecAttrProtocolFTP },
            { "ftps", kSecAttrProtocolFTPS },
            { "http", kSecAttrProtocolHTTP },
            { "https", kSecAttrProtocolHTTPS },
            { "rtsp", kSecAttrProtocolRTSP },
            { "sftp", kSecAttrProtocolSSH },
            { "smb", kSecAttrProtocolSMB },
        };

        const struct vlc2secattr *entry =
            bsearch(psz_protocol, tab, ARRAY_SIZE(tab), sizeof(tab[0]), vlc2secattr_cmp);
        if (!entry)
            return VLC_EGENERIC;

        [query setObject:(__bridge id)entry->secattr forKey:(__bridge id)kSecAttrProtocol];
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
        [query setObject:[NSNumber numberWithInt:atoi(psz_port)] forKey:(__bridge id)kSecAttrPort];
    }
    if (psz_realm) {
        [query setObject:[NSString stringWithUTF8String:psz_realm] forKey:(__bridge id)kSecAttrSecurityDomain];
    }
    if (psz_authtype) {
        if (strncasecmp(psz_protocol, "http", 4) == 0) {
            const struct vlc2secattr tab[] =
            { /* /!\ Alphabetical order /!\ */
                { "Basic", kSecAttrAuthenticationTypeHTTPBasic },
                { "Digest", kSecAttrAuthenticationTypeHTTPDigest },
            };
            const struct vlc2secattr *entry =
                bsearch(psz_authtype, tab, ARRAY_SIZE(tab), sizeof(tab[0]), vlc2secattr_cmp);
            if (entry)
                [query setObject:(__bridge id)entry->secattr forKey:(__bridge id)kSecAttrAuthenticationType];
        }
        else if (strcasecmp(psz_protocol, "smb") == 0) {
            if (strcmp(psz_authtype, "2") == 0)
                [query setObject:(__bridge id)kSecAttrAuthenticationTypeMSN forKey:(__bridge id)kSecAttrAuthenticationType];
        }
    }

    return VLC_SUCCESS;
}

static int FillEntryValues(const NSDictionary *item, char *ppsz_values[KEY_MAX])
{
    NSString *protocol = [item objectForKey:(__bridge id)kSecAttrProtocol];
    if (protocol)
    {
        ppsz_values[KEY_PROTOCOL] = strdup([protocol UTF8String]);
        if (!ppsz_values[KEY_PROTOCOL])
            return VLC_ENOMEM;
    }

    NSString *user = [item objectForKey:(__bridge id)kSecAttrAccount];
    if (user)
    {
        ppsz_values[KEY_USER] = strdup([user UTF8String]);
        if (!ppsz_values[KEY_USER])
            return VLC_ENOMEM;
    }

    NSString *server = [item objectForKey:(__bridge id)kSecAttrServer];
    if (server)
    {
        ppsz_values[KEY_SERVER] = strdup([server UTF8String]);
        if (!ppsz_values[KEY_SERVER])
            return VLC_ENOMEM;
    }

    NSString *path = [item objectForKey:(__bridge id)kSecAttrPath];
    if (path)
    {
        ppsz_values[KEY_PATH] = strdup([path UTF8String]);
        if (!ppsz_values[KEY_PATH])
            return VLC_ENOMEM;
    }

    NSNumber *port = [item objectForKey:(__bridge id)kSecAttrPort];
    if (port)
    {
        ppsz_values[KEY_PORT] = strdup([[port stringValue] UTF8String]);
        if (!ppsz_values[KEY_PORT])
            return VLC_ENOMEM;
    }

    NSString *realm = [item objectForKey:(__bridge id)kSecAttrSecurityDomain];
    if (realm)
    {
        ppsz_values[KEY_REALM] = strdup([realm UTF8String]);
        if (!ppsz_values[KEY_REALM])
            return VLC_ENOMEM;
    }

    const char *auth_val = NULL;
    if ([protocol isEqualToString:(__bridge NSString*)kSecAttrProtocolHTTP]
     || [protocol isEqualToString:(__bridge NSString*)kSecAttrProtocolHTTPS])
    {
        id authtype = [item objectForKey:(__bridge id)kSecAttrAuthenticationType];
        if (authtype == (__bridge id)kSecAttrAuthenticationTypeHTTPBasic)
            auth_val = "Basic";
        else if (authtype == (__bridge id)kSecAttrAuthenticationTypeHTTPDigest)
            auth_val = "Digest";
    }
    else if ([protocol isEqualToString:(__bridge NSString*)kSecAttrProtocolSMB])
    {
        id keytype = [item objectForKey:(__bridge id)kSecAttrAuthenticationType];
        if (keytype == (__bridge id)kSecAttrAuthenticationTypeMSN)
            auth_val = "2";
    }
    if (auth_val)
    {
        ppsz_values[KEY_AUTHTYPE] = strdup(auth_val);
        if (!ppsz_values[KEY_AUTHTYPE])
            return VLC_ENOMEM;
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

    msg_Dbg(p_keystore, "Store keychain entry for server %s", ppsz_values[KEY_SERVER]);

    NSMutableDictionary *query = nil;
    NSMutableDictionary *searchQuery = CreateQuery(p_keystore);

    /* set attributes */
    if (SetAttributesForQuery(ppsz_values, searchQuery, psz_label))
        return VLC_EGENERIC;

    // One return type must be added for SecItemCopyMatching, even if not used.
    // Older macOS versions (10.7) are very picky here...
    [searchQuery setObject:@(YES) forKey:(__bridge id)kSecReturnRef];
    CFTypeRef result = NULL;

    /* search */
    status = SecItemCopyMatching((__bridge CFDictionaryRef)searchQuery, &result);

    if (status == errSecSuccess) {
        msg_Dbg(p_keystore, "the item was already known to keychain, so it will be updated");
        /* item already existed in keychain, let's update */
        query = [[NSMutableDictionary alloc] init];

        /* just set the secret data */
        NSData *secretData = [NSData dataWithBytes:p_secret length:i_secret_len];
        [query setObject:secretData forKey:(__bridge id)kSecValueData];
        [query setObject:@(kVlc4Creator) forKey:(__bridge id)kSecAttrCreator];

        status = SecItemUpdate((__bridge CFDictionaryRef)(searchQuery), (__bridge CFDictionaryRef)(query));
    } else if (status == errSecItemNotFound) {
        msg_Dbg(p_keystore, "creating new item in keychain");
        /* item not found, let's create! */
        query = CreateQuery(p_keystore);

        /* set attributes */
        SetAttributesForQuery(ppsz_values, query, psz_label);

        /* set accessibility */
        SetAccessibilityForQuery(p_keystore, query);

        /* set secret data */
        NSData *secretData = [NSData dataWithBytes:p_secret length:i_secret_len];
        [query setObject:secretData forKey:(__bridge id)kSecValueData];
        [query setObject:@(kVlc4Creator) forKey:(__bridge id)kSecAttrCreator];

        status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
    }
    if (status != errSecSuccess) {
        msg_Err(p_keystore, "Storage failed (%i: '%s')", (int)status, [ErrorForStatus(status) UTF8String]);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static unsigned int Find(vlc_keystore *p_keystore,
                         const char *const ppsz_values[KEY_MAX],
                         vlc_keystore_entry **pp_entries)
{
    CFTypeRef result = NULL;
    NSMutableDictionary *baseLookupQuery = CreateQuery(p_keystore);
    OSStatus status;

    msg_Dbg(p_keystore, "Lookup keychain entry for server %s", ppsz_values[KEY_SERVER]);

    /* set attributes */
    if (SetAttributesForQuery(ppsz_values, baseLookupQuery, NULL))
        return 0;

    /* search */
    NSMutableDictionary *searchQuery = [baseLookupQuery mutableCopy];
    [searchQuery setObject:(__bridge id)kCFBooleanTrue forKey:(__bridge id)kSecReturnAttributes];
    [searchQuery setObject:(__bridge id)kSecMatchLimitAll forKey:(__bridge id)kSecMatchLimit];

    status = SecItemCopyMatching((__bridge CFDictionaryRef)searchQuery, &result);
    if (status != errSecSuccess) {
        msg_Warn(p_keystore, "lookup failed (%i: '%s')", status, [ErrorForStatus(status) UTF8String]);
        return 0;
    }

    NSArray *listOfResults = (__bridge_transfer NSArray *)result;
    NSUInteger count = listOfResults.count;
    msg_Dbg(p_keystore, "found %lu result(s) for the provided attributes", (unsigned long)count);

    vlc_keystore_entry *p_entries = calloc(count,
                                           sizeof(vlc_keystore_entry));
    if (!p_entries)
        return 0;

    for (NSUInteger i = 0; i < count; i++) {
        vlc_keystore_entry *p_entry = &p_entries[i];
        NSDictionary *keychainItem = [listOfResults objectAtIndex:i];

        if (FillEntryValues(keychainItem, p_entry->ppsz_values))
        {
            vlc_keystore_release_entries(p_entries, 1);
            return 0;
        }

        NSString *accountName = [keychainItem objectForKey:(__bridge id)kSecAttrAccount];
        NSMutableDictionary *passwordFetchQuery = [baseLookupQuery mutableCopy];
        [passwordFetchQuery setObject:(__bridge id)kCFBooleanTrue forKey:(__bridge id)kSecReturnData];
        [passwordFetchQuery setObject:(__bridge id)kSecMatchLimitOne forKey:(__bridge id)kSecMatchLimit];
        if (accountName)
            [passwordFetchQuery setObject:accountName forKey:(__bridge id)kSecAttrAccount];

        CFTypeRef secretResult = NULL;
        status = SecItemCopyMatching((__bridge CFDictionaryRef)passwordFetchQuery, &secretResult);
        if (status != noErr) {
            msg_Err(p_keystore, "Lookup error: %i (%s)", (int)status, [ErrorForStatus(status) UTF8String]);
            vlc_keystore_release_entries(p_entries, (unsigned int)count);
            return 0;
        }

        NSData *secretData = (__bridge_transfer NSData *)secretResult;
        NSNumber *creator = [keychainItem objectForKey:(__bridge id)kSecAttrCreator];
        if (creator && [creator isEqual:@(kVlc4Creator)]) {
            msg_Dbg(p_keystore, "Found keychain entry in vlc4 format");
            vlc_keystore_entry_set_secret(p_entry, secretData.bytes, secretData.length);

        } else {
            msg_Dbg(p_keystore, "Found keychain entry in vlc3 format");

            /* we need to do some padding here, as string is expected to be 0 terminated */
            NSUInteger secretDataLength = secretData.length;
            uint8_t *paddedSecretData = calloc(1, secretDataLength + 1);
            memcpy(paddedSecretData, secretData.bytes, secretDataLength);
            vlc_keystore_entry_set_secret(p_entry, paddedSecretData, secretDataLength + 1);
            free(paddedSecretData);
        }

        vlc_keystore_entry_set_secret(p_entry, secretData.bytes, secretData.length);
    }

    *pp_entries = p_entries;

    return (unsigned int)count;
}

static unsigned int Remove(vlc_keystore *p_keystore,
                           const char *const ppsz_values[KEY_MAX])
{
    CFTypeRef result = NULL;
    NSMutableDictionary *query = CreateQuery(p_keystore);
    OSStatus status;

    if (SetAttributesForQuery(ppsz_values, query, NULL))
        return 0;

    [query setObject:@(YES) forKey:(__bridge id)kSecReturnAttributes];
    [query setObject:(__bridge id)kSecMatchLimitAll forKey:(__bridge id)kSecMatchLimit];

    /* do a copy matching to see how many items we are going to delete */
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status != errSecSuccess) {
        msg_Warn(p_keystore, "lookup failed (%i: '%s')", (int)status, [ErrorForStatus(status) UTF8String]);
        return 0;
    }

    NSArray *listOfResults = (__bridge_transfer NSArray *)result;
    NSUInteger count = listOfResults.count;
    msg_Dbg(p_keystore, "found %lu result(s) for the provided attributes",
            (unsigned long)count);

    /* delete everything!! */
    status = SecItemDelete((__bridge CFDictionaryRef)query);

    if (status != errSecSuccess) {
        msg_Err(p_keystore, "deleting items matching the provided attributes failed");
        return VLC_EGENERIC;
    }

    return (unsigned int)count;
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
