/*****************************************************************************
 * main.h: access to all program variables
 * Declaration and extern access to global program object.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: main.h,v 1.16 2001/04/29 02:48:51 stef Exp $
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
 * main_t, p_main (global variable)
 *****************************************************************************
 * This structure has an unique instance, declared in main and pointed by the
 * only global variable of the program. It should allow access to any variable
 * of the program, for user-interface purposes or more easier call of interface
 * and common functions (example: the intf_*Msg functions). Please avoid using
 * it when you can access the members you need in an other way. In fact, it
 * should only be used by interface thread.
 *****************************************************************************/

typedef struct
{
    /* Global properties */
    int                    i_argc;           /* command line arguments count */
    char **                ppsz_argv;              /* command line arguments */
    char **                ppsz_env;                /* environment variables */
    char *                 psz_arg0;         /* program name (whithout path) */

    int                    i_cpu_capabilities;             /* CPU extensions */
    int                    i_warning_level;        /* warning messages level */

    /* Generic settings */
    boolean_t              b_audio;             /* is audio output allowed ? */
    boolean_t              b_video;             /* is video output allowed ? */
    boolean_t              b_channels;    /* is channel changing supported ? */
    boolean_t              b_spdif;                          /* spdif mode ? */

    /* Unique threads */
    p_vout_thread_t        p_vout;                    /* video output thread */
    p_aout_thread_t        p_aout;                    /* audio output thread */
    p_intf_thread_t        p_intf;                  /* main interface thread */

    /* Shared data - these structures are accessed directly from p_main by
     * several modules */
    struct module_bank_s * p_bank;                            /* module bank */
    p_playlist_t           p_playlist;                           /* playlist */
    p_intf_msg_t           p_msg;                 /* messages interface data */
    p_input_channel_t      p_channel;                /* channel library data */
} main_t;

extern main_t *p_main;

/*****************************************************************************
 * Prototypes - these methods are used to get default values for some threads
 * and modules.
 *****************************************************************************/
int    main_GetIntVariable( char *psz_name, int i_default );
char * main_GetPszVariable( char *psz_name, char *psz_default );
void   main_PutIntVariable( char *psz_name, int i_value );
void   main_PutPszVariable( char *psz_name, char *psz_value );

