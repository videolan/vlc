/*****************************************************************************
 * intf_channels.c: channel handling functions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: intf_channels.c,v 1.2 2001/03/21 13:42:34 sam Exp $
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
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                                   /* FILE */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"
#include "intf_channels.h"
#include "interface.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int ParseChannel( intf_channel_t *p_channel, char *psz_str );

/*****************************************************************************
 * intf_LoadChannels: load channels description from a file
 *****************************************************************************
 * This structe describes all interface-specific data of the main (interface)
 * thread.
 * Each line of the file is a semicolon separated list of the following
 * fields :
 *      integer         channel number
 *      string          channel description
 *      integer         input method (see input.h)
 *      string          input source
 *      integer         input port
 *      integer         input vlan id
 * The last field must end with a semicolon.
 * Comments and empty lines are not explicitely allowed, but lines with parsing
 * errors are ignored without warning.
 *****************************************************************************/
int intf_LoadChannels( intf_thread_t *p_intf, char *psz_filename )
{
    FILE *              p_file;                                      /* file */
    intf_channel_t *    p_channel;                        /* current channel */
    char                psz_line[INTF_MAX_CMD_SIZE];          /* line buffer */
    int                 i_index;                   /* channel or field index */

    /* Set default value */
    p_intf->p_channel = NULL;

    /* Open file */
    p_file = fopen( psz_filename, "r" );
    if( p_file == NULL )
    {
        intf_DbgMsg( "intf warning: cannot open %s (%s)",
                     psz_filename, strerror(errno) );
        return( 1 );
    }

    /* First pass: count number of lines */
    for( i_index = 0; fgets( psz_line, INTF_MAX_CMD_SIZE, p_file ) != NULL;
         i_index++ )
    {
        ;
    }

    if( i_index != 0 )
    {
        /* Allocate array and rewind - some of the lines may be invalid, and the
         * array will probably be larger than the actual number of channels, but
         * it has no consequence. */
        p_intf->p_channel = malloc( sizeof( intf_channel_t ) * i_index );
        if( p_intf->p_channel == NULL )
        {
            intf_ErrMsg( "intf error: cannot create intf_channel_t (%s)",
                         strerror(ENOMEM) );
            fclose( p_file );
            return( 1 );
        }
        p_channel = p_intf->p_channel;
        rewind( p_file );

        /* Second pass: read channels descriptions */
        while( fgets( psz_line, INTF_MAX_CMD_SIZE, p_file ) != NULL )
        {
            if( !ParseChannel( p_channel, psz_line ) )
            {
                intf_DbgMsg( "channel [%d] %s : method %d (%s:%d vlan id %d)",
                         p_channel->i_channel, p_channel->psz_description,
                         p_channel->i_input_method,
                         p_channel->psz_input_source,
                         p_channel->i_input_port, p_channel->i_input_vlan_id );
                p_channel++;
            }
        }

        /* Add marker at the end of the array */
        p_channel->i_channel = -1;
    }

    /* Close file */
    fclose( p_file );
    return( 0 );
}

/*****************************************************************************
 * intf_UnloadChannels: unload channels description
 *****************************************************************************
 * This function free all resources allocated by LoadChannels, if any.
 *****************************************************************************/
void intf_UnloadChannels( intf_thread_t *p_intf )
{
    int i_channel;                                          /* channel index */

    if( p_intf->p_channel != NULL )
    {
        /* Free allocated strings */
        for( i_channel = 0;
             p_intf->p_channel[ i_channel ].i_channel != -1;
             i_channel++ )
        {
            if( p_intf->p_channel[ i_channel ].psz_description != NULL )
            {
                free( p_intf->p_channel[ i_channel ].psz_description );
            }
            if( p_intf->p_channel[ i_channel ].psz_input_source != NULL )
            {
                free( p_intf->p_channel[ i_channel ].psz_input_source );
            }
        }

        /* Free array */
        free( p_intf->p_channel );
        p_intf->p_channel = NULL;
    }
}

/*****************************************************************************
 * intf_SelectChannel: change channel
 *****************************************************************************
 * Kill existing input, if any, and try to open a new one, using an input
 * configuration table.
 *****************************************************************************/
