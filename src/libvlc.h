/*****************************************************************************
 * libvlc.h: Internal libvlc generic/misc declaration
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

#ifndef LIBVLC_LIBVLC_H
# define LIBVLC_LIBVLC_H 1

#include <vlc_input_item.h>

extern const char psz_vlc_changeset[];

typedef struct variable_t variable_t;

/*
 * OS-specific initialization
 */
void system_Init      ( void );
void system_Configure ( libvlc_int_t *, int, const char *const [] );
#if defined(_WIN32) || defined(__OS2__)
void system_End(void);
#endif
void vlc_CPU_dump(vlc_object_t *);

/*
 * Threads subsystem
 */

/* This cannot be used as is from plugins yet: */
int vlc_clone_detach (vlc_thread_t *, void *(*)(void *), void *, int);

int vlc_set_priority( vlc_thread_t, int );

void vlc_threads_setup (libvlc_int_t *);

void vlc_trace (const char *fn, const char *file, unsigned line);
#define vlc_backtrace() vlc_trace(__func__, __FILE__, __LINE__)

/*
 * Logging
 */
typedef struct vlc_logger vlc_logger_t;

int vlc_LogPreinit(libvlc_int_t *) VLC_USED;
void vlc_LogInit(libvlc_int_t *);

/*
 * LibVLC exit event handling
 */
typedef struct vlc_exit
{
    vlc_mutex_t lock;
    void (*handler) (void *);
    void *opaque;
} vlc_exit_t;

void vlc_ExitInit( vlc_exit_t * );

/*
 * LibVLC objects stuff
 */

/**
 * Initializes a VLC object.
 *
 * @param obj storage space for object to initialize [OUT]
 * @param parent parent object (or NULL to initialize the root) [IN]
 * @param type_name object type name
 *
 * @note The type name pointer must remain valid even after the object is
 * deinitialized, as it might be passed by address to log message queue.
 * Using constant string literals is appropriate.
 *
 * @retval 0 on success
 * @retval -1 on (out of memory) error
 */
int vlc_object_init(vlc_object_t *obj, vlc_object_t *parent,
                    const char *type_name);

/**
 * Deinitializes a VLC object.
 *
 * This frees resources allocated by vlc_object_init().
 */
void vlc_object_deinit(vlc_object_t *obj);

/**
 * Creates a VLC object.
 *
 * Note that because the object name pointer must remain valid, potentially
 * even after the destruction of the object (through the message queues), this
 * function CANNOT be exported to plugins as is. In this case, the old
 * vlc_object_create() must be used instead.
 *
 * @param p_this an existing VLC object
 * @param i_size byte size of the object structure
 * @param psz_type object type name
 * @return the created object, or NULL.
 */
extern void *
vlc_custom_create (vlc_object_t *p_this, size_t i_size, const char *psz_type);
#define vlc_custom_create(o, s, n) \
        vlc_custom_create(VLC_OBJECT(o), s, n)

/**
 * Allocates an object resource.
 *
 * @param size storage size in bytes of the resource data
 * @param release callback to release the resource
 *
 * @return a pointer to the (uninitialized) storage space, or NULL on error
 */
void *vlc_objres_new(size_t size, void (*release)(void *));

/**
 * Pushes an object resource on the object resources stack.
 *
 * @param obj object to allocate the resource for
 * @param data resource base address (as returned by vlc_objres_new())
 */
void vlc_objres_push(vlc_object_t *obj, void *data);

/**
 * Releases all resources of an object.
 *
 * All resources added with vlc_objres_add() are released in reverse order.
 * The resource list is reset to empty.
 *
 * @param obj object whose resources to release
 */
void vlc_objres_clear(vlc_object_t *obj);

/**
 * Releases one object resource explicitly.
 *
 * If a resource associated with an object needs to be released explicitly
 * earlier than normal, call this function. This is relatively slow and should
 * be avoided.
 *
 * @param obj object whose resource to release
 * @param data private data for the comparison function
 * @param match comparison function to match the targeted resource
 */
void vlc_objres_remove(vlc_object_t *obj, void *data,
                       bool (*match)(void *, void *));

#define ZOOM_SECTION N_("Zoom")
#define ZOOM_QUARTER_KEY_TEXT N_("1:4 Quarter")
#define ZOOM_HALF_KEY_TEXT N_("1:2 Half")
#define ZOOM_ORIGINAL_KEY_TEXT N_("1:1 Original")
#define ZOOM_DOUBLE_KEY_TEXT N_("2:1 Double")

/**
 * Private LibVLC instance data.
 */
typedef struct vlc_dialog_provider vlc_dialog_provider;
typedef struct vlc_keystore vlc_keystore;
typedef struct vlc_actions_t vlc_actions_t;
typedef struct vlc_playlist vlc_playlist_t;
typedef struct vlc_media_source_provider_t vlc_media_source_provider_t;
typedef struct intf_thread_t intf_thread_t;

typedef struct libvlc_priv_t
{
    libvlc_int_t       public_data;

    /* Singleton objects */
    vlc_mutex_t lock; ///< protect playlist and interfaces
    vlm_t             *p_vlm;  ///< the VLM singleton (or NULL)
    vlc_dialog_provider *p_dialog_provider; ///< dialog provider
    vlc_keystore      *p_memory_keystore; ///< memory keystore
    intf_thread_t *interfaces;  ///< Linked-list of interfaces
    vlc_playlist_t *main_playlist;
    struct input_preparser_t *parser; ///< Input item meta data handler
    vlc_media_source_provider_t *media_source_provider;
    vlc_actions_t *actions; ///< Hotkeys handler
    struct vlc_medialibrary_t *p_media_library; ///< Media library instance
    struct vlc_thumbnailer_t *p_thumbnailer; ///< Lazily instantiated media thumbnailer

    /* Exit callback */
    vlc_exit_t       exit;
} libvlc_priv_t;

static inline libvlc_priv_t *libvlc_priv (libvlc_int_t *libvlc)
{
    return container_of(libvlc, libvlc_priv_t, public_data);
}

int intf_InsertItem(libvlc_int_t *, const char *mrl, unsigned optc,
                    const char * const *optv, unsigned flags);
void intf_DestroyAll( libvlc_int_t * );

int vlc_MetadataRequest(libvlc_int_t *libvlc, input_item_t *item,
                        input_item_meta_request_option_t i_options,
                        const input_preparser_callbacks_t *cbs,
                        void *cbs_userdata,
                        int timeout, void *id);

/*
 * Variables stuff
 */
void var_OptionParse (vlc_object_t *, const char *, bool trusted);

#endif
