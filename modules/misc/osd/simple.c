/*****************************************************************************
 * simple.c - The OSD Menu simple parser code.
 *****************************************************************************
 * Copyright (C) 2005-2008 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_config.h>

#include <vlc_keys.h>
#include <vlc_image.h>
#include <vlc_osd.h>
#include <vlc_charset.h>

#include <limits.h>

#include "osd_menu.h"

int osd_parser_simpleOpen( vlc_object_t *p_this );

/*****************************************************************************
 * Simple parser open function
 *****************************************************************************/
int osd_parser_simpleOpen( vlc_object_t *p_this )
{
    osd_menu_t     *p_menu = (osd_menu_t *) p_this;
    osd_button_t   *p_current = NULL; /* button currently processed */
    osd_button_t   *p_prev = NULL;    /* previous processed button */

    FILE       *fd = NULL;
    int        result = 0;

    if( !p_menu ) return VLC_ENOOBJ;

    msg_Dbg( p_this, "opening osdmenu definition file %s", p_menu->psz_file );
    fd = utf8_fopen( p_menu->psz_file, "r" );
    if( !fd )
    {
        msg_Err( p_this, "failed to open osdmenu definition file %s",
                p_menu->psz_file );
        return VLC_EGENERIC;
    }

    /* Read first line */
    if( !feof( fd ) )
    {
        char action[25] = "";
        char cmd[25] = "";
        char path[PATH_MAX] = "";
        char *psz_path = NULL;
        size_t i_len = 0;
        long pos = 0;

        result = fscanf(fd, "%24s %255s", &action[0], &path[0] );

        /* override images path ? */
        psz_path = config_GetPsz( p_this, "osdmenu-file-path" );
        if( psz_path )
        {
            /* psz_path is not null and therefor &path[0] cannot be NULL
             * it might be null terminated.
             */
            strncpy( &path[0], psz_path, PATH_MAX );
            free( psz_path );
            psz_path = NULL;
        }
        /* NULL terminate before asking the length of path[] */
        path[PATH_MAX-1] = '\0';
        i_len = strlen(&path[0]);
        if( i_len == PATH_MAX )
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
        msg_Dbg( p_this, "osdmenu dir %s", &path[0] );

        if( i_len == 0 )
            p_menu = osd_MenuNew( p_menu, NULL, 0, 0 );
        else
            p_menu = osd_MenuNew( p_menu, &path[0], 0, 0 );

        /* Peek for 'style' argument */
        pos = ftell( fd );
        if( pos < 0 )
                goto error;

        result = fscanf(fd, "%24s %24s", &cmd[0], &action[0] );
        if( result == 0 || result == EOF )
            goto error;

        msg_Dbg( p_this, "osdmenu %s %s", &cmd[0], &action[0] );
        if( strncmp( &cmd[0], "style", 5 ) == 0 )
        {
            if( strncmp( &action[0], "default", 7) == 0 )
            {
                p_menu->i_style = OSD_MENU_STYLE_SIMPLE;
            }
            else if( strncmp( &action[0], "concat", 6) == 0 )
            {
                p_menu->i_style = OSD_MENU_STYLE_CONCAT;
            }
        }
        else
        {
            result = fseek( fd, pos, SEEK_SET );
            if( result < 0 )
                goto error;
        }
    }

    if( !p_menu )
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
        char path[PATH_MAX]  = "";
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
            p_menu->p_button = p_current;
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
                        p_menu->p_state->p_volume = p_up;
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

                    p_up->b_range = true;

                    result = fscanf( fd, "\t%24s", &action[0] );
                    if( result == 0 )
                        goto error;

                    result = fscanf( fd, "\t%d", &i_index );
                    if( result == 0 )
                        goto error;

                    msg_Dbg( p_this, " + (menu up) hotkey down %s, file=%s%s",
                             &action[0], p_menu->psz_path, &file[0] );

                    free( p_up->psz_action_down );
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

                        if( p_menu->psz_path )
                        {
                            size_t i_path_size = strlen( p_menu->psz_path );
                            size_t i_file_size = strlen( &file[0] );

                            if( (i_path_size + i_file_size >= PATH_MAX) ||
                                (i_path_size >= PATH_MAX) )
                                goto error;

                            strncpy( &path[0], p_menu->psz_path, i_path_size );
                            strncpy( &path[i_path_size], &file[0],
                                     PATH_MAX - (i_path_size + i_file_size) );
                            path[ i_path_size + i_file_size ] = '\0';

                            p_range_current = osd_StateNew( p_menu, &path[0], "pressed" );
                        }
                        else /* absolute paths are used. */
                            p_range_current = osd_StateNew( p_menu, &file[0], "pressed" );

                        if( !p_range_current )
                            goto error;

                        if( !p_range_current->p_pic )
                        {
                            osd_StatesFree( p_menu, p_range_current );
                            goto error;
                        }

                        p_range_current->i_x = i_x;
                        p_range_current->i_y = i_y;

                        /* increment the number of ranges for this button */
                        p_up->i_ranges++;

                        if( p_range_prev )
                            p_range_prev->p_next = p_range_current;
                        else
                            p_up->p_states = p_range_current;
                        p_range_current->p_prev = p_range_prev;

                        msg_Dbg( p_this, "  |- range=%d, file=%s%s",
                                 p_up->i_ranges,
                                 p_menu->psz_path, &file[0] );
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

                p_current->b_range = true;

                result = fscanf( fd, "\t%24s", &action[0] );
                if( result == 0 )
                    goto error;

                result = fscanf( fd, "\t%d", &i_index );
                if( result == 0 )
                    goto error;

                msg_Dbg( p_this, " + hotkey down %s, file=%s%s", 
                         &action[0], p_menu->psz_path, &file[0] );
                free( p_current->psz_action_down );
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

                    if( p_menu->psz_path )
                    {
                        size_t i_path_size = strlen( p_menu->psz_path );
                        size_t i_file_size = strlen( &file[0] );

                        if( (i_path_size + i_file_size >= PATH_MAX) ||
                            (i_path_size >= PATH_MAX) )
                            goto error;

                        strncpy( &path[0], p_menu->psz_path, i_path_size );
                        strncpy( &path[i_path_size], &file[0],
                                 PATH_MAX - (i_path_size + i_file_size) );
                        path[ i_path_size + i_file_size ] = '\0';

                        p_range_current = osd_StateNew( p_menu, &path[0], "pressed" );
                    }
                    else /* absolute paths are used. */
                        p_range_current = osd_StateNew( p_menu, &file[0], "pressed" );

                    if( !p_range_current )
                        goto error;

                    if( !p_range_current->p_pic )
                    {
                        osd_StatesFree( p_menu, p_range_current );
                        goto error;
                    }

                    p_range_current->i_x = i_x;
                    p_range_current->i_y = i_y;

                    /* increment the number of ranges for this button */
                    p_current->i_ranges++;

                    if( p_range_prev )
                        p_range_prev->p_next = p_range_current;
                    else
                        p_current->p_states = p_range_current;
                    p_range_current->p_prev = p_range_prev;

                    msg_Dbg( p_this, "  |- range=%d, file=%s%s",
                             p_current->i_ranges,
                             p_menu->psz_path, &file[0] );
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
                msg_Err( p_this, "invalid button state %s for button %s "
                         "expected %u: unselect, select or pressed)",
                         &state[0], &action[0], (unsigned)strlen(&state[0]));
                goto error;
            }

            if( p_menu->psz_path )
            {
                size_t i_path_size = strlen( p_menu->psz_path );
                size_t i_file_size = strlen( &file[0] );

                if( (i_path_size + i_file_size >= PATH_MAX) ||
                    (i_path_size >= PATH_MAX) )
                    goto error;

                strncpy( &path[0], p_menu->psz_path, i_path_size );
                strncpy( &path[i_path_size], &file[0],
                         PATH_MAX - (i_path_size + i_file_size) );
                path[ i_path_size + i_file_size ] = '\0';

                p_state_current = osd_StateNew( p_menu, &path[0], &state[0] );
            }
            else /* absolute paths are used. */
                p_state_current = osd_StateNew( p_menu, &file[0], &state[0] );

            if( !p_state_current )
                goto error;

            if( !p_state_current->p_pic )
            {
                osd_StatesFree( p_menu, p_state_current );
                goto error;
            }

            p_state_current->i_x = i_x;
            p_state_current->i_y = i_y;

            if( p_state_prev )
                p_state_prev->p_next = p_state_current;
            else
                p_current->p_states = p_state_current;
            p_state_current->p_prev = p_state_prev;

            msg_Dbg( p_this, " |- state=%s, file=%s%s", &state[0],
                     p_menu->psz_path, &file[0] );
        }
        p_current->p_current_state = p_current->p_states;
    }

    /* Find the last button and store its pointer.
     * The OSD menu behaves like a roundrobin list.
     */
    p_current = p_menu->p_button;
    while( p_current && p_current->p_next )
    {
        osd_button_t *p_temp = NULL;
        p_temp = p_current->p_next;
        p_current = p_temp;
    }
    p_menu->p_last_button = p_current;
    fclose( fd );
    return VLC_SUCCESS;

error:
    msg_Err( p_menu, "parsing file failed (returned %d)", result );
    osd_MenuFree( p_menu );
    fclose( fd );
    return VLC_EGENERIC;
}
