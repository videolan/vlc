/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/* Number of tries before we unload an unused module */
#define MODULE_HIDE_DELAY 10

/* The module handle type. */
#ifdef SYS_BEOS
typedef int     module_handle_t;
#else
typedef void *  module_handle_t;
#endif

/*****************************************************************************
 * Configuration structure
 *****************************************************************************/
typedef struct module_config_s
{
    int         i_type;                         /* Configuration widget type */
    char *      psz_text;        /* Text commenting or describing the widget */
    char *      psz_name;                                   /* Variable name */
    void *      p_getlist;          /* Function to call to get a choice list */
    void *      p_change;        /* Function to call when commiting a change */
} module_config_t;

/*****************************************************************************
 * Bank and module description structures
 *****************************************************************************/

/* The main module structure */
typedef struct module_s
{
    boolean_t           b_builtin;  /* Set to true if the module is built in */

    module_handle_t     handle;      /* Unique handle to refer to the module */
    char *              psz_filename;                     /* Module filename */

    char *              psz_name;                    /* Module _unique_ name */
    char *              psz_longname;             /* Module descriptive name */
    char *              psz_version;                       /* Module version */

    int                 i_usage;                        /* Reference counter */
    int                 i_unused_delay;    /* Delay until module is unloaded */

    struct module_s *   next;                                 /* Next module */
    struct module_s *   prev;                             /* Previous module */

    u32                 i_capabilities;                   /* Capability list */

    module_config_t *   p_config;    /* Module configuration structure table */

} module_t;

/* The module bank structure */
typedef struct module_bank_s
{
    module_t *          first; /* First module of the bank */

    vlc_mutex_t         lock;  /* Global lock -- you can't imagine how awful it
                                  is to design thread-safe linked lists. */
} module_bank_t;

/*****************************************************************************
 * Exported functions.
 *****************************************************************************/
module_bank_t * module_InitBank     ( void );
int             module_DestroyBank  ( module_bank_t * p_bank );
int             module_ResetBank    ( module_bank_t * p_bank );
void            module_ManageBank   ( module_bank_t * p_bank );

int module_Need    ( module_t * p_module );
int module_Unneed  ( module_t * p_module );

