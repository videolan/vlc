/*****************************************************************************
 * main.h: access to all program variables
 * Declaration and extern access to global program object.
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 the VideoLAN team
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
    uint32_t               i_cpu;       ///< CPU extensions

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
#elif defined( WIN32 ) && !defined( UNDER_CE )
    SIGNALOBJECTANDWAIT    SignalObjectAndWait;
    vlc_bool_t             b_fast_mutex;
    int                    i_win9x_cv;
    char *                 psz_vlcpath;
#elif defined( UNDER_CE )
    char *                 psz_vlcpath;
#endif
};

/*****************************************************************************
 * libvlc_internal_instance_t
 *****************************************************************************
 * This structure is a LibVLC instance, for use by libvlc core and plugins
 *****************************************************************************/
struct libvlc_int_t
{
    VLC_COMMON_MEMBERS

    /* Global properties */
    int                    i_argc;           ///< command line arguments count
    char **                ppsz_argv;        ///< command line arguments
    char *                 psz_homedir;      ///< configuration directory
    char *                 psz_userdir;      ///< user's home directory
    char *                 psz_configfile;   ///< location of config file

    playlist_t            *p_playlist;       ///< playlist object

    /* Messages */
    msg_bank_t             msg_bank;    ///< The message bank
    int                    i_verbose;   ///< info messages
    vlc_bool_t             b_color;     ///< color messages?

    module_t *             p_memcpy_module;  ///< Fast memcpy plugin used
    void* ( *pf_memcpy ) ( void *, const void *, size_t ); ///< fast memcpy 
    void* ( *pf_memset ) ( void *, int, size_t );          ///< fast memset

    vlc_bool_t             b_stats;       ///< Should we collect stats ?
    vlc_mutex_t            timer_lock;    ///< Lock to protect timers
    int                    i_timers;      ///< Number of timers
    counter_t            **pp_timers;     ///< Array of all timers

    vlc_mutex_t            config_lock;    ///< Lock for the config file
#ifdef __APPLE__
    vlc_mutex_t            quicktime_lock; ///< QT is not thread safe on OSX
#endif

    /* Structure storing the action name / key associations */
    struct hotkey
    {
        const char *psz_action;
        int i_action;
        int i_key;

        /* hotkey accounting information */
        mtime_t i_delta_date;/*< minimum delta time between two key presses */
        mtime_t i_last_date; /*< last date key was pressed */
        int     i_times;     /*< n times pressed within delta date*/
    } *p_hotkeys;
};

