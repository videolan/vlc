/*****************************************************************************
 * vlc_keystore.h:
 *****************************************************************************
 * Copyright (C) 2015-2016 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_KEYSTORE_H
# define VLC_KEYSTORE_H

#include <vlc_common.h>

typedef struct vlc_keystore vlc_keystore;
typedef struct vlc_keystore_entry vlc_keystore_entry;
typedef struct vlc_credential vlc_credential;

/* Called from src/libvlc.c */
int
libvlc_InternalKeystoreInit(libvlc_int_t *p_libvlc);

/* Called from src/libvlc.c */
void
libvlc_InternalKeystoreClean(libvlc_int_t *p_libvlc);

/**
 * @defgroup keystore Keystore and credential API
 * @ingroup os
 * @{
 * @file
 * This file declares vlc keystore API
 * @defgroup keystore_public Keystore public API
 * @{
 */

/**
 * List of keys that can be stored via the keystore API
 */
enum vlc_keystore_key {
    KEY_PROTOCOL,
    KEY_USER,
    KEY_SERVER,
    KEY_PATH,
    KEY_PORT,
    KEY_REALM,
    KEY_AUTHTYPE,
    KEY_MAX,
};
#define VLC_KEYSTORE_VALUES_INIT(ppsz_values) memset(ppsz_values, 0, sizeof(const char *) * KEY_MAX)

/**
 * Keystore entry returned by vlc_keystore_find()
 */
struct vlc_keystore_entry
{
    /** Set of key/values. Values can be NULL */
    char *              ppsz_values[KEY_MAX];
    /** Secret password */
    uint8_t *           p_secret;
    /** Length of the secret */
    size_t              i_secret_len;
};

/**
 * Create a keystore object
 *
 * A keystore object is persistent across runtime. It is saved on local
 * filesystem via a vlc keystore module (KWallet, SecretService, Apple Keychain
 * Service ...).
 *
 * @note to be released with vlc_keystore_release()
 *
 * @param p_parent the parent object used to create the keystore object
 *
 * @return a pointer to the keystore object, or NULL in case of error
 */
VLC_API vlc_keystore *
vlc_keystore_create(vlc_object_t *p_parent);
#define vlc_keystore_create(x) vlc_keystore_create(VLC_OBJECT(x))

/**
 * Release a keystore object
 */
VLC_API void
vlc_keystore_release(vlc_keystore *p_keystore);


/**
 * Store a secret associated with a set of key/values
 *
 * @param ppsz_values set of key/values, see vlc_keystore_key.
 *        ppsz_values[KEY_PROTOCOL] and  ppsz_values[KEY_SERVER] must be valid
 *        strings
 * @param p_secret binary secret or string password
 * @param i_secret_len length of p_secret. If it's less than 0, then p_secret
 * is assumed to be a '\0' terminated string
 * @param psz_label user friendly label
 *
 * @return VLC_SUCCESS on success, or VLC_EGENERIC on error
 */
VLC_API int
vlc_keystore_store(vlc_keystore *p_keystore,
                   const char *const ppsz_values[KEY_MAX],
                   const uint8_t* p_secret, ssize_t i_secret_len,
                   const char *psz_label);

/**
 * Find all entries that match a set of key/values
 *
 * @param ppsz_values set of key/values, see vlc_keystore_key, any values can
 * be NULL
 * @param pp_entries list of found entries. To be released with
 * vlc_keystore_release_entries()
 *
 * @return the number of entries
 */
VLC_API unsigned int
vlc_keystore_find(vlc_keystore *p_keystore,
                  const char *const ppsz_values[KEY_MAX],
                  vlc_keystore_entry **pp_entries) VLC_USED;

/**
 * Remove all entries that match a set of key/values
 *
 * @note only entries added by VLC can be removed
 *
 * @param ppsz_values set of key/values, see vlc_keystore_key, any values can
 * be NULL
 *
 * @return the number of entries
 */
VLC_API unsigned int
vlc_keystore_remove(vlc_keystore *p_keystore,
                    const char *const ppsz_values[KEY_MAX]);

/**
 * Release the list of entries returned by vlc_keystore_find()
 */
VLC_API void
vlc_keystore_release_entries(vlc_keystore_entry *p_entries, unsigned int i_count);

/**
 * @}
 * @defgroup credential Credential API
 * @{
 */

/**
 * @note init with vlc_credential_init()
 */
struct vlc_credential
{
    /** url to store or to search */
    const vlc_url_t *p_url;
    /** http realm or smb domain to search, can be overridden after a call to
     * vlc_credential_get() */
    const char *psz_realm;
    /** http authtype to search, can be overridden after a call to
     * vlc_credential_get() */
    const char *psz_authtype;
    /** valid only if vlc_credential_get() returned true */
    const char *psz_username;
    /** valid only if vlc_credential_get() returned true */
    const char *psz_password;

