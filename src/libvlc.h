/*****************************************************************************
 * libvlc.h: Internal libvlc generic/misc declaration
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
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

extern const char psz_vlc_changeset[];

typedef struct variable_t variable_t;

/* Actions (hot keys) */
struct vlc_actions;
struct vlc_actions *vlc_InitActions (libvlc_int_t *);
extern void vlc_DeinitActions (libvlc_int_t *, struct vlc_actions *);

size_t vlc_towc (const char *str, uint32_t *restrict pwc);

/*
 * OS-specific initialization
 */
void system_Init      ( void );
void system_Configure ( libvlc_int_t *, int, const char *const [] );
void system_End       ( void );

void vlc_CPU_init(void);
void vlc_CPU_dump(vlc_object_t *);

/*
 * Threads subsystem
 */

/* This cannot be used as is from plugins yet: */
int vlc_clone_detach (vlc_thread_t *, void *(*)(void *), void *, int);

int vlc_object_waitpipe (vlc_object_t *obj);

int vlc_set_priority( vlc_thread_t, int );

void vlc_threads_setup (libvlc_int_t *);

void vlc_trace (const char *fn, const char *file, unsigned line);
#define vlc_backtrace() vlc_trace(__func__, __FILE__, __LINE__)

#if defined (LIBVLC_USE_PTHREAD) && !defined (NDEBUG)
void vlc_assert_locked (vlc_mutex_t *);
#else
# define vlc_assert_locked( m ) (void)m
#endif

/*
 * LibVLC exit event handling
 */
typedef struct vlc_exit
{
    vlc_mutex_t lock;
    void (*handler) (void *);
    void *opaque;
    bool killed;
} vlc_exit_t;

void vlc_ExitInit( vlc_exit_t * );
void vlc_ExitDestroy( vlc_exit_t * );

/*
 * LibVLC objects stuff
 */

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
 * Assign a name to an object for vlc_object_find_name().
 */
extern int vlc_object_set_name(vlc_object_t *, const char *);
#define vlc_object_set_name(o, n) vlc_object_set_name(VLC_OBJECT(o), n)

/* Types */
typedef void (*vlc_destructor_t) (struct vlc_object_t *);
void vlc_object_set_destructor (vlc_object_t *, vlc_destructor_t);
#define vlc_object_set_destructor(a,b) \
        vlc_object_set_destructor (VLC_OBJECT(a), b)

/*
 * To be cleaned-up module stuff:
 */
extern char *psz_vlcpath;

module_t *module_find_by_shortcut (const char *psz_shortcut);

/**
 * Private LibVLC data for each object.
 */
typedef struct vlc_object_internals vlc_object_internals_t;

struct vlc_object_internals
{
    char           *psz_name; /* given name */

    /* Object variables */
    void           *var_root;
    vlc_mutex_t     var_lock;
    vlc_cond_t      var_wait;

    /* Objects thread synchronization */
    int             pipes[2];

    /* Objects management */
    vlc_spinlock_t   ref_spin;
    unsigned         i_refcount;
    vlc_destructor_t pf_destructor;

    /* Objects tree structure */
    vlc_object_internals_t *next;  /* next sibling */
    vlc_object_internals_t *prev;  /* previous sibling */
    vlc_object_internals_t *first; /* first child */
};

#define ZOOM_SECTION N_("Zoom")
#define ZOOM_QUARTER_KEY_TEXT N_("1:4 Quarter")
#define ZOOM_HALF_KEY_TEXT N_("1:2 Half")
#define ZOOM_ORIGINAL_KEY_TEXT N_("1:1 Original")
#define ZOOM_DOUBLE_KEY_TEXT N_("2:1 Double")

#define vlc_internals( obj ) (((vlc_object_internals_t*)(VLC_OBJECT(obj)))-1)
#define vlc_externals( priv ) ((vlc_object_t *)((priv) + 1))

typedef struct sap_handler_t sap_handler_t;

/**
 * Private LibVLC instance data.
 */
