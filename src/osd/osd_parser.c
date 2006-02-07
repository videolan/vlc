/*****************************************************************************
 * osd_parser.c - The OSD Menu  parser core code.
 *****************************************************************************
 * Copyright (C) 2005 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc_config.h>
#include <vlc_video.h>

#include <vlc_keys.h>
#include <vlc_image.h>
#include <vlc_osd.h>


#undef OSD_MENU_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *ppsz_button_states[] = { "unselect", "select", "pressed" };

/* OSD Menu structure support routines */
static osd_menu_t   *osd_MenuNew( osd_menu_t *, const char *, int, int );
static osd_button_t *osd_ButtonNew( const char *, int, int );
static osd_state_t  *osd_StateNew( vlc_object_t *, const char *, const char * );

static void osd_MenuFree  ( vlc_object_t *, osd_menu_t * );
static void osd_ButtonFree( vlc_object_t *, osd_button_t * );
static void osd_StatesFree( vlc_object_t *, osd_state_t * );

static picture_t *osd_LoadImage( vlc_object_t *, const char *);

/*****************************************************************************
 * osd_LoadImage: loads the logo image into memory
 *****************************************************************************/
static picture_t *osd_LoadImage( vlc_object_t *p_this, const char *psz_filename )
{
    picture_t *p_pic = NULL;
    image_handler_t *p_image;
    video_format_t fmt_in = {0}, fmt_out = {0};

    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    p_image = image_HandlerCreate( p_this );
    if( p_image )
    {
        p_pic = image_ReadUrl( p_image, psz_filename, &fmt_in, &fmt_out );
        image_HandlerDelete( p_image );
#if 0        
        p_pic = osd_YuvaYuvp( p_this, p_pic );
#endif
    }
    else msg_Err( p_this, "unable to handle this chroma" );

    return p_pic;
}

/*****************************************************************************
 * Create a new Menu structure
 *****************************************************************************/
static osd_menu_t *osd_MenuNew( osd_menu_t *p_menu, const char *psz_path, int i_x, int i_y )
{
    if( !p_menu ) return NULL;
    
    p_menu->p_state = (osd_menu_state_t *) malloc( sizeof( osd_menu_state_t ) );
    if( !p_menu->p_state )
        msg_Err( p_menu, "memory allocation for OSD Menu state failed." );

    if( psz_path != NULL )
        p_menu->psz_path = strdup( psz_path );
    else
        p_menu->psz_path = NULL;
    p_menu->i_x = i_x;
    p_menu->i_y = i_y;
    
    return p_menu; 
}

/*****************************************************************************
 * Free the menu
 *****************************************************************************/
static void osd_MenuFree( vlc_object_t *p_this, osd_menu_t *p_menu )
{
    msg_Dbg( p_this, "freeing menu" );
    osd_ButtonFree( p_this, p_menu->p_button );
    p_menu->p_button = NULL;
    p_menu->p_last_button = NULL;
    if( p_menu->psz_path ) free( p_menu->psz_path );
    p_menu->psz_path = NULL;
    if( p_menu->p_state ) free( p_menu->p_state );
    p_menu->p_state = NULL;
}

/*****************************************************************************
 * Create a new button
 *****************************************************************************/
static osd_button_t *osd_ButtonNew( const char *psz_action, int i_x, int i_y )
{
    osd_button_t *p_button = NULL;
    p_button = (osd_button_t*) malloc( sizeof(osd_button_t) );
    if( !p_button )
        return NULL;
    
    memset( p_button, 0, sizeof(osd_button_t) );
    p_button->psz_action = strdup(psz_action);
    p_button->psz_action_down = NULL;
    p_button->p_feedback = NULL;
    p_button->i_x = i_x;
    p_button->i_y = i_y;
    
    return p_button;
}

/*****************************************************************************
 * Free a button
 *****************************************************************************/ 
