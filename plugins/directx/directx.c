/*****************************************************************************
 * directx.c : Windows DirectX plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: directx.c,v 1.10 2002/05/30 08:17:04 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list );
void _M( vout_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
#define HW_YUV_TEXT N_("use hardware YUV->RGB conversions")
#define HW_YUV_LONGTEXT N_( \
    "Try to use hardware acceleration for YUV->RGB conversions. " \
    "This option doesn't have any effect when using overlays." )
#define SYSMEM_TEXT N_("use video buffers in system memory")
#define SYSMEM_LONGTEXT N_( \
    "Create video buffers in system memory instead of video memory. This " \
    "isn't recommended as usually using video memory allows to benefit from " \
    "more hardware acceleration (like rescaling or YUV->RGB conversions). " \
    "This option doesn't have any effect when using overlays." )

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Video"), NULL )
ADD_BOOL ( "directx-hw-yuv", 1, NULL, HW_YUV_TEXT, HW_YUV_LONGTEXT )
ADD_BOOL ( "directx-use-sysmem", 0, NULL, SYSMEM_TEXT, SYSMEM_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("DirectX extension module") )
    ADD_CAPABILITY( AOUT, 150 )
    ADD_CAPABILITY( VOUT, 150 )
    ADD_SHORTCUT( "directx" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( aout_getfunctions )( &p_module->p_functions->aout );
    _M( vout_getfunctions )( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
    /* check if we registered a window class because we need to
     * unregister it */
    WNDCLASS wndclass;
    if( GetClassInfo( GetModuleHandle(NULL), "VLC DirectX", &wndclass ) )
        UnregisterClass( "VLC DirectX", GetModuleHandle(NULL) );
MODULE_DEACTIVATE_STOP
