/*****************************************************************************
 * xosd.c : X On Screen Display interface
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: xosd.c,v 1.4 2002/07/20 18:01:43 sam Exp $
 *
 * Authors: Loïc Minier <lool@videolan.org>
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

#include <xosd.h>

#include <vlc/intf.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of rc interface
 *****************************************************************************/
struct intf_sys_t
{
    input_thread_t * p_input;   /* associated input thread */
    xosd * p_osd;               /* libxosd handle */
    char * psz_source;          /* current file || NULL */
};

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void intf_getfunctions ( function_list_t * p_function_list );
static int  intf_Open         ( intf_thread_t *p_intf );
static void intf_Close        ( intf_thread_t *p_intf );
static void intf_Run          ( intf_thread_t *p_intf );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
#define POSITION_TEXT N_("flip vertical position")
#define POSITION_LONGTEXT N_("Display xosd output on the bottom of the " \
                             "screen instead of the top")

#define TXT_OFS_TEXT N_("vertical offset")
#define TXT_OFS_LONGTEXT N_("Vertical offset in pixels of the displayed text")

#define SHD_OFS_TEXT N_("shadow offset")
#define SHD_OFS_LONGTEXT N_("Offset in pixels of the shadow")

#define FONT_TEXT N_("font")
#define FONT_LONGTEXT N_("Font used to display text in the xosd output")

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_BOOL( "xosd-position", 1, NULL, POSITION_TEXT, POSITION_LONGTEXT )
ADD_INTEGER( "xosd-text-offset", 0, NULL, TXT_OFS_TEXT, TXT_OFS_LONGTEXT )
ADD_INTEGER( "xosd-shadow-offset", 1, NULL, SHD_OFS_TEXT, SHD_OFS_LONGTEXT )
ADD_STRING( "xosd-font", "-misc-fixed-medium-r-*-*-*-300-*-*-*-*-*-*", NULL, FONT_TEXT, FONT_LONGTEXT )
MODULE_CONFIG_STOP
// -misc-fixed-medium-r-normal-*-*-160-*-*-c-*-iso8859-15

MODULE_INIT_START
    SET_DESCRIPTION( _("xosd interface module") )
    ADD_CAPABILITY( INTF, 40 )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Open: initialize and create stuff
 *****************************************************************************/
static int intf_Open( intf_thread_t * p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    }

    /* Initialize library */
    p_intf->p_sys->p_osd =
        xosd_init( "fixed", "LawnGreen", 3, XOSD_top, 0, 1 );

    /* Initialize to NULL */
    p_intf->p_sys->psz_source = NULL;

    xosd_display( p_intf->p_sys->p_osd,
                  0,
                  XOSD_string,
                  "xosd interface initialized" );
    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface stuff
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->psz_source ) free( p_intf->p_sys->psz_source );

    /* Uninitialize library */
    xosd_uninit( p_intf->p_sys->p_osd );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: xosd thread
 *****************************************************************************
 * This part of the interface runs in a separate thread
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    p_intf->p_sys->p_input = NULL;

    while( !p_intf->b_die )
    {
        /* Manage the input part */
        if( p_intf->p_sys->p_input == NULL )
        {
            p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                              FIND_ANYWHERE );
        }
        else if( p_intf->p_sys->p_input->b_dead )
        {
            vlc_object_release( p_intf->p_sys->p_input );
            p_intf->p_sys->p_input = NULL;
        }
        else /* We have a valid input */
        {
            /* Did source change? */
            if ( (p_intf->p_sys->psz_source == NULL)
                 || (strcmp( p_intf->p_sys->psz_source,
                             p_intf->p_sys->p_input->psz_source ) != 0)
               )
            {
                if( p_intf->p_sys->psz_source )
                    free( p_intf->p_sys->psz_source );

                p_intf->p_sys->psz_source =
                    strdup( p_intf->p_sys->p_input->psz_source );

                /* Set user preferences */
                xosd_set_font( p_intf->p_sys->p_osd,
                               config_GetPsz( p_intf, "xosd-font" ) );
                xosd_set_offset( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-text-offset" ) );
                xosd_set_shadow_offset( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-shadow-offset" ));
                xosd_set_pos( p_intf->p_sys->p_osd,
                    config_GetInt( p_intf, "xosd-position" ) ? XOSD_bottom
                                                             : XOSD_top );

                /* Display */
                xosd_display( p_intf->p_sys->p_osd,
                              0,                               /* first line */
                              XOSD_string,
                              p_intf->p_sys->psz_source );
            }
        }

        msleep( INTF_IDLE_SLEEP );
    }

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

}

