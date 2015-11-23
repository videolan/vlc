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

/**
 * @file
 * This file declares vlc keystore API
 */

#ifndef VLC_KEYSTORE_H
# define VLC_KEYSTORE_H

#include <vlc_common.h>

typedef struct vlc_keystore vlc_keystore;
typedef struct vlc_keystore_entry vlc_keystore_entry;

/**
 * @defgroup keystore Keystore API
 * @{
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

typedef struct vlc_keystore_sys vlc_keystore_sys;
struct vlc_keystore
{
    VLC_COMMON_MEMBERS
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
