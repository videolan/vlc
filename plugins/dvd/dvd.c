/*****************************************************************************
 * dvd.c : DVD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: dvd.c,v 1.9 2001/05/30 17:03:12 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#ifdef HAVE_CSS
#define MODULE_NAME dvd
#else /* HAVE_CSS */
#define MODULE_NAME dvdnocss
#endif /* HAVE_CSS */
#include "modules_inner.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"

#include "modules.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for DVD module" )
    ADD_COMMENT( "foobar !" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_INPUT;
#ifdef HAVE_CSS
    p_module->psz_longname = "full DVD input module with CSS decryption";
#else /* HAVE_CSS */
    p_module->psz_longname = "DVD input module, CSS decryption disabled";
#endif /* HAVE_CSS */
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( input_getfunctions )( &p_module->p_functions->input );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

#ifdef HAVE_CSS
#else /* HAVE_CSS */
#ifdef BUILTIN
int module_dvd_InitModule( module_t *p_module )
{
    return module_dvdnocss_InitModule( p_module );
}

int module_dvd_ActivateModule( module_t *p_module )
{
    return module_dvdnocss_ActivateModule( p_module );
}

int module_dvd_DeactivateModule( module_t *p_module )
{
    return module_dvdnocss_DeactivateModule( p_module );
}
#endif /* BUILTIN */
#endif /* HAVE_CSS */

