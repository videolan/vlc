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

extern const char vlc_usage[];

/* Hotkey stuff */
extern const struct hotkey libvlc_hotkeys[];
extern const size_t libvlc_hotkeys_size;
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
#define VLC_OBJECT_INTF        (-4)
#define VLC_OBJECT_PACKETIZER  (-13)

/*
 * Threads subsystem
 */

/* Hopefully, no need to export this. There is a new thread API instead. */
void vlc_thread_cancel (vlc_object_t *);
int vlc_object_waitpipe (vlc_object_t *obj);

void vlc_trace (const char *fn, const char *file, unsigned line);
#define vlc_backtrace() vlc_trace(__func__, __FILE__, __LINE__)

#if defined (LIBVLC_USE_PTHREAD) && !defined (NDEBUG)
# define vlc_assert_locked( m ) \
         assert (pthread_mutex_lock (m) == EDEADLK)
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
    vlc_mutex_t lock;
    vlc_cond_t  wait;

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

    /* Object variables */
    variable_t *    p_vars;
    vlc_mutex_t     var_lock;
    vlc_cond_t      var_wait;
    int             i_vars;

    /* Thread properties, if any */
    vlc_thread_t    thread_id;
    bool            b_thread;

    /* Objects thread synchronization */
    vlc_mutex_t     lock;
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

typedef struct sap_handler_t sap_handler_t;

/**
 * Private LibVLC instance data.
 */
typedef struct libvlc_priv_t
{
    libvlc_int_t       public_data;
    vlc_cond_t         exiting; ///< signaled when VLC wants to exit

    /* Configuration */
    vlc_mutex_t        config_lock; ///< config file lock
    char *             psz_configfile;   ///< location of config file

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

    void              *p_stats_computer;  ///< Input thread computing stats
                                          /// (needs cleanup)

    /* Singleton objects */
    module_t          *p_memcpy_module;  ///< Fast memcpy plugin used
    playlist_t        *p_playlist; //< the playlist singleton
    vlm_t             *p_vlm;  ///< the VLM singleton (or NULL)
    interaction_t     *p_interaction;    ///< interface interaction object
    intf_thread_t     *p_interaction_intf; ///< XXX interface for interaction
    httpd_t           *p_httpd; ///< HTTP daemon (src/network/httpd.c)
#ifdef ENABLE_SOUT
    sap_handler_t     *p_sap; ///< SAP SDP advertiser
#endif
    vlc_mutex_t        structure_lock;
} libvlc_priv_t;

static inline libvlc_priv_t *libvlc_priv (libvlc_int_t *libvlc)
{
    return (libvlc_priv_t *)libvlc;
}

void playlist_ServicesDiscoveryKillAll( playlist_t *p_playlist );

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
