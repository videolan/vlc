/*****************************************************************************
 * interface.c: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as command line.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors:
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
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"
#include "playlist.h"
#include "input.h"

#include "audio_output.h"

#include "intf_msg.h"
#include "interface.h"
#include "intf_cmd.h"
#include "intf_console.h"

#include "video.h"
#include "video_output.h"

#include "main.h"

/*****************************************************************************
 * intf_channel_t: channel description
 *****************************************************************************
 * A 'channel' is a descriptor of an input method. It is used to switch easily
 * from source to source without having to specify the whole input thread
 * configuration. The channels array, stored in the interface thread object, is
 * loaded in intf_Create, and unloaded in intf_Destroy.
 *****************************************************************************/
typedef struct intf_channel_s
{
    /* Channel description */
    int         i_channel;            /* channel number, -1 for end of array */
    char *      psz_description;              /* channel description (owned) */

    /* Input configuration */
    int         i_input_method;                   /* input method descriptor */
    char *      psz_input_source;                   /* source string (owned) */
    int         i_input_port;                                        /* port */
    int         i_input_vlan;                                        /* vlan */
} intf_channel_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      LoadChannels    ( intf_thread_t *p_intf, char *psz_filename );
static void     UnloadChannels  ( intf_thread_t *p_intf );
static int      ParseChannel    ( intf_channel_t *p_channel, char *psz_str );

/*****************************************************************************
 * intf_Create: prepare interface before main loop
 *****************************************************************************
 * This function opens output devices and creates specific interfaces. It sends
 * its own error messages.
 *****************************************************************************/
intf_thread_t* intf_Create( void )
{
    intf_thread_t * p_intf;
    typedef void    ( intf_getplugin_t ) ( intf_thread_t * p_intf );
    int             i_index;
    int             i_best_index = 0, i_best_score = 0;

    /* Allocate structure */
    p_intf = malloc( sizeof( intf_thread_t ) );
    if( !p_intf )
    {
        intf_ErrMsg("error: %s\n", strerror( ENOMEM ) );
        return( NULL );
    }

    /* Get a suitable interface plugin */
    for( i_index = 0 ; i_index < p_main->p_bank->i_plugin_count ; i_index++ )
    {
        /* If there's a plugin in p_info ... */
        if( p_main->p_bank->p_info[ i_index ] != NULL )
        {
            /* ... and if this plugin provides the functions we want ... */
            if( p_main->p_bank->p_info[ i_index ]->intf_GetPlugin != NULL )
            {
                /* ... and if this plugin has a good score ... */
                if( p_main->p_bank->p_info[ i_index ]->i_score > i_best_score )
                {
                    /* ... then take it */
                    i_best_score = p_main->p_bank->p_info[ i_index ]->i_score;
                    i_best_index = i_index;
                }
            }
        }
    }

    if( i_best_score == 0 )
    {
        free( p_intf );
        return( NULL );
    }

    /* Get the plugin functions */
    ( (intf_getplugin_t *)
      p_main->p_bank->p_info[ i_best_index ]->intf_GetPlugin )( p_intf );

    /* Initialize structure */
    p_intf->b_die =     0;
    p_intf->p_vout =    NULL;
    p_intf->p_input =   NULL;
    p_intf->p_keys =    NULL;

    /* Load channels - the pointer will be set to NULL on failure. The
     * return value is ignored since the program can work without
     * channels */
    LoadChannels( p_intf, main_GetPszVariable( INTF_CHANNELS_VAR, INTF_CHANNELS_DEFAULT ));

    /* Start interfaces */
    p_intf->p_console = intf_ConsoleCreate();
    if( p_intf->p_console == NULL )
    {
        intf_ErrMsg("error: can't create control console\n");
        free( p_intf );
        return( NULL );
    }
    if( p_intf->p_sys_create( p_intf ) )
    {
        intf_ErrMsg("error: can't create interface\n");
        intf_ConsoleDestroy( p_intf->p_console );
        free( p_intf );
        return( NULL );
    }

    intf_Msg("Interface initialized\n");
    return( p_intf );
}

/*****************************************************************************
 * intf_Run
 *****************************************************************************
 * Initialization script and main interface loop.
 *****************************************************************************/
