/*****************************************************************************
 * sdl.c : SDL plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
 *      . Initial plugin code by Samuel Hocevar <sam@via.ecp.fr>
 *      . Modified to use the SDL by Pierre Baillet <octplane@via.ecp.fr>
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

#define MODULE_NAME sdl

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "plugins.h"

#include "interface.h"
#include "audio_output.h"
#include "video.h"
#include "video_output.h"

/* audio includes */
#include "modules.h"
#include "modules_inner.h"

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for SDL module" )
    ADD_COMMENT( "For now, the SDL module cannot be configured" )
MODULE_CONFIG_END

/*****************************************************************************
 * Capabilities defined in the other files.
 ******************************************************************************/
extern void aout_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * InitModule: get the module structure and configuration.
 *****************************************************************************
 * We have to fill psz_name, psz_longname and psz_version. These variables
 * will be strdup()ed later by the main application because the module can
 * be unloaded later to save memory, and we want to be able to access this
 * data even after the module has been unloaded.
 *****************************************************************************/
int InitModule( module_t * p_module )
{
    p_module->psz_name = MODULE_STRING;
    p_module->psz_longname = "Simple DirectMedia Layer module";
    p_module->psz_version = VERSION;

    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_AOUT;

    return( 0 );
}

/*****************************************************************************
 * ActivateModule: set the module to an usable state.
 *****************************************************************************
 * This function fills the capability functions and the configuration
 * structure. Once ActivateModule() has been called, the i_usage can
 * be set to 0 and calls to NeedModule() be made to increment it. To unload
 * the module, one has to wait until i_usage == 0 and call DeactivateModule().
 *****************************************************************************/
int ActivateModule( module_t * p_module )
{
    p_module->p_functions = malloc( sizeof( module_functions_t ) );
    if( p_module->p_functions == NULL )
    {
        return( -1 );
    }

    aout_getfunctions( &p_module->p_functions->aout );

    p_module->p_config = p_config;

    return( 0 );
}

/*****************************************************************************
 * DeactivateModule: make sure the module can be unloaded.
 *****************************************************************************
 * This function must only be called when i_usage == 0. If it successfully
 * returns, i_usage can be set to -1 and the module unloaded. Be careful to
 * lock usage_lock during the whole process.
 *****************************************************************************/
int DeactivateModule( module_t * p_module )
{
    free( p_module->p_functions );

    return( 0 );
}

/* old plugin API */

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static void vout_GetPlugin( p_vout_thread_t p_vout );
static void intf_GetPlugin( p_intf_thread_t p_intf );

/* Video output */
int     vout_SDLCreate       ( vout_thread_t *p_vout, char *psz_display,
                               int i_root_window, void *p_data );
int     vout_SDLInit         ( p_vout_thread_t p_vout );
void    vout_SDLEnd          ( p_vout_thread_t p_vout );
void    vout_SDLDestroy      ( p_vout_thread_t p_vout );
int     vout_SDLManage       ( p_vout_thread_t p_vout );
void    vout_SDLDisplay      ( p_vout_thread_t p_vout );
void    vout_SDLSetPalette   ( p_vout_thread_t p_vout,
                               u16 *red, u16 *green, u16 *blue, u16 *transp );
/* Interface */
int     intf_SDLCreate       ( p_intf_thread_t p_intf );
void    intf_SDLDestroy      ( p_intf_thread_t p_intf );
void    intf_SDLManage       ( p_intf_thread_t p_intf );


/*****************************************************************************
 * GetConfig: get the plugin structure and configuration
 *****************************************************************************/
plugin_info_t * GetConfig( void )
{
    plugin_info_t * p_info = (plugin_info_t *) malloc( sizeof(plugin_info_t) );

    p_info->psz_name    = "SDL (video)";
    p_info->psz_version = VERSION;
    p_info->psz_author  = "the VideoLAN team <vlc@videolan.org>";

    p_info->aout_GetPlugin = NULL;
    p_info->vout_GetPlugin = vout_GetPlugin;
    p_info->intf_GetPlugin = intf_GetPlugin;
    p_info->yuv_GetPlugin = NULL;
  
    
    /* if the SDL libraries are there, assume we can enter the
     * initialization part at least, even if we fail afterwards */
    
    p_info->i_score = 0x100;
    
    /* If this plugin was requested, score it higher */
    if( TestMethod( VOUT_METHOD_VAR, "sdl" ) )
    {
        p_info->i_score += 0x200;
    }

    return( p_info );
}

/*****************************************************************************
 * Following functions are only called through the p_info structure
 *****************************************************************************/

static void vout_GetPlugin( p_vout_thread_t p_vout )
{
    p_vout->p_sys_create  = vout_SDLCreate;
    p_vout->p_sys_init    = vout_SDLInit;
    p_vout->p_sys_end     = vout_SDLEnd;
    p_vout->p_sys_destroy = vout_SDLDestroy;
    p_vout->p_sys_manage  = vout_SDLManage;
    p_vout->p_sys_display = vout_SDLDisplay;
    p_vout->p_set_palette = vout_SDLSetPalette;

}

static void intf_GetPlugin( p_intf_thread_t p_intf )
{
    p_intf->p_sys_create  = intf_SDLCreate;
    p_intf->p_sys_destroy = intf_SDLDestroy;
    p_intf->p_sys_manage  = intf_SDLManage;
}


