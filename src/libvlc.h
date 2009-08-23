/*****************************************************************************
 * libvlc.h: Internal libvlc generic/misc declaration
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

#ifndef LIBVLC_LIBVLC_H
# define LIBVLC_LIBVLC_H 1

typedef struct variable_t variable_t;

/* Actions (hot keys) */
typedef struct action
{
    char name[24];
    int  value;
} action_t;
extern const struct action libvlc_actions[];
extern const size_t libvlc_actions_count;
extern int vlc_key_to_action (vlc_object_t *, const char *,
                              vlc_value_t, vlc_value_t, void *);

/*
 * OS-specific initialization
 */
void system_Init      ( libvlc_int_t *, int *, const char *[] );
void system_Configure ( libvlc_int_t *, int *, const char *[] );
void system_End       ( libvlc_int_t * );

/*
 * Legacy object stuff that is still used within libvlccore (only)
 */
#define vlc_object_signal_unlocked( obj )

vlc_list_t *vlc_list_find( vlc_object_t *, int, int );

/*
 * Threads subsystem
 */

/* This cannot be used as is from plugins: */
void vlc_detach (vlc_thread_t);

/* Hopefully, no need to export this. There is a new thread API instead. */
void vlc_thread_cancel (vlc_object_t *);
int vlc_object_waitpipe (vlc_object_t *obj);

void vlc_threads_setup (libvlc_int_t *);

void vlc_trace (const char *fn, const char *file, unsigned line);
#define vlc_backtrace() vlc_trace(__func__, __FILE__, __LINE__)

#if defined (LIBVLC_USE_PTHREAD) && !defined (NDEBUG)
void vlc_assert_locked (vlc_mutex_t *);
#else
# define vlc_assert_locked( m ) (void)m
#endif

/*
 * CPU capabilities
 */
extern uint32_t cpu_flags;
uint32_t CPUCapabilities( void );

/*
 * Message/logging stuff
 */

/**
 * Store all data required by messages interfaces.
 */
typedef struct msg_bank_t
{
    /** Message queue lock */
    vlc_rwlock_t lock;

    /* Subscribers */
    int i_sub;
    msg_subscription_t **pp_sub;

    /* Logfile for WinCE */
#ifdef UNDER_CE
    FILE *logfile;
#endif
} msg_bank_t;

void msg_Create  (libvlc_int_t *);
void msg_Destroy (libvlc_int_t *);

/** Internal message stack context */
void msg_StackSet ( int, const char*, ... );
void msg_StackAdd ( const char*, ... );
const char* msg_StackMsg ( void );
void msg_StackDestroy (void *);

/*
 * Unicode stuff
 */
char *vlc_fix_readdir (const char *);

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
 * @param i_type object type, usually VLC_OBJECT_CUSTOM
 * @param psz_type object type name
 * @return the created object, or NULL.
 */
extern void *
__vlc_custom_create (vlc_object_t *p_this, size_t i_size, int i_type,
                     const char *psz_type);
#define vlc_custom_create(o, s, t, n) \
        __vlc_custom_create(VLC_OBJECT(o), s, t, n)

/**
 * Assign a name to an object for vlc_object_find_name().
 */
extern int vlc_object_set_name(vlc_object_t *, const char *);
#define vlc_object_set_name(o, n) vlc_object_set_name(VLC_OBJECT(o), n)

/*
 * To be cleaned-up module stuff:
 */
extern char *psz_vlcpath;

/* Return a NULL terminated array with the names of the modules that have a
 * certain capability.
 * Free after uses both the string and the table. */
char **module_GetModulesNamesForCapability (const char * psz_capability,
                                            char ***psz_longname);
module_t *module_find_by_shortcut (const char *psz_shortcut);

/**
 * Private LibVLC data for each object.
 */
typedef struct vlc_object_internals_t
{
    int             i_object_type; /* Object type, deprecated */
    char           *psz_name; /* given name */

    /* Object variables */
    variable_t *    p_vars;
    vlc_mutex_t     var_lock;
    vlc_cond_t      var_wait;
    int             i_vars;

    /* Thread properties, if any */
    vlc_thread_t    thread_id;
    bool            b_thread;

    /* Objects thread synchronization */
    int             pipes[2];

    /* Objects management */
    vlc_spinlock_t   ref_spin;
    unsigned         i_refcount;
    vlc_destructor_t pf_destructor;

    /* Objects tree structure */
    vlc_object_t    *prev, *next;
    vlc_object_t   **pp_children;
    int              i_children;
} vlc_object_internals_t;

#define ZOOM_SECTION N_("Zoom")
#define ZOOM_QUARTER_KEY_TEXT N_("1:4 Quarter")
#define ZOOM_HALF_KEY_TEXT N_("1:2 Half")
#define ZOOM_ORIGINAL_KEY_TEXT N_("1:1 Original")
#define ZOOM_DOUBLE_KEY_TEXT N_("2:1 Double")

#define vlc_internals( obj ) (((vlc_object_internals_t*)(VLC_OBJECT(obj)))-1)

static inline const char *vlc_object_get_name(const vlc_object_t *o)
{
    return vlc_internals(o)->psz_name;
}

typedef struct sap_handler_t sap_handler_t;

/**
 * Private LibVLC instance data.
 */
