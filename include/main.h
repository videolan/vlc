/*****************************************************************************
 * main.h: access to all program variables
 * Declaration and extern access to global program object.
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 VideoLAN
 * $Id: main.h,v 1.44 2002/08/12 09:34:15 sam Exp $
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
 * vlc_t, p_vlc (global variable)
 *****************************************************************************
 * This structure has an unique instance, declared in main and pointed by the
 * only global variable of the program. It should allow access to any variable
 * of the program, for user-interface purposes or more easier call of interface
 * and common functions (example: the intf_*Msg functions). Please avoid using
 * it when you can access the members you need in an other way. In fact, it
 * should only be used by interface thread.
 *****************************************************************************/
struct vlc_t
{
    VLC_COMMON_MEMBERS

    /* The vlc structure status */
    int                    i_status;

    /* Global properties */
    int                    i_argc;           /* command line arguments count */
    char **                ppsz_argv;              /* command line arguments */
    char *                 psz_homedir;             /* user's home directory */

    u32                    i_cpu;                          /* CPU extensions */

    /* Generic settings */
    vlc_bool_t             b_quiet;                            /* be quiet ? */
    vlc_bool_t             b_verbose;                     /* info messages ? */
    vlc_bool_t             b_color;                      /* color messages ? */
    mtime_t                i_desync;   /* relative desync of the audio ouput */

    /* Fast memcpy plugin used */
    module_t *             p_memcpy_module;
    void* ( *pf_memcpy ) ( void *, const void *, size_t );
    void* ( *pf_memset ) ( void *, int, size_t );

    /* The module bank */
    module_bank_t *        p_module_bank;

    /* The message bank */
    msg_bank_t             msg_bank;

    /* Shared data - these structures are accessed directly from p_vlc by
     * several modules */
    input_channel_t *      p_channel;                /* channel library data */

    /* Locks */
    vlc_mutex_t            config_lock;          /* lock for the config file */
    vlc_mutex_t            structure_lock;        /* lock for the p_vlc tree */

    /* Object structure data */
    int                    i_unique;                    /* p_vlc occurence # */
    int                    i_counter;                      /* object counter */
    int                    i_objects;              /* Attached objects count */
    vlc_object_t **        pp_objects;               /* Array of all objects */

    /* Pointer to the big, evil global lock */
    vlc_mutex_t *          p_global_lock;
    void **                pp_global_data;

    /* System-specific variables */
#if defined( SYS_BEOS )
    vlc_object_t *         p_appthread;
#elif defined( WIN32 )
    SIGNALOBJECTANDWAIT    SignalObjectAndWait;
    vlc_bool_t             b_fast_mutex;
    int                    i_win9x_cv;
#endif
};