    /* internal */
    enum {
        GET_FROM_URL,
        GET_FROM_OPTION,
        GET_FROM_MEMORY_KEYSTORE,
        GET_FROM_KEYSTORE,
        GET_FROM_DIALOG,
    } i_get_order;

    vlc_keystore *p_keystore;
    vlc_keystore_entry *p_entries;
    unsigned int i_entries_count;

    char *psz_split_domain;
    char *psz_var_username;
    char *psz_var_password;

    char *psz_dialog_username;
    char *psz_dialog_password;
    bool b_from_keystore;
    bool b_store;
};

/**
 * Init a credential struct
 *
 * @note to be cleaned with vlc_credential_clean()
 *
 * @param psz_url url to store or to search
 */
VLC_API void
vlc_credential_init(vlc_credential *p_credential, const vlc_url_t *p_url);

/**
 * Clean a credential struct
 */
VLC_API void
vlc_credential_clean(vlc_credential *p_credential);

/**
 * Get a username/password couple
 *
 * This will search for a credential using url, VLC options, the vlc_keystore
 * or by asking the user via dialog_Login(). This function can be called
 * indefinitely, it will first return the user/password from the url (if any),
 * then from VLC options (if any), then from the keystore (if any), and finally
 * from the dialog (if any). This function will return true as long as the user
 * fill the dialog texts and will return false when the user cancel it.
 *
 * @param p_parent the parent object (for var, keystore and dialog)
 * @param psz_option_username VLC option name for the username
 * @param psz_option_password VLC option name for the password
 * @param psz_dialog_title dialog title, if NULL, this function won't use the
 * keystore or the dialog
 * @param psz_dialog_fmt dialog text using format
 *
 * @return true if vlc_credential.psz_username and vlc_credential.psz_password
 * are valid, otherwise this function should not be called again.
 */

VLC_API bool
vlc_credential_get(vlc_credential *p_credential, vlc_object_t *p_parent,
                   const char *psz_option_username,
                   const char *psz_option_password,
                   const char *psz_dialog_title,
                   const char *psz_dialog_fmt, ...) VLC_FORMAT(6, 7);
#define vlc_credential_get(a, b, c, d, e, f, ...) \
    vlc_credential_get(a, VLC_OBJECT(b), c, d, e, f, ##__VA_ARGS__)

/**
 * Store the last dialog credential returned by vlc_credential_get()
 *
 * This function will store the credential in the memory keystore if it's
 * valid, or will store in the permanent one if it comes from the dialog and if
 * the user asked for it.
 *
 * @return true if the credential was stored or comes from the keystore, false
 * otherwise
 */
VLC_API bool
vlc_credential_store(vlc_credential *p_credential, vlc_object_t *p_parent);
#define vlc_credential_store(a, b) \
    vlc_credential_store(a, VLC_OBJECT(b))

/**
 * @}
 * @defgroup keystore_implementation Implemented by keystore modules
 * @{
 */

#define VLC_KEYSTORE_NAME "libVLC"

static inline int
vlc_keystore_entry_set_secret(vlc_keystore_entry *p_entry,
                              const uint8_t *p_secret, size_t i_secret_len)
{
    p_entry->p_secret = (uint8_t*) malloc(i_secret_len);
    if (!p_entry->p_secret)
        return VLC_EGENERIC;
    memcpy(p_entry->p_secret, p_secret, i_secret_len);
    p_entry->i_secret_len = i_secret_len;
    return VLC_SUCCESS;
}

static inline void
vlc_keystore_release_entry(vlc_keystore_entry *p_entry)
{
    for (unsigned int j = 0; j < KEY_MAX; ++j)
    {
        free(p_entry->ppsz_values[j]);
        p_entry->ppsz_values[j] = NULL;
    }
    free(p_entry->p_secret);
    p_entry->p_secret = NULL;
}

typedef struct vlc_keystore_sys vlc_keystore_sys;
struct vlc_keystore
{
    struct vlc_object_t obj;
    module_t            *p_module;
    vlc_keystore_sys    *p_sys;

    /** See vlc_keystore_store() */
    int                 (*pf_store)(vlc_keystore *p_keystore,
                                    const char *const ppsz_values[KEY_MAX],
                                    const uint8_t *p_secret,
                                    size_t i_secret_len, const char *psz_label);
    /**  See vlc_keystore_find() */
    unsigned int        (*pf_find)(vlc_keystore *p_keystore,
                                   const char *const ppsz_values[KEY_MAX],
                                   vlc_keystore_entry **pp_entries);

    /** See vlc_keystore_remove() */
    unsigned int        (*pf_remove)(vlc_keystore *p_keystore,
                                     const char *const ppsz_values[KEY_MAX]);
};

/** @} @} */

#endif
