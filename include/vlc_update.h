/*****************************************************************************
 * vlc_update.h: VLC update and plugins download
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: $
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
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

#ifndef _VLC_UPDATE_H
#define _VLC_UPDATE_H

#include <vlc/vlc.h>

/**
 * \defgroup update Update
 *
 * @{
 */

#define UPDATE_FILE_TYPE_ALL    (~0)
#define UPDATE_FILE_TYPE_NONE   0

#define UPDATE_FILE_TYPE_UNDEF      1
#define UPDATE_FILE_TYPE_INFO       2
#define UPDATE_FILE_TYPE_SOURCE     4
#define UPDATE_FILE_TYPE_BINARY     8
#define UPDATE_FILE_TYPE_PLUGIN     16

#define UPDATE_RELEASE_STATUS_ALL       (~0)
#define UPDATE_RELEASE_STATUS_NONE      0

#define UPDATE_RELEASE_STATUS_OLDER     1
#define UPDATE_RELEASE_STATUS_EQUAL     2
#define UPDATE_RELEASE_STATUS_NEWER     4

#define UPDATE_RELEASE_TYPE_STABLE      1
#define UPDATE_RELEASE_TYPE_TESTING     2
#define UPDATE_RELEASE_TYPE_UNSTABLE    4

#define UPDATE_FAIL     0
#define UPDATE_SUCCESS  1
#define UPDATE_NEXT     0
#define UPDATE_PREV     2
#define UPDATE_MIRROR   4
#define UPDATE_RELEASE  8
#define UPDATE_FILE     16
#define UPDATE_RESET    32

/**
 * Describes an update file
 */
struct update_file_t
{
    int i_type;             ///< File type
    char* psz_md5;          ///< MD5 hash
    long int l_size;        ///< File size in bytes
    char* psz_url;          ///< Relative (to a mirror) or absolute url
    char* psz_description;  ///< Plain text description
};

/**
 * Describes an update VLC release number
 */
struct update_release_t
{
    char* psz_major;        ///< Version major string
    char* psz_minor;        ///< Version minor string
    char* psz_revision;     ///< Version revision string
    char* psz_extra;        ///< Version extra string

    char* psz_svn_revision; ///< SVN revision

    int i_type;             ///< Release type

    int i_status;           ///< Release status compared to current VLC version

    struct update_file_t* p_files; ///< Files list
    int i_files;            ///< Number of files in the files list
};

/**
 * Describes a mirror
 */
struct update_mirror_t
{
    char *psz_name;         ///< Mirror name
    char *psz_location;     ///< Mirror geographical location
    char *psz_type;         ///< Mirror type (FTP, HTTP, ...)

    char *psz_base_url;     ///< Mirror base url

};

/**
 * The update object. Stores (and caches) all information relative to updates
 */
struct update_t
{
    vlc_t *p_vlc;

    vlc_mutex_t lock;

    struct update_release_t *p_releases;    ///< Releases (version) list
    int i_releases;                         ///< Number of releases
    vlc_bool_t b_releases;                  ///< True if we have a releases list

    struct update_mirror_t *p_mirrors;      ///< Mirrors list
    int i_mirrors;                          ///< Number of mirrors
    vlc_bool_t b_mirrors;                   ///< True if we have a mirrors list
};

/**
 * The update iterator structure. Usefull to browse the update object seamlessly
 */
struct update_iterator_t
{
    update_t *p_u;  ///< Pointer to VLC update object

    int i_r;        ///< Position in the releases list
    int i_f;        ///< Position in the release's files list
    int i_m;        ///< Position in the mirrors list

    int i_t;        ///< File type bitmask
    int i_rs;       ///< Release status bitmask
    int i_rt;       ///< Release type bitmask

    struct
    {
        int i_type;             ///< Type
        char* psz_md5;          ///< MD5 hash
        long int l_size;        ///< Size in bytes
        char* psz_url;          ///< Absolute URL
        char* psz_description;  ///< Description
    } file;         ///< Local 'copy' of the current file's information
    struct
    {
        char *psz_version;      ///< Version string
        char *psz_svn_revision; ///< SVN revision
        int i_status;           ///< Status
        int i_type;             ///< Type
    } release;      ///< Local 'copy' of the current release's information
    struct
    {
        char *psz_name;         ///< Name
        char *psz_location;     ///< Geographical location
        char *psz_type;         ///< Type (HTTP, FTP, ...)
    } mirror;       ///< Local 'copy' of the current mirror's information
};

#define update_New( a ) __update_New( VLC_OBJECT( a ) )

VLC_EXPORT( update_t *, __update_New, ( vlc_object_t * ) );
VLC_EXPORT( void, update_Delete, (update_t * ) );
VLC_EXPORT( void, update_Check, ( update_t *, vlc_bool_t ) );

VLC_EXPORT( update_iterator_t *, update_iterator_New, ( update_t * ) );
VLC_EXPORT( void, update_iterator_Delete, ( update_iterator_t * ) );
VLC_EXPORT( unsigned int, update_iterator_Action, ( update_iterator_t *, int ) );
VLC_EXPORT( unsigned int, update_iterator_ChooseMirrorAndFile, ( update_iterator_t *, int, int, int ) );
VLC_EXPORT( void, update_download, ( update_iterator_t *, char * ) );

/**
 * @}
 */

#endif
