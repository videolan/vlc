/*****************************************************************************
 * vlc_addons.h : addons handling and describing
 *****************************************************************************
 * Copyright (C) 2013 VideoLAN and authors
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

#ifndef VLC_ADDONS_H
#define VLC_ADDONS_H 1

#include <vlc_arrays.h>
#include <vlc_events.h>

# ifdef __cplusplus
extern "C" {
# endif

typedef enum addon_type_t
{
    ADDON_UNKNOWN = 0,
    ADDON_EXTENSION,
    ADDON_PLAYLIST_PARSER,
    ADDON_SERVICE_DISCOVERY,
    ADDON_SKIN2,
    ADDON_PLUGIN,
    ADDON_INTERFACE,
    ADDON_META,
    ADDON_OTHER
} addon_type_t;

typedef enum addon_state_t
{
    ADDON_NOTINSTALLED = 0,
    ADDON_INSTALLING,
    ADDON_INSTALLED,
    ADDON_UNINSTALLING
} addon_state_t;

typedef enum addon_flags_t
{
    ADDON_BROKEN     = 1, /* Have install inconsistency */
    ADDON_MANAGEABLE = 1 << 1, /* Have manifest, can install or uninstall files */
    ADDON_UPDATABLE  = 1 << 2,
} addon_flags_t;

#define ADDON_MAX_SCORE (5 * 100)
#define ADDON_UUID_SIZE 16
#define ADDON_UUID_PSZ_SIZE (ADDON_UUID_SIZE * 2 + 4)
typedef uint8_t addon_uuid_t[ADDON_UUID_SIZE];

typedef struct addon_file_t
{
    addon_type_t e_filetype;
    char *psz_download_uri;
    char *psz_filename;
} addon_file_t;

struct addon_entry_t
{
    vlc_mutex_t lock;

    addon_type_t e_type;
    addon_state_t e_state;
    addon_flags_t e_flags;

    /* data describing addon */
    addon_uuid_t uuid;
    char *psz_name;
    char *psz_summary;
    char *psz_description;
    char *psz_author;
    char *psz_source_uri; /* webpage, ... */
    char *psz_image_uri;
    char *psz_image_data; /* base64, png */
    char *psz_version;

    /* stats */
    long int i_downloads;
    int i_score; /* score 0..5 in hundredth */

    /* Lister */
    char *psz_source_module;

    /* files list */
    char *psz_archive_uri; /* Archive */
    DECL_ARRAY(addon_file_t *) files;

    /* custom data storage (if needed by module/source) */
    void * p_custom;
};

typedef struct addons_finder_t addons_finder_t;
typedef struct addons_finder_sys_t addons_finder_sys_t;
struct addons_finder_t
{
    struct vlc_object_t obj;

    int ( * pf_find )( addons_finder_t * );
    int ( * pf_retrieve )( addons_finder_t *, addon_entry_t * );
    DECL_ARRAY( addon_entry_t * ) entries;
    char *psz_uri;

    addons_finder_sys_t *p_sys;
};

typedef struct addons_storage_t addons_storage_t;
typedef struct addons_storage_sys_t addons_storage_sys_t;
struct addons_storage_t
{
    struct vlc_object_t obj;

    int ( * pf_install )( addons_storage_t *, addon_entry_t * );
    int ( * pf_remove )( addons_storage_t *, addon_entry_t * );
    int ( * pf_catalog ) ( addons_storage_t *, addon_entry_t **, int );

    addons_storage_sys_t *p_sys;
};

typedef struct addons_manager_t addons_manager_t;

struct addons_manager_owner
{
    void *sys;
    void (*addon_found)(struct addons_manager_t *, struct addon_entry_t *);
    void (*discovery_ended)(struct addons_manager_t *);
    void (*addon_changed)(struct addons_manager_t *, struct addon_entry_t *);
};

typedef struct addons_manager_private_t addons_manager_private_t;
struct addons_manager_t
{
    struct addons_manager_owner owner;
    addons_manager_private_t *p_priv;
};

/**
 *  addon entry lifecycle
 */
VLC_API addon_entry_t *addon_entry_New( void );
VLC_API addon_entry_t *addon_entry_Hold(addon_entry_t *);
VLC_API void addon_entry_Release(addon_entry_t *);

/**
 * addons manager lifecycle
 */
VLC_API addons_manager_t *addons_manager_New( vlc_object_t *,
    const struct addons_manager_owner * );
VLC_API void addons_manager_Delete( addons_manager_t * );

/**
 * Charge currently installed, usable and manageable addons
 * (default "addons storage" module)
 */
VLC_API int addons_manager_LoadCatalog( addons_manager_t * );

/**
 * Gather addons info from repository (default "addons finder" module)
 * If psz_uri is not NULL, only gather info from the pointed package.
 */
VLC_API void addons_manager_Gather( addons_manager_t *, const char *psz_uri );

/**
 * Install or Remove the addon identified by its uuid
 */
VLC_API int addons_manager_Install( addons_manager_t *p_manager, const addon_uuid_t uuid );
VLC_API int addons_manager_Remove( addons_manager_t *p_manager, const addon_uuid_t uuid );

/**
 * String uuid to binary uuid helpers
 */
static inline bool addons_uuid_read( const char *psz_uuid, addon_uuid_t *p_uuid )
{
    if ( !psz_uuid ) return false;
    if ( strlen( psz_uuid ) < ADDON_UUID_PSZ_SIZE ) return false;

    int i = 0, j = 0;
    while ( i<ADDON_UUID_PSZ_SIZE )
    {
        if ( *( psz_uuid + i ) == '-' )
            i++;
        int v;
        sscanf( psz_uuid + i, "%02x", &v );
        (*p_uuid)[j++] = v & 0xFF;
        i+=2;
    }

    return true;
}

static inline char * addons_uuid_to_psz( const addon_uuid_t * p_uuid )
{
    char *psz = (char*) calloc( ADDON_UUID_PSZ_SIZE + 1 , sizeof(char) );
    if ( psz )
    {
        int i=0;
        char *p = psz;
        while ( i < ADDON_UUID_SIZE )
        {
            if ( i == 4 || i== 7 || i== 9 || i== 11 )
                *p++ = '-';
            int v = 0xFF & (*p_uuid)[i];
            sprintf( p, "%02x", v );
            p += 2;
            i++;
        }
    }
    return psz;
}

# ifdef __cplusplus
}
# endif

#endif
