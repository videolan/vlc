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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * libvlc_t (global variable)
 *****************************************************************************
 * This structure has an unique instance, statically allocated in main and
 * never accessed from the outside. It store once-initialized data such as
 * the CPU capabilities or the global lock.
 *****************************************************************************/
struct libvlc_t
{
    VLC_COMMON_MEMBERS

    /* Initialization boolean */
    vlc_bool_t             b_ready;

    /* CPU extensions */
    uint32_t               i_cpu;

    /* Generic settings */
    int                    i_verbose;                       /* info messages */
    vlc_bool_t             b_color;                       /* color messages? */

    /* Object structure data */
    int                    i_counter;                      /* object counter */
    int                    i_objects;              /* Attached objects count */
    vlc_object_t **        pp_objects;               /* Array of all objects */

    /* The message bank */
    msg_bank_t             msg_bank;

    /* UTF-8 conversion */
    vlc_mutex_t            from_locale_lock;
    vlc_mutex_t            to_locale_lock;
    vlc_iconv_t            from_locale;
    vlc_iconv_t            to_locale;

    /* The module bank */
    module_bank_t *        p_module_bank;

    /* Arch-specific variables */
#if !defined( WIN32 )
    vlc_bool_t             b_daemon;
#endif 
#if defined( SYS_BEOS )
    vlc_object_t *         p_appthread;
    char *                 psz_vlcpath;
#elif defined( SYS_DARWIN )
    char *                 psz_vlcpath;
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
 * vlc_t, p_vlc
 *****************************************************************************
 * This structure is a LibVLC instance.
 *****************************************************************************/
struct vlc_t
{
    VLC_COMMON_MEMBERS

    /* Global properties */
    int                    i_argc;           /* command line arguments count */
    char **                ppsz_argv;              /* command line arguments */
    char *                 psz_homedir;             /* user's home directory */
    char *                 psz_configfile;        /* location of config file */

    /* Fast memcpy plugin used */
    module_t *             p_memcpy_module;
    void* ( *pf_memcpy ) ( void *, const void *, size_t );
    void* ( *pf_memset ) ( void *, int, size_t );

    /* Shared data - these structures are accessed directly from p_vlc by
     * several modules */

    /* Locks */
    vlc_mutex_t            config_lock;          /* lock for the config file */
#ifdef SYS_DARWIN
    vlc_mutex_t            quicktime_lock;          /* QT is not thread safe on OSX */
#endif

    /* Structure storing the action name / key associations */
    struct hotkey
    {
        const char *psz_action;
        int i_action;
        int i_key;

    } *p_hotkeys;
};