void intf_Run( intf_thread_t *p_intf )
{
    if( p_main->p_playlist->p_list )
    {
        p_intf->p_input = input_CreateThread( INPUT_METHOD_TS_FILE, NULL, 0, 0, p_main->p_intf->p_vout, p_main->p_aout, NULL );
    }
    /* Execute the initialization script - if a positive number is returned,
     * the script could be executed but failed */
    else if( intf_ExecScript( main_GetPszVariable( INTF_INIT_SCRIPT_VAR, INTF_INIT_SCRIPT_DEFAULT ) ) > 0 )
    {
        intf_ErrMsg("warning: error(s) during startup script\n");
    }

    /* Main loop */
    while(!p_intf->b_die)
    {
        /* Flush waiting messages */
        intf_FlushMsg();

        /* Manage specific interface */
        p_intf->p_sys_manage( p_intf );

        /* Check attached threads status */
        if( (p_intf->p_vout != NULL) && p_intf->p_vout->b_error )
        {
            /* FIXME: add aout error detection ?? */
            p_intf->b_die = 1;
        }
        if( (p_intf->p_input != NULL) && p_intf->p_input->b_error )
        {
            input_DestroyThread( p_intf->p_input, NULL );
            p_intf->p_input = NULL;
            intf_DbgMsg("Input thread destroyed\n");
        }

        /* Sleep to avoid using all CPU - since some interfaces needs to access
         * keyboard events, a 100ms delay is a good compromise */
        msleep( INTF_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * intf_Destroy: clean interface after main loop
 *****************************************************************************
 * This function destroys specific interfaces and close output devices.
 *****************************************************************************/
void intf_Destroy( intf_thread_t *p_intf )
{
    p_intf_key  p_cur;
    p_intf_key  p_next;
    /* Destroy interfaces */
    p_intf->p_sys_destroy( p_intf );
    intf_ConsoleDestroy( p_intf->p_console );

    /* Unload channels */
    UnloadChannels( p_intf );

    /* Destroy keymap */
    
    p_cur = p_intf->p_keys;
    while( p_cur != NULL)
    {
        p_next = p_cur->next;
        free(p_cur);
        p_cur = p_next;
     }
     
    
    /* Free structure */
    free( p_intf );
}

/*****************************************************************************
 * intf_SelectChannel: change channel
 *****************************************************************************
 * Kill existing input, if any, and try to open a new one, using an input
 * configuration table.
 *****************************************************************************/
int intf_SelectChannel( intf_thread_t * p_intf, int i_channel )
{
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

                intf_Msg("Channel %d: %s\n", i_channel, p_channel->psz_description );

                /* Open a new input */
                p_intf->p_input = input_CreateThread( p_channel->i_input_method, p_channel->psz_input_source,
                                                      p_channel->i_input_port, p_channel->i_input_vlan,
                                                      p_intf->p_vout, p_main->p_aout, NULL );
                return( p_intf->p_input == NULL );
            }
        }
    }

    /* Channel does not exist */
    intf_Msg("Channel %d does not exist\n", i_channel );
    return( 1 );
}

/*****************************************************************************
 * intf_AssignKey: assign standartkeys                                       *
 *****************************************************************************
 * This function fills in the associative array that links the key pressed   *
 * and the key we use internally.                                            *
 ****************************************************************************/

void intf_AssignKey( intf_thread_t *p_intf, int r_key, int f_key)
{
    p_intf_key  p_cur =  p_intf->p_keys;
  
    if( p_cur == NULL )
    {
        p_cur = (p_intf_key )(malloc ( sizeof( intf_key ) ) );
        p_cur->received_key = r_key;
        p_cur->forwarded_key = f_key;
        p_cur->next = NULL;
        p_intf->p_keys = p_cur;
    } else {
        while( p_cur->next != NULL && p_cur ->received_key != r_key)
        {
            p_cur = p_cur->next;
        }
        if( p_cur->next == NULL )
        {   
            p_cur->next  = ( p_intf_key )( malloc( sizeof( intf_key ) ) );
            p_cur = p_cur->next;
            p_cur->next = NULL;
            p_cur->received_key = r_key;
        }
        p_cur->forwarded_key = f_key;
    }        
}


int intf_getKey( intf_thread_t *p_intf, int r_key)
{
    p_intf_key current = p_intf->p_keys;
    while(current != NULL && current->received_key != r_key)
    {
        
        current = current->next;
    }
    if(current == NULL)
       /* didn't find any key in the array */ 
        return( -1 );
    else
        return( current->forwarded_key );
    /* well, something went wrong */
    return( -1 );
}


/*****************************************************************************
 * intf_AssignNormalKeys: used for normal interfaces.
 *****************************************************************************
 * This function assign the basic key to the normal keys.
 *****************************************************************************/

void intf_AssignNormalKeys( intf_thread_t *p_intf)
{
     intf_AssignKey( p_intf , 'Q', 'Q');
     intf_AssignKey( p_intf ,  27, 'Q');
     intf_AssignKey( p_intf ,   3, 'Q');
     intf_AssignKey( p_intf , '0', '0');
     intf_AssignKey( p_intf , '1', '1');
     intf_AssignKey( p_intf , '2', '2');
     intf_AssignKey( p_intf , '3', '3');
     intf_AssignKey( p_intf , '4', '4');
     intf_AssignKey( p_intf , '5', '5');
     intf_AssignKey( p_intf , '6', '6');
     intf_AssignKey( p_intf , '7', '7');
     intf_AssignKey( p_intf , '8', '8');
     intf_AssignKey( p_intf , '9', '9');
     intf_AssignKey( p_intf , '0', '0');
     intf_AssignKey( p_intf , '+', '+');
     intf_AssignKey( p_intf , '-', '-');
     intf_AssignKey( p_intf , 'm', 'M');
     intf_AssignKey( p_intf , 'M', 'M');
     intf_AssignKey( p_intf , 'g', 'g');
     intf_AssignKey( p_intf , 'G', 'G');
     intf_AssignKey( p_intf , 'c', 'c');
     intf_AssignKey( p_intf , ' ', ' ');
     intf_AssignKey( p_intf , 'i', 'i');
     intf_AssignKey( p_intf , 's', 's');
}   