static void osd_ButtonFree( vlc_object_t *p_this, osd_button_t *p_button )
{
    osd_button_t *p_current = p_button;
    osd_button_t *p_next = NULL;
    osd_button_t *p_prev = NULL;
    
    /* First walk to the end. */
    while( p_current->p_next )
    {
        p_next = p_current->p_next;
        p_current = p_next;        
    }
    /* Then free end first and walk to the start. */
    while( p_current->p_prev )
    {
        msg_Dbg( p_this, "+ freeing button %s [%p]", p_current->psz_action, p_current );
        p_prev = p_current->p_prev;
        p_current = p_prev;
        if( p_current->p_next )
        {
            if( p_current->p_next->psz_name ) 
                free( p_current->p_next->psz_name );
            if( p_current->p_next->psz_action )
                free( p_current->p_next->psz_action );
            if( p_current->p_next->psz_action_down )
                free( p_current->p_next->psz_action_down );
            if( p_current->p_feedback && p_current->p_feedback->p_data_orig )
                free( p_current->p_feedback->p_data_orig );
            if( p_current->p_feedback )
                free( p_current->p_feedback );
            
            p_current->p_next->psz_action_down = NULL;
            p_current->p_next->psz_action = NULL;
            p_current->p_next->psz_name = NULL;
            p_current->p_feedback = NULL;
                            
            /* Free all states first */            
            if( p_current->p_next->p_states )   
                osd_StatesFree( p_this, p_current->p_next->p_states );
            p_current->p_next->p_states = NULL;          
            if( p_current->p_next) free( p_current->p_next );
            p_current->p_next = NULL;  
        }            
        
        if( p_current->p_up )
        {
            if( p_current->p_up->psz_name ) 
                free( p_current->p_up->psz_name );
            if( p_current->p_up->psz_action )
                free( p_current->p_up->psz_action );
            if( p_current->p_up->psz_action_down )
                free( p_current->p_up->psz_action_down );
            if( p_current->p_feedback && p_current->p_feedback->p_data_orig )
                free( p_current->p_feedback->p_data_orig );
            if( p_current->p_feedback )
                free( p_current->p_feedback );
            
            p_current->p_up->psz_action_down = NULL;
            p_current->p_up->psz_action = NULL;
            p_current->p_up->psz_name = NULL;
            p_current->p_feedback = NULL;
            
            /* Free all states first */            
            if( p_current->p_up->p_states )   
                osd_StatesFree( p_this, p_current->p_up->p_states );
            p_current->p_up->p_states = NULL;          
            if( p_current->p_up ) free( p_current->p_up );
            p_current->p_up = NULL;          
        }
    }    
    /* Free the last one. */
    if( p_button ) 
    {
        msg_Dbg( p_this, "+ freeing button %s [%p]", p_button->psz_action, p_button );    
        if( p_button->psz_name ) free( p_button->psz_name );
        if( p_button->psz_action ) free( p_button->psz_action );
        if( p_button->psz_action_down ) free( p_button->psz_action_down );   
        if( p_current->p_feedback && p_current->p_feedback->p_data_orig )
            free( p_current->p_feedback->p_data_orig );
        if( p_current->p_feedback )
            free( p_current->p_feedback );
        
        p_button->psz_name = NULL;
        p_button->psz_action = NULL;
        p_button->psz_action_down = NULL;
        p_current->p_feedback = NULL;
                
        if( p_button->p_states )
            osd_StatesFree( p_this, p_button->p_states );
        p_button->p_states = NULL;          
        free( p_button );
        p_button = NULL;
    }
}

/*****************************************************************************
 * Create a new state image
 *****************************************************************************/
static osd_state_t *osd_StateNew( vlc_object_t *p_this, const char *psz_file, const char *psz_state )
{
    osd_state_t *p_state = NULL;
    p_state = (osd_state_t*) malloc( sizeof(osd_state_t) );
    if( !p_state )
        return NULL;
        
    memset( p_state, 0, sizeof(osd_state_t) );    
    p_state->p_pic = osd_LoadImage( p_this, psz_file );

    if( psz_state )
    {
        p_state->psz_state = strdup( psz_state );
        if( strncmp( ppsz_button_states[0], psz_state, strlen(ppsz_button_states[0]) ) == 0 )
            p_state->i_state = OSD_BUTTON_UNSELECT;
        else if( strncmp( ppsz_button_states[1], psz_state, strlen(ppsz_button_states[1]) ) == 0 )
            p_state->i_state = OSD_BUTTON_SELECT;
        else if( strncmp( ppsz_button_states[2], psz_state, strlen(ppsz_button_states[2]) ) == 0 )
            p_state->i_state = OSD_BUTTON_PRESSED;
    }
    return p_state;
}