int intf_SelectChannel( intf_thread_t * p_intf, int i_channel )
{
    /* FIXME */
#if 0
    intf_channel_t *    p_channel;                                /* channel */

    /* Look for channel in array */
    if( p_intf->p_channel != NULL )
    {
        for( p_channel = p_intf->p_channel; p_channel->i_channel != -1; p_channel++ )
        {
            if( p_channel->i_channel == i_channel )
            {
            /*
             * Change channel
             */

            /* Kill existing input, if any */
            if( p_intf->p_input != NULL )
            {
                input_DestroyThread( p_intf->p_input, NULL );
            }

            intf_Msg("Channel %d: %s", i_channel, p_channel->psz_description );

            /* Open a new input */
            p_intf->p_input = input_CreateThread( p_channel->i_input_method, p_channel->psz_input_source,
                                  p_channel->i_input_port, p_channel->i_input_vlan_id,
                                  p_intf->p_vout, p_main->p_aout, NULL );
            return( p_intf->p_input == NULL );
            }
        }
    }

    /* Channel does not exist */
    intf_Msg("Channel %d does not exist", i_channel );
#endif
    return( 1 );
}

/* Following functions are local */

/*****************************************************************************
 * ParseChannel: parse a channel description line
 *****************************************************************************
 * See intf_LoadChannels. This function return non 0 on parsing error.
 *****************************************************************************/
static int ParseChannel( intf_channel_t *p_channel, char *psz_str )
{
    char *      psz_index;                              /* current character */
    char *      psz_end;                           /* end pointer for strtol */
    int         i_field;                        /* field number, -1 on error */
    int         i_field_length;             /* field length, for text fields */

    /* Set some default fields */
    p_channel->i_channel =              0;
    p_channel->psz_description =        NULL;
    p_channel->i_input_method =         0;
    p_channel->psz_input_source =       NULL;
    p_channel->i_input_port =           0;
    p_channel->i_input_vlan_id =        0;

    /* Parse string */
    i_field = 0;
    for( psz_index = psz_str; (i_field != -1) && (*psz_index != '\0'); psz_index++ )
    {
        if( *psz_index == ';' )
        {
            /* Mark end of field */
            *psz_index = '\0';

            /* Parse field */
            switch( i_field++ )
            {
            case 0:                                        /* channel number */
                p_channel->i_channel = strtol( psz_str, &psz_end, 0);
                if( (*psz_str == '\0') || (*psz_end != '\0') )
                {
                    i_field = -1;
                }
                break;
            case 1:                                   /* channel description */
                i_field_length = strlen( psz_str );
                if( i_field_length != 0 )
                {
                    p_channel->psz_description = malloc( i_field_length + 1 );
                    if( p_channel->psz_description == NULL )
                    {
                        intf_ErrMsg( "intf error: cannot create channel "
                                     "description (%s)", strerror( ENOMEM ) );
                        i_field = -1;
                    }
                    else
                    {
                        strcpy( p_channel->psz_description, psz_str );
                    }
                }
                break;
            case 2:                                          /* input method */
                p_channel->i_input_method = strtol( psz_str, &psz_end, 0);
                if( (*psz_str == '\0') || (*psz_end != '\0') )
                {
                    i_field = -1;
                }
                break;
            case 3:                                          /* input source */
                i_field_length = strlen( psz_str );
                if( i_field_length != 0 )
                {
                    p_channel->psz_input_source = malloc( i_field_length + 1 );
                    if( p_channel->psz_input_source == NULL )
                    {
                        intf_ErrMsg( "intf error: cannot create input "
                                     "source (%s)", strerror( ENOMEM ) );
                        i_field = -1;
                    }
                    else
                    {
                        strcpy( p_channel->psz_input_source, psz_str );
                    }
                }
                break;
            case 4:                                            /* input port */
                p_channel->i_input_port = strtol( psz_str, &psz_end, 0);
                if( (*psz_str == '\0') || (*psz_end != '\0') )
                {
                    i_field = -1;
                }
                break;
            case 5:                                          /* input vlan id */
                p_channel->i_input_vlan_id = strtol( psz_str, &psz_end, 0);
                if( (*psz_str == '\0') || (*psz_end != '\0') )
                {
                    i_field = -1;
                }
                break;
                /* ... following fields are ignored */
            }

            /* Set new beginning of field */
            psz_str = psz_index + 1;
        }
    }

    /* At least the first three fields must be parsed sucessfully for function
     * success. Other parsing errors are returned using i_field = -1. */
    if( i_field < 3 )
    {
        /* Function fails. Free allocated strings */
        if( p_channel->psz_description != NULL )
        {
            free( p_channel->psz_description );
        }
        if( p_channel->psz_input_source != NULL )
        {
            free( p_channel->psz_input_source );
        }
        return( 1 );
    }

    /* Return success */
    return( 0 );
}