/*****************************************************************************
 * intf_ProcessKey: process standard keys
 *****************************************************************************
 * This function will process standard keys and return non 0 if the key was
 * unknown.
 *****************************************************************************/
int intf_ProcessKey( intf_thread_t *p_intf, int g_key )
{
    static int i_volbackup;
    int i_key;
    
    i_key = intf_getKey( p_intf, g_key); 
    switch( i_key )
    {
    case 'Q':                                                  /* quit order */
        p_intf->b_die = 1;
        break;
    case '0':                                               /* source change */
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        /* Change channel - return code is ignored since SelectChannel displays
         * its own error messages */
        intf_SelectChannel( p_intf, i_key - '0' );
        break;
    case '+':                                                    /* volume + */
        if( (p_main->p_aout != NULL) && (p_main->p_aout->vol < VOLMAX) )
            p_main->p_aout->vol += VOLSTEP;
        break;
    case '-':                                                    /* volume - */
        if( (p_main->p_aout != NULL) && (p_main->p_aout->vol > VOLSTEP) )
            p_main->p_aout->vol -= VOLSTEP;
        break;
    case 'M':                                                 /* toggle mute */
        if( (p_main->p_aout != NULL) && (p_main->p_aout->vol))
        {
            i_volbackup = p_main->p_aout->vol;
            p_main->p_aout->vol = 0;
        }
        else if( (p_main->p_aout != NULL) && (!p_main->p_aout->vol))
            p_main->p_aout->vol = i_volbackup;
        break;
    case 'g':                                                     /* gamma - */
        if( (p_intf->p_vout != NULL) && (p_intf->p_vout->f_gamma > -INTF_GAMMA_LIMIT) )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->f_gamma   -= INTF_GAMMA_STEP;
            p_intf->p_vout->i_changes |= VOUT_GAMMA_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }
        break;
    case 'G':                                                     /* gamma + */
        if( (p_intf->p_vout != NULL) && (p_intf->p_vout->f_gamma < INTF_GAMMA_LIMIT) )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->f_gamma   += INTF_GAMMA_STEP;
            p_intf->p_vout->i_changes |= VOUT_GAMMA_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }
        break;
    case 'c':                                            /* toggle grayscale */
        if( p_intf->p_vout != NULL )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->b_grayscale = !p_intf->p_vout->b_grayscale;
            p_intf->p_vout->i_changes  |= VOUT_GRAYSCALE_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }
        break;
    case ' ':                                            /* toggle interface */
        if( p_intf->p_vout != NULL )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->b_interface     = !p_intf->p_vout->b_interface;
            p_intf->p_vout->i_changes |= VOUT_INTF_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }
        break;
    case 'i':                                                 /* toggle info */
        if( p_intf->p_vout != NULL )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->b_info     = !p_intf->p_vout->b_info;
            p_intf->p_vout->i_changes |= VOUT_INFO_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }
        break;
    case 's':                                              /* toggle scaling */
        if( p_intf->p_vout != NULL )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->b_scale    = !p_intf->p_vout->b_scale;
            p_intf->p_vout->i_changes |= VOUT_SCALE_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }
        break;
   default:                                                   /* unknown key */
        return( 1 );
    }

    return( 0 );
}

/* following functions are local */

/*****************************************************************************
 * LoadChannels: load channels description from a file
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
 *      integer         input vlan
 * The last field must end with a semicolon.
 * Comments and empty lines are not explicitely allowed, but lines with parsing
 * errors are ignored without warning.
 *****************************************************************************/
static int LoadChannels( intf_thread_t *p_intf, char *psz_filename )
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
        intf_ErrMsg("error: can't open %s (%s)\n", psz_filename, strerror(errno));
        return( 1 );
    }

    /* First pass: count number of lines */
    for( i_index = 0; fgets( psz_line, INTF_MAX_CMD_SIZE, p_file ) != NULL; i_index++ )
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
            intf_ErrMsg("error: %s\n", strerror(ENOMEM));
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
                intf_DbgMsg( "channel [%d] %s : method %d (%s:%d vlan %d)\n",
                         p_channel->i_channel, p_channel->psz_description,
                         p_channel->i_input_method,
                         p_channel->psz_input_source,
                         p_channel->i_input_port, p_channel->i_input_vlan );
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
 * UnloadChannels: unload channels description
 *****************************************************************************
 * This function free all resources allocated by LoadChannels, if any.
 *****************************************************************************/
static void UnloadChannels( intf_thread_t *p_intf )
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
 * ParseChannel: parse a channel description line
 *****************************************************************************
 * See LoadChannels. This function return non 0 on parsing error.
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
    p_channel->i_input_vlan =           0;

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
                        intf_ErrMsg("error: %s\n", strerror( ENOMEM ));
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
                        intf_ErrMsg("error: %s\n", strerror( ENOMEM ));
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
            case 5:                                            /* input vlan */
                p_channel->i_channel = strtol( psz_str, &psz_end, 0);
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