/*****************************************************************************
 * Free state images
 *****************************************************************************/
static void osd_StatesFree( vlc_object_t *p_this, osd_state_t *p_states )
{
    osd_state_t *p_state = p_states;
    osd_state_t *p_next = NULL;
    osd_state_t *p_prev = NULL;
    
    while( p_state->p_next )
    {
        p_next = p_state->p_next;
        p_state = p_next;
    }
    /* Then free end first and walk to the start. */
    while( p_state->p_prev )
    {
        msg_Dbg( p_this, " |- freeing state %s [%p]", p_state->psz_state, p_state );
        p_prev = p_state->p_prev;
        p_state = p_prev;
        if( p_state->p_next )
        {
            if( p_state->p_next->p_pic && p_state->p_next->p_pic->p_data_orig )
                free( p_state->p_next->p_pic->p_data_orig );
            if( p_state->p_next->p_pic ) free( p_state->p_next->p_pic );
            p_state->p_next->p_pic = NULL;      
            if( p_state->p_next->psz_state ) free( p_state->p_next->psz_state );
            p_state->p_next->psz_state = NULL;
            free( p_state->p_next );
            p_state->p_next = NULL;        
        }
    }
    /* Free the last one. */
    if( p_states )
    {
        msg_Dbg( p_this, " |- freeing state %s [%p]", p_state->psz_state, p_states );
        if( p_states->p_pic && p_states->p_pic->p_data_orig )
            free( p_states->p_pic->p_data_orig );
        if( p_states->p_pic ) free( p_states->p_pic );
        p_states->p_pic = NULL;
        if( p_state->psz_state ) free( p_state->psz_state );
        p_state->psz_state = NULL;
        free( p_states );
        p_states = NULL;
    }
}

/*****************************************************************************
 * osd_ConfigLoader: Load and parse osd text configurationfile
 *****************************************************************************/