typedef struct libvlc_priv_t
{
    libvlc_int_t       public_data;
    vlc_cond_t         exiting; ///< signaled when VLC wants to exit

    int                i_last_input_id ; ///< Last id of input item

    /* Messages */
    msg_bank_t         msg_bank;    ///< The message bank
    int                i_verbose;   ///< info messages
    bool               b_color;     ///< color messages?
    vlc_dictionary_t   msg_enabled_objects; ///< Enabled objects
    bool               msg_all_objects_enabled; ///< Should we print all objects?

    /* Timer stats */
    vlc_mutex_t        timer_lock;  ///< Lock to protect timers
    counter_t        **pp_timers;   ///< Array of all timers
    int                i_timers;    ///< Number of timers
    bool               b_stats;     ///< Whether to collect stats

    /* Singleton objects */
    module_t          *p_memcpy_module;  ///< Fast memcpy plugin used
    playlist_t        *p_playlist; //< the playlist singleton
    vlm_t             *p_vlm;  ///< the VLM singleton (or NULL)
    vlc_object_t      *p_dialog_provider; ///< dialog provider
    httpd_t           *p_httpd; ///< HTTP daemon (src/network/httpd.c)
#ifdef ENABLE_SOUT
    sap_handler_t     *p_sap; ///< SAP SDP advertiser
#endif

    /* Interfaces */
    struct intf_thread_t *p_intf; ///< Interfaces linked-list

    /* Objects tree */
    vlc_mutex_t        structure_lock;
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
#define stats_Update(a,b,c) __stats_Update( VLC_OBJECT(a), b, c )
int __stats_Update (vlc_object_t*, counter_t *, vlc_value_t, vlc_value_t *);
#define stats_CounterCreate(a,b,c) __stats_CounterCreate( VLC_OBJECT(a), b, c )
counter_t * __stats_CounterCreate (vlc_object_t*, int, int);
#define stats_Get(a,b,c) __stats_Get( VLC_OBJECT(a), b, c)
int __stats_Get (vlc_object_t*, counter_t *, vlc_value_t*);

void stats_CounterClean (counter_t * );

#define stats_GetInteger(a,b,c) __stats_GetInteger( VLC_OBJECT(a), b, c )
static inline int __stats_GetInteger( vlc_object_t *p_obj, counter_t *p_counter,
                                      int *value )
{
    int i_ret;
    vlc_value_t val; val.i_int = 0;
    if( !p_counter ) return VLC_EGENERIC;
    i_ret = __stats_Get( p_obj, p_counter, &val );
    *value = val.i_int;
    return i_ret;
}

#define stats_GetFloat(a,b,c) __stats_GetFloat( VLC_OBJECT(a), b, c )
static inline int __stats_GetFloat( vlc_object_t *p_obj, counter_t *p_counter,
                                    float *value )
{
    int i_ret;
    vlc_value_t val; val.f_float = 0.0;
    if( !p_counter ) return VLC_EGENERIC;
    i_ret = __stats_Get( p_obj, p_counter, &val );
    *value = val.f_float;
    return i_ret;
}
#define stats_UpdateInteger(a,b,c,d) __stats_UpdateInteger( VLC_OBJECT(a),b,c,d )
static inline int __stats_UpdateInteger( vlc_object_t *p_obj,counter_t *p_co,
                                         int i, int *pi_new )
{
    int i_ret;
    vlc_value_t val;
    vlc_value_t new_val; new_val.i_int = 0;
    if( !p_co ) return VLC_EGENERIC;
    val.i_int = i;
    i_ret = __stats_Update( p_obj, p_co, val, &new_val );
    if( pi_new )
        *pi_new = new_val.i_int;
    return i_ret;
}
#define stats_UpdateFloat(a,b,c,d) __stats_UpdateFloat( VLC_OBJECT(a),b,c,d )
static inline int __stats_UpdateFloat( vlc_object_t *p_obj, counter_t *p_co,
                                       float f, float *pf_new )
{
    vlc_value_t val;
    int i_ret;
    vlc_value_t new_val;new_val.f_float = 0.0;
    if( !p_co ) return VLC_EGENERIC;
    val.f_float = f;
    i_ret =  __stats_Update( p_obj, p_co, val, &new_val );
    if( pf_new )
        *pf_new = new_val.f_float;
    return i_ret;
}

VLC_EXPORT( void, stats_ComputeInputStats, (input_thread_t*, input_stats_t*) );
VLC_EXPORT( void, stats_ReinitInputStats, (input_stats_t *) );
VLC_EXPORT( void, stats_DumpInputStats, (input_stats_t *) );

/*
 * Replacement functions
 */
# ifndef HAVE_DIRENT_H
typedef void DIR;
#  ifndef FILENAME_MAX
#      define FILENAME_MAX (260)
#  endif
struct dirent
{
    long            d_ino;          /* Always zero. */
    unsigned short  d_reclen;       /* Always zero. */
    unsigned short  d_namlen;       /* Length of name in d_name. */
    char            d_name[FILENAME_MAX]; /* File name. */
};
#  define opendir vlc_opendir
#  define readdir vlc_readdir
#  define closedir vlc_closedir
#  define rewinddir vlc_rewindir
void *vlc_opendir (const char *);
void *vlc_readdir (void *);
int   vlc_closedir(void *);
void  vlc_rewinddir(void *);
# endif

#if defined (WIN32)
#   include <dirent.h>
void *vlc_wopendir (const wchar_t *);
/* void *vlc_wclosedir (void *); in vlc's exported symbols */
struct _wdirent *vlc_wreaddir (void *);
void vlc_rewinddir (void *);
#   define _wopendir vlc_wopendir
#   define _wreaddir vlc_wreaddir
#   define _wclosedir vlc_wclosedir
#   define rewinddir vlc_rewinddir
#endif

#endif
