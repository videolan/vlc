/*****************************************************************************
 * vlc_access.h: Access descriptor, queries and methods
 *****************************************************************************
 * Copyright (C) 1999-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_ACCESS_H
#define VLC_ACCESS_H 1

#include <vlc_stream.h>

/**
 * \defgroup access Access
 * \ingroup input
 * Raw input byte streams
 * @{
 * \file
 * Input byte stream modules interface
 */

/**
 * Special redirection error code.
 *
 * In case of redirection, the access open function should clean up (as in
 * normal failure case), store the heap-allocated redirection URL in
 * access_t.psz_url, and return this value.
 */
#define VLC_ACCESS_REDIRECT VLC_ETIMEOUT

/**
 * Opens a new read-only byte stream.
 *
 * This function might block.
 * The initial offset is of course always zero.
 *
 * \param obj parent VLC object
 * \param mrl media resource location to read
 * \return a new access object on success, NULL on failure
 */
VLC_API access_t *vlc_access_NewMRL(vlc_object_t *obj, const char *mrl);

/**
 * \defgroup access_helper Access Helpers
 * @{
 */

/**
 * Default pf_control callback for directory accesses.
 */
VLC_API int access_vaDirectoryControlHelper( access_t *p_access, int i_query, va_list args );

#define ACCESS_SET_CALLBACKS( read, block, control, seek ) \
    do { \
        p_access->pf_read = (read); \
        p_access->pf_block = (block); \
        p_access->pf_control = (control); \
        p_access->pf_seek = (seek); \
    } while(0)

#define STANDARD_READ_ACCESS_INIT \
    do { \
        ACCESS_SET_CALLBACKS( Read, NULL, Control, Seek ); \
        p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) ); \
        if( !p_sys ) return VLC_ENOMEM;\
    } while(0);

#define STANDARD_BLOCK_ACCESS_INIT \
    do { \
        ACCESS_SET_CALLBACKS( NULL, Block, Control, Seek ); \
        p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) ); \
        if( !p_sys ) return VLC_ENOMEM; \
    } while(0);

/**
 * Access pf_readdir helper struct
 * \see access_fsdir_init()
 * \see access_fsdir_additem()
 * \see access_fsdir_finish()
 */
struct access_fsdir
{
    input_item_node_t *p_node;
    void **pp_slaves;
    unsigned int i_slaves;
    int i_sub_autodetect_fuzzy;
    bool b_show_hiddenfiles;
    char *psz_ignored_exts;
    char *psz_sort;
};

/**
 * Init a access_fsdir struct
 *
 * \param p_fsdir need to be cleaned with access_fsdir_finish()
 * \param p_node node that will be used to add items
 */
VLC_API void access_fsdir_init(struct access_fsdir *p_fsdir,
                               access_t *p_access, input_item_node_t *p_node);

/**
 * Finish adding items to the node
 *
 * \param b_success if true, items of the node will be sorted according
 * "directory-sort" option.
 */
VLC_API void access_fsdir_finish(struct access_fsdir *p_fsdir, bool b_success);

/**
 * Add a new input_item_t entry to the node of the access_fsdir struct.
 *
 * \param p_fsdir previously inited access_fsdir struct
 * \param psz_uri uri of the new item
 * \param psz_filename file name of the new item
 * \param i_type see \ref input_item_type_e
 * \param i_net see \ref input_item_net_type
 */
VLC_API int access_fsdir_additem(struct access_fsdir *p_fsdir,
                                 const char *psz_uri, const char *psz_filename,
                                 int i_type, int i_net);

/**
 * @} @}
 */

#endif