int osd_ConfigLoader( vlc_object_t *p_this, const char *psz_file,
    osd_menu_t **p_menu )
{
    osd_button_t   *p_current = NULL; /* button currently processed */
    osd_button_t   *p_prev = NULL;    /* previous processed button */ 

#define MAX_FILE_PATH 256    
    FILE       *fd = NULL;
    int        result = 0;
    
    msg_Dbg( p_this, "opening osd definition file %s", psz_file );
    fd = fopen( psz_file, "r" );
    if( !fd )  
    {
        msg_Err( p_this, "failed opening osd definition file %s", psz_file );
        return VLC_EGENERIC;
    }
    
    /* Read first line */    
    if( !feof( fd ) )
    {
        char action[25] = "";
        char path[MAX_FILE_PATH] = "";
        char *psz_path = NULL;
        size_t i_len = 0;

        /* override images path ? */
        psz_path = config_GetPsz( p_this, "osdmenu-file-path" );
        if( psz_path == NULL )
        {                                    
            result = fscanf(fd, "%24s %255s", &action[0], &path[0] );
        }
        else
        {
            /* psz_path is not null and therefor &path[0] cannot be NULL 
             * it might be null terminated.
             */
            strncpy( &path[0], psz_path, MAX_FILE_PATH );
            free( psz_path );
            psz_path = NULL;
        }
        /* NULL terminate before asking the length of path[] */
        path[MAX_FILE_PATH-1] = '\0';
        i_len = strlen(&path[0]);
        if( i_len == MAX_FILE_PATH )
            i_len--; /* truncate to prevent buffer overflow */
#if defined(WIN32) || defined(UNDER_CE)
        if( (i_len > 0) && path[i_len] != '\\' )
            path[i_len] = '\\';
#else        
        if( (i_len > 0) && path[i_len] != '/' )
            path[i_len] = '/';
#endif
        path[i_len+1] = '\0';
        if( result == 0 || result == EOF )
            goto error;                        
        msg_Dbg( p_this, "%s=%s", &action[0], &path[0] );
         
        if( i_len == 0 )
            *p_menu = osd_MenuNew( *p_menu, NULL, 0, 0 );
        else
            *p_menu = osd_MenuNew( *p_menu, &path[0], 0, 0 );
    }
    
    if( !*p_menu )
        goto error;
        
    /* read successive lines */
    while( !feof( fd ) )
    {   
        osd_state_t   *p_state_current = NULL; /* button state currently processed */
        osd_state_t   *p_state_prev = NULL;    /* previous state processed button */ 
        
        char cmd[25] = "";      
        char action[25] = "";
        char state[25]  = "";
        char file[256]  = "";
        char path[512]  = "";        
        int  i_x = 0;
        int  i_y = 0;

        result = fscanf( fd, "%24s %24s (%d,%d)", &cmd[0], &action[0], &i_x, &i_y );
        if( result == 0 )
            goto error;              
        if( strncmp( &cmd[0], "action", 6 ) != 0 )
            break;
        msg_Dbg( p_this, " + %s hotkey=%s (%d,%d)", &cmd[0], &action[0], i_x, i_y );                    
                
        p_prev = p_current;
        p_current = osd_ButtonNew( &action[0], i_x, i_y );   
        if( !p_current )
            goto error;
        
        if( p_prev )
            p_prev->p_next = p_current;            
        else
            (*p_menu)->p_button = p_current;            
        p_current->p_prev = p_prev;
        
        /* parse all states */
        while( !feof( fd ) )
        {
            char type[25] = "";
            
            result = fscanf( fd, "\t%24s", &state[0] );   
            if( result == 0 )
                goto error;
            
            /* FIXME: We only parse one level deep now */    
            if( strncmp( &state[0], "action", 6 ) == 0 )
            {
                osd_button_t   *p_up = NULL;
                
                result = fscanf( fd, "%24s (%d,%d)", &action[0], &i_x, &i_y );
                if( result == 0 )
                    goto error;
                /* create new button */                
                p_up = osd_ButtonNew( &action[0], i_x, i_y );                 
                if( !p_up ) 
                    goto error;
                /* Link to list */                    
                p_up->p_down = p_current;
                p_current->p_up = p_up;                 
                msg_Dbg( p_this, " + (menu up) hotkey=%s (%d,%d)", &action[0], i_x, i_y );
                /* Parse type state */
                result = fscanf( fd, "\t%24s %24s", &cmd[0], &type[0] );   
                if( result == 0 )
                    goto error;
                if( strncmp( &cmd[0], "type", 4 ) == 0 )
                {  
                    if( strncmp( &type[0], "volume", 6 ) == 0 )
                    {
                        (*p_menu)->p_state->p_volume = p_up;
                        msg_Dbg( p_this, " + type=%s", &type[0] );
                    }
                }
                /* Parse range state */
                result = fscanf( fd, "\t%24s", &state[0] );   
                if( result == 0 )
                    goto error;
                /* Parse the range state */        
                if( strncmp( &state[0], "range", 5 ) == 0 )
                {
                    osd_state_t   *p_range_current = NULL; /* range state currently processed */
                    osd_state_t   *p_range_prev = NULL;    /* previous state processed range */ 
                    int i_index = 0;
                                                            
                    p_up->b_range = VLC_TRUE;
    
                    result = fscanf( fd, "\t%24s", &action[0] );   
                    if( result == 0 )
                        goto error;
                    
                    result = fscanf( fd, "\t%d", &i_index );   
                    if( result == 0 )
                        goto error;
                                                                                            
                    msg_Dbg( p_this, " + (menu up) hotkey down %s, file=%s%s", &action[0], (*p_menu)->psz_path, &file[0] );
                    
                    if( p_up->psz_action_down ) free( p_up->psz_action_down );
                    p_up->psz_action_down = strdup( &action[0] );                     
                    
                    /* Parse range contstruction :
                     * range <hotkey> 
                     *      <state1> <file1>
                     *
                     *      <stateN> <fileN>
                     * end 
                     */                
                    while( !feof( fd ) )
                    {
                        result = fscanf( fd, "\t%255s", &file[0] );   
                        if( result == 0 )
                            goto error;
                        if( strncmp( &file[0], "end", 3 ) == 0 )
                            break;
                        
                        p_range_prev = p_range_current;
            
                        if( (*p_menu)->psz_path )
                        {
                            size_t i_path_size = strlen( (*p_menu)->psz_path );
                            size_t i_file_size = strlen( &file[0] );
                            
                            strncpy( &path[0], (*p_menu)->psz_path, i_path_size );
                            strncpy( &path[i_path_size], &file[0], 512 - (i_path_size + i_file_size) );
                            path[ i_path_size + i_file_size ] = '\0';
                            
                            p_range_current = osd_StateNew( p_this, &path[0], "pressed" );
                        }
                        else /* absolute paths are used. */
                            p_range_current = osd_StateNew( p_this, &file[0], "pressed" );
                            
                        if( !p_range_current || !p_range_current->p_pic )
                            goto error;
                            
                        /* increment the number of ranges for this button */                
                        p_up->i_ranges++;
                        
                        if( p_range_prev )
                            p_range_prev->p_next = p_range_current;
                        else
                            p_up->p_states = p_range_current;                                
                        p_range_current->p_prev = p_range_prev;
                        
                        msg_Dbg( p_this, "  |- range=%d, file=%s%s", 
                                p_up->i_ranges, 
                                (*p_menu)->psz_path, &file[0] );                        
                    }
                    if( i_index > 0 )
                    { 
                        osd_state_t *p_range = NULL;
                        
                        /* Find the default index for state range */                        
                        p_range = p_up->p_states;
                        while( (--i_index > 0) && p_range->p_next )
                        {
                            osd_state_t *p_temp = NULL;
                            p_temp = p_range->p_next;
                            p_range = p_temp;
                        }
                        p_up->p_current_state = p_range;
                    }                    
                    else p_up->p_current_state = p_up->p_states;
                    
                }   
                result = fscanf( fd, "\t%24s", &state[0] );   
                if( result == 0 )
                    goto error;                    
                if( strncmp( &state[0], "end", 3 ) != 0 )
                    goto error;
                    
                /* Continue at the beginning of the while() */
                continue;
            }
            
            /* Parse the range state */        
            if( strncmp( &state[0], "range", 5 ) == 0 )
            {
                osd_state_t   *p_range_current = NULL; /* range state currently processed */
                osd_state_t   *p_range_prev = NULL;    /* previous state processed range */ 
                int i_index = 0;
                                                        
                p_current->b_range = VLC_TRUE;

                result = fscanf( fd, "\t%24s", &action[0] );   
                if( result == 0 )
                    goto error;
                
                result = fscanf( fd, "\t%d", &i_index );   
                if( result == 0 )
                    goto error;
                                                                                        
                msg_Dbg( p_this, " + hotkey down %s, file=%s%s", &action[0], (*p_menu)->psz_path, &file[0] );                        
                if( p_current->psz_action_down ) free( p_current->psz_action_down );
                p_current->psz_action_down = strdup( &action[0] );                     
                
                /* Parse range contstruction :
                 * range <hotkey> 
                 *      <state1> <file1>
                 *
                 *      <stateN> <fileN>
                 * end 
                 */                
                while( !feof( fd ) )
                {
                    result = fscanf( fd, "\t%255s", &file[0] );   
                    if( result == 0 )
                        goto error;
                    if( strncmp( &file[0], "end", 3 ) == 0 )
                        break;
                    
                    p_range_prev = p_range_current;
        
                    if( (*p_menu)->psz_path )
                    {
                        size_t i_path_size = strlen( (*p_menu)->psz_path );
                        size_t i_file_size = strlen( &file[0] );
                        
                        strncpy( &path[0], (*p_menu)->psz_path, i_path_size );
                        strncpy( &path[i_path_size], &file[0], 512 - (i_path_size + i_file_size) );
                        path[ i_path_size + i_file_size ] = '\0';
                        
                        p_range_current = osd_StateNew( p_this, &path[0], "pressed" );
                    }
                    else /* absolute paths are used. */
                        p_range_current = osd_StateNew( p_this, &file[0], "pressed" );
                        
                    if( !p_range_current || !p_range_current->p_pic )
                        goto error;
                        
                    /* increment the number of ranges for this button */                
                    p_current->i_ranges++;
                    
                    if( p_range_prev )
                        p_range_prev->p_next = p_range_current;
                    else
                        p_current->p_states = p_range_current;                                
                    p_range_current->p_prev = p_range_prev;
                    
                    msg_Dbg( p_this, "  |- range=%d, file=%s%s", 
                            p_current->i_ranges, 
                            (*p_menu)->psz_path, &file[0] );                        
                }
                if( i_index > 0 )
                {             
                    osd_state_t *p_range = NULL;
                       
                    /* Find the default index for state range */                        
                    p_range = p_current->p_states;
                    while( (--i_index > 0) && p_range->p_next )
                    {
                        osd_state_t *p_temp = NULL;
                        p_temp = p_range->p_next;
                        p_range = p_temp;
                    }
                    p_current->p_current_state = p_range;
                }                    
                else p_current->p_current_state = p_current->p_states;
                /* Continue at the beginning of the while() */
                continue;
            }   
            if( strncmp( &state[0], "end", 3 ) == 0 )
                break;
                
            result = fscanf( fd, "\t%255s", &file[0] );   
            if( result == 0 )
                goto error;                                
            
            p_state_prev = p_state_current;

            if( ( strncmp( ppsz_button_states[0], &state[0], strlen(ppsz_button_states[0]) ) != 0 ) &&
                ( strncmp( ppsz_button_states[1], &state[0], strlen(ppsz_button_states[1]) ) != 0 ) &&
                ( strncmp( ppsz_button_states[2], &state[0], strlen(ppsz_button_states[2]) ) != 0 ) )
            {
                msg_Err( p_this, "invalid button state %s for button %s expected %d: unselect, select or pressed)",
                                    &state[0], &action[0], strlen(&state[0]));
                goto error;
            }
            
            if( (*p_menu)->psz_path )
            {
                size_t i_path_size = strlen( (*p_menu)->psz_path );
                size_t i_file_size = strlen( &file[0] );
                
                strncpy( &path[0], (*p_menu)->psz_path, i_path_size );
                strncpy( &path[i_path_size], &file[0], 512 - (i_path_size + i_file_size) );
                path[ i_path_size + i_file_size ] = '\0';
                
                p_state_current = osd_StateNew( p_this, &path[0], &state[0] );
            }
            else /* absolute paths are used. */
                p_state_current = osd_StateNew( p_this, &file[0], &state[0] );
                
            if( !p_state_current || !p_state_current->p_pic )
                goto error;
                
            if( p_state_prev )
                p_state_prev->p_next = p_state_current;
            else
                p_current->p_states = p_state_current;                                
            p_state_current->p_prev = p_state_prev;
            
            msg_Dbg( p_this, " |- state=%s, file=%s%s", &state[0], (*p_menu)->psz_path, &file[0] );
        }
        p_current->p_current_state = p_current->p_states;
    }

    /* Find the last button and store its pointer. 
     * The OSD menu behaves like a roundrobin list.
     */        
    p_current = (*p_menu)->p_button;
    while( p_current && p_current->p_next )
    {
        osd_button_t *p_temp = NULL;
        p_temp = p_current->p_next;
        p_current = p_temp;
    }
    (*p_menu)->p_last_button = p_current;
    fclose( fd );
    return 0;
    
#undef MAX_FILE_PATH 
error:
    msg_Err( p_this, "parsing file failed (returned %d)", result );
    fclose( fd );
    return 1;        
}

/*****************************************************************************
 * osd_ConfigUnload: Load and parse osd text configurationfile
 *****************************************************************************/
void osd_ConfigUnload( vlc_object_t *p_this, osd_menu_t **p_osd)
{
    msg_Dbg( p_this, "unloading OSD menu structure" );
    osd_MenuFree( p_this, *p_osd );
}
