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

extern const char vlc_usage[];

extern const struct hotkey libvlc_hotkeys[];
extern const size_t libvlc_hotkeys_size;


/*
 * OS-specific initialization
 */
void system_Init      ( libvlc_int_t *, int *, const char *[] );
void system_Configure ( libvlc_int_t *, int *, const char *[] );
void system_End       ( libvlc_int_t * );

#if defined( SYS_BEOS )
/* Nothing at the moment, create beos_specific.h when needed */
#elif defined( __APPLE__ )
/* Nothing at the moment, create darwin_specific.h when needed */
#elif defined( WIN32 ) || defined( UNDER_CE )
VLC_EXPORT( const char * , system_VLCPath, (void));
#else
# define system_Init( a, b, c )      (void)0
# define system_Configure( a, b, c ) (void)0
# define system_End( a )             (void)0
#endif


/*
 * Threads subsystem
 */
int __vlc_threads_init( vlc_object_t * );
int __vlc_threads_end( vlc_object_t * );

/*
 * CPU capabilities
 */
extern uint32_t cpu_flags;
uint32_t CPUCapabilities( void );

/*
 * Unicode stuff
 */

/*
 * LibVLC objects stuff
 */

extern vlc_object_t *
vlc_custom_create (vlc_object_t *p_this, size_t i_size, int i_type,
                   const char *psz_type);

/*****************************************************************************
 * libvlc_global_data_t (global variable)
 *****************************************************************************
 * This structure has an unique instance, statically allocated in main and
 * never accessed from the outside. It stores once-initialized data such as
 * the CPU capabilities or the global lock.
 *****************************************************************************/
struct libvlc_global_data_t
{
    VLC_COMMON_MEMBERS

    vlc_bool_t             b_ready;     ///< Initialization boolean

   /* Object structure data */
    int                    i_counter;   ///< object counter
    int                    i_objects;   ///< Attached objects count
    vlc_object_t **        pp_objects;  ///< Array of all objects

    module_bank_t *        p_module_bank; ///< The module bank
    intf_thread_t         *p_probe;       ///< Devices prober

    /* Arch-specific variables */
#if !defined( WIN32 )
    vlc_bool_t             b_daemon;
#endif
#if defined( SYS_BEOS )
    vlc_object_t *         p_appthread;
    char *                 psz_vlcpath;
#elif defined( __APPLE__ )
    char *                 psz_vlcpath;
    vlc_iconv_t            iconv_macosx; /* for HFS+ file names */
    vlc_mutex_t            iconv_lock;
#elif defined( WIN32 )
    char *                 psz_vlcpath;
#endif
};


libvlc_global_data_t *vlc_global (void);
libvlc_int_t *vlc_current_object (int i_object);

/* Private LibVLC data for each objects */
struct vlc_object_internals_t
{
    /* Object variables */
    variable_t *    p_vars;
    vlc_mutex_t     var_lock;
    int             i_vars;

    /* Thread properties, if any */
    vlc_thread_t    thread_id;
    vlc_bool_t      b_thread;

    /* Objects thread synchronization */
    int             pipes[2];
    vlc_spinlock_t  spin;

    /* Objects management */
    unsigned        i_refcount;
    vlc_bool_t      b_attached;
};


static inline vlc_object_internals_t *vlc_internals( vlc_object_t *obj )
{
    return obj->p_internals;
}

/*
 * Configuration stuff
 */
#if 0
struct module_config_t
{
    int          i_type;                               /* Configuration type */
    const char  *psz_type;                          /* Configuration subtype */
    const char  *psz_name;                                    /* Option name */
    char         i_short;                      /* Optional short option name */
    const char  *psz_text;      /* Short comment on the configuration option */
    const char  *psz_longtext;   /* Long comment on the configuration option */
    module_value_t value;                                    /* Option value */
    module_value_t orig;
    module_value_t saved;
    module_nvalue_t min;
    module_nvalue_t max;

    /* Function to call when commiting a change */
    vlc_callback_t pf_callback;
    void          *p_callback_data;

    /* Values list */
    const char **ppsz_list;       /* List of possible values for the option */
    int         *pi_list;                              /* Idem for integers */
    const char **ppsz_list_text;          /* Friendly names for list values */
    int          i_list;                               /* Options list size */

    /* Actions list */
    vlc_callback_t *ppf_action;    /* List of possible actions for a config */
    const char    **ppsz_action_text;         /* Friendly names for actions */
    int            i_action;                           /* actions list size */

    /* Misc */
    vlc_mutex_t *p_lock;            /* Lock to use when modifying the config */
    vlc_bool_t   b_dirty;          /* Dirty flag to indicate a config change */
    vlc_bool_t   b_advanced;          /* Flag to indicate an advanced option */
    vlc_bool_t   b_internal;   /* Flag to indicate option is not to be shown */
    vlc_bool_t   b_restart;   /* Flag to indicate the option needs a restart */
                              /* to take effect */

    /* Deprecated */
    const char    *psz_current;                         /* Good option name */
    vlc_bool_t     b_strict;                     /* Transitionnal or strict */

    /* Option values loaded from config file */
    vlc_bool_t   b_autosave;      /* Config will be auto-saved at exit time */
    vlc_bool_t   b_unsaveable;                    /* Config should be saved */
};
#endif

extern module_config_t libvlc_config[];
extern const size_t libvlc_config_count;

/*
 * Variables stuff
 */
void var_OptionParse (vlc_object_t *, const char *);

#endif