typedef struct libvlc_priv_t
{
    libvlc_int_t       public_data;

    bool               playlist_active;

    /* Messages */
    int                i_verbose;   ///< info messages
    bool               b_color;     ///< color messages?

    /* Timer stats */
    bool               b_stats;     ///< Whether to collect stats
    vlc_mutex_t        timer_lock;  ///< Lock to protect timers
    counter_t        **pp_timers;   ///< Array of all timers
    int                i_timers;    ///< Number of timers

    /* Singleton objects */
    module_t          *p_memcpy_module;  ///< Fast memcpy plugin used
    playlist_t        *p_playlist; ///< the playlist singleton
    struct media_library_t *p_ml;    ///< the ML singleton
    vlc_mutex_t       ml_lock; ///< Mutex for ML creation
    vlm_t             *p_vlm;  ///< the VLM singleton (or NULL)
    vlc_object_t      *p_dialog_provider; ///< dialog provider
#ifdef ENABLE_SOUT
    sap_handler_t     *p_sap; ///< SAP SDP advertiser
#endif
    struct vlc_actions *actions; ///< Hotkeys handler

    /* Interfaces */
    struct intf_thread_t *p_intf; ///< Interfaces linked-list

    /* Objects tree */
    vlc_mutex_t        structure_lock;

    /* Exit callback */
    vlc_exit_t       exit;
} libvlc_priv_t;

static inline libvlc_priv_t *libvlc_priv (libvlc_int_t *libvlc)
{
    return (libvlc_priv_t *)libvlc;
}

void playlist_ServicesDiscoveryKillAll( playlist_t *p_playlist );
void intf_DestroyAll( libvlc_int_t * );

#define libvlc_stats( o ) (libvlc_priv((VLC_OBJECT(o))->p_libvlc)->b_stats)

/**
 * LibVLC "main module" configuration settings array.
 */
extern module_config_t libvlc_config[];
extern const size_t libvlc_config_count;

/*
 * Variables stuff
 */
void var_OptionParse (vlc_object_t *, const char *, bool trusted);

/*
 * Stats stuff
 */
int stats_Update (vlc_object_t*, counter_t *, vlc_value_t, vlc_value_t *);
counter_t * stats_CounterCreate (vlc_object_t*, int, int);
#define stats_CounterCreate(a,b,c) stats_CounterCreate( VLC_OBJECT(a), b, c )
int stats_Get (vlc_object_t*, counter_t *, vlc_value_t*);
#define stats_Get(a,b,c) stats_Get( VLC_OBJECT(a), b, c)

void stats_CounterClean (counter_t * );

static inline int stats_GetInteger( vlc_object_t *p_obj, counter_t *p_counter,
                                    int64_t *value )
{
    int i_ret;
    vlc_value_t val; val.i_int = 0;
    if( !p_counter ) return VLC_EGENERIC;
    i_ret = stats_Get( p_obj, p_counter, &val );
    *value = val.i_int;
    return i_ret;
}
#define stats_GetInteger(a,b,c) stats_GetInteger( VLC_OBJECT(a), b, c )

static inline int stats_GetFloat( vlc_object_t *p_obj, counter_t *p_counter,
                                    float *value )
{
    int i_ret;
    vlc_value_t val; val.f_float = 0.0;
    if( !p_counter ) return VLC_EGENERIC;
    i_ret = stats_Get( p_obj, p_counter, &val );
    *value = val.f_float;
    return i_ret;
}
#define stats_GetFloat(a,b,c) stats_GetFloat( VLC_OBJECT(a), b, c )

static inline int stats_UpdateInteger( vlc_object_t *p_obj,counter_t *p_co,
                                         int i, int *pi_new )
{
    int i_ret;
    vlc_value_t val;
    vlc_value_t new_val; new_val.i_int = 0;
    if( !p_co ) return VLC_EGENERIC;
    val.i_int = i;
    i_ret = stats_Update( p_obj, p_co, val, &new_val );
    if( pi_new )
        *pi_new = new_val.i_int;
    return i_ret;
}
#define stats_UpdateInteger(a,b,c,d) stats_UpdateInteger( VLC_OBJECT(a),b,c,d )

static inline int stats_UpdateFloat( vlc_object_t *p_obj, counter_t *p_co,
                                       float f, float *pf_new )
{
    vlc_value_t val;
    int i_ret;
    vlc_value_t new_val;new_val.f_float = 0.0;
    if( !p_co ) return VLC_EGENERIC;
    val.f_float = f;
    i_ret =  stats_Update( p_obj, p_co, val, &new_val );
    if( pf_new )
        *pf_new = new_val.f_float;
    return i_ret;
}
#define stats_UpdateFloat(a,b,c,d) stats_UpdateFloat( VLC_OBJECT(a),b,c,d )

void stats_ComputeInputStats(input_thread_t*, input_stats_t*);
void stats_ReinitInputStats(input_stats_t *);

#endif
