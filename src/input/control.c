/*****************************************************************************
 * control.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

#include "ninput.h"
#include "../../modules/demux/util/sub.h"

struct input_thread_sys_t
{
    /* subtitles */
    int              i_sub;
    subtitle_demux_t **sub;
    int64_t          i_stop_time;
};

static void UpdateBookmarksOption( input_thread_t * );

/****************************************************************************
 * input_Control
 ****************************************************************************/
/**
 * Control function for inputs.
 * \param p_input input handle
 * \param i_query query type
 * \return VLC_SUCESS if ok
 */
int input_Control( input_thread_t *p_input, int i_query, ...  )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = input_vaControl( p_input, i_query, args );
    va_end( args );

    return i_result;
}

int input_vaControl( input_thread_t *p_input, int i_query, va_list args )
{
    int     i_ret;
    seekpoint_t *p_bkmk, ***ppp_bkmk;
    int i_bkmk = 0;
    int *pi_bkmk;
    int i, *pi;
    vlc_value_t val, text;
    char *psz_option, *psz_value;
    int i_int, *pi_int;
    double f, *pf;
    int64_t i_64, *pi_64;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    switch( i_query )
    {
        case INPUT_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            *pf = var_GetFloat( p_input, "position" );
            i_ret = VLC_SUCCESS;
            break;
        case INPUT_SET_POSITION:
            f = (double)va_arg( args, double );
            i_ret = var_SetFloat( p_input, "position", f );
            break;

        case INPUT_GET_LENGTH:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetTime( p_input, "length" );
            i_ret = VLC_SUCCESS;
            break;
        case INPUT_GET_TIME:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetTime( p_input, "time" );
            i_ret = VLC_SUCCESS;
            break;
        case INPUT_SET_TIME:
            i_64 = (int64_t)va_arg( args, int64_t );
            i_ret = var_SetTime( p_input, "time", i_64 );
            break;

        case INPUT_GET_RATE:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = var_GetInteger( p_input, "rate" );
            i_ret = VLC_SUCCESS;
            break;
        case INPUT_SET_RATE:
            i_int = (int)va_arg( args, int );
            i_ret = var_SetInteger( p_input, "rate", i_int );
            break;

        case INPUT_GET_STATE:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = var_GetInteger( p_input, "state" );
            i_ret = VLC_SUCCESS;
            break;
        case INPUT_SET_STATE:
            i_int = (int)va_arg( args, int );
            i_ret = var_SetInteger( p_input, "state", i_int );
            break;

        case INPUT_ADD_OPTION:
        {
            psz_option = (char *)va_arg( args, char * );
            psz_value = (char *)va_arg( args, char * );
            i_ret = VLC_EGENERIC;

            vlc_mutex_lock( &p_input->p_item->lock );
            /* Check if option already exists */            
            for( i = 0; i < p_input->p_item->i_options; i++ )
            {
                if( !strncmp( p_input->p_item->ppsz_options[i], psz_option,
                              strlen( psz_option ) ) &&
                    p_input->p_item->ppsz_options[i][strlen(psz_option)]
                      == '=' )
                {
                    free( p_input->p_item->ppsz_options[i] );
                    break;
                }
            }
            if( i == p_input->p_item->i_options )
            {
                p_input->p_item->i_options++;
                p_input->p_item->ppsz_options =
                    realloc( p_input->p_item->ppsz_options,
                             p_input->p_item->i_options * sizeof(char **) );
            }

            asprintf( &p_input->p_item->ppsz_options[i],
                      "%s=%s", psz_option, psz_value ) ;
            vlc_mutex_unlock( &p_input->p_item->lock );

            i_ret = VLC_SUCCESS;
            break;
        }

        case INPUT_SET_NAME:
        {
            char *psz_name = (char *)va_arg( args, char * );
            i_ret = VLC_EGENERIC;
            if( !psz_name ) break;
            vlc_mutex_lock( &p_input->p_item->lock );
            if( p_input->p_item->psz_name ) free( p_input->p_item->psz_name );
            p_input->p_item->psz_name = strdup( psz_name );
            vlc_mutex_unlock( &p_input->p_item->lock );
            i_ret = VLC_SUCCESS;

            /* Notify playlist */
            {
                vlc_value_t val;
                playlist_t *p_playlist =
                (playlist_t *)vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                               FIND_PARENT );
                if( p_playlist )
                {
                    val.i_int = p_playlist->i_index;
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    var_Set( p_playlist, "item-change", val );
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    vlc_object_release( p_playlist );
                }
            }
            break;
        }

        case INPUT_ADD_INFO:
        {
            char *psz_cat = (char *)va_arg( args, char * );
            char *psz_name = (char *)va_arg( args, char * );
            char *psz_format = (char *)va_arg( args, char * );

            info_category_t *p_cat;
            info_t *p_info;
            int i;

            i_ret = VLC_EGENERIC;

            vlc_mutex_lock( &p_input->p_item->lock );
            for( i = 0; i < p_input->p_item->i_categories; i++ )
            {
                if( !strcmp( p_input->p_item->pp_categories[i]->psz_name,
                             psz_cat ) )
                    break;
            }

            if( i == p_input->p_item->i_categories )
            {
                p_cat = malloc( sizeof( info_category_t ) );
                if( !p_cat ) break;
                p_cat->psz_name = strdup( psz_cat );
                p_cat->i_infos = 0;
                p_cat->pp_infos = NULL;
                INSERT_ELEM( p_input->p_item->pp_categories,
                             p_input->p_item->i_categories,
                             p_input->p_item->i_categories, p_cat );
            }

            p_cat = p_input->p_item->pp_categories[i];

            for( i = 0; i < p_cat->i_infos; i++ )
            {
                if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
                {
                    if( p_cat->pp_infos[i]->psz_value )
                        free( p_cat->pp_infos[i]->psz_value );
                    break;
                }
            }

            if( i == p_cat->i_infos )
            {
                p_info = malloc( sizeof( info_t ) );
                if( !p_info ) break;
                INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos,
                             p_cat->i_infos, p_info );
                p_info->psz_name = strdup( psz_name );
            }

            p_info = p_cat->pp_infos[i];
            vasprintf( &p_info->psz_value, psz_format, args );

            vlc_mutex_unlock( &p_input->p_item->lock );

            i_ret = VLC_SUCCESS;

            /* Notify playlist */
            {
                vlc_value_t val;
                playlist_t *p_playlist =
                (playlist_t *)vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                               FIND_PARENT );
                if( p_playlist )
                {
                    val.i_int = p_playlist->i_index;
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    var_Set( p_playlist, "item-change", val );
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    vlc_object_release( p_playlist );
                }
            }
        }
        break;

        case INPUT_GET_INFO:
        {
            char *psz_cat = (char *)va_arg( args, char * );
            char *psz_name = (char *)va_arg( args, char * );
            char **ppsz_value = (char **)va_arg( args, char ** );
            int i;

            i_ret = VLC_EGENERIC;
            *ppsz_value = NULL;

            vlc_mutex_lock( &p_input->p_item->lock );
            for( i = 0; i < p_input->p_item->i_categories; i++ )
            {
                if( !strcmp( p_input->p_item->pp_categories[i]->psz_name,
                             psz_cat ) )
                    break;
            }

            if( i != p_input->p_item->i_categories )
            {
                info_category_t *p_cat;
                p_cat = p_input->p_item->pp_categories[i];

                for( i = 0; i < p_cat->i_infos; i++ )
                {
                    if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
                    {
                        if( p_cat->pp_infos[i]->psz_value )
                        {
                            *ppsz_value =strdup(p_cat->pp_infos[i]->psz_value);
                            i_ret = VLC_SUCCESS;
                        }
                        break;
                    }
                }
            }
            vlc_mutex_unlock( &p_input->p_item->lock );
        }
        break;

        case INPUT_ADD_BOOKMARK:
            p_bkmk = (seekpoint_t *)va_arg( args, seekpoint_t * );
            p_bkmk = vlc_seekpoint_Duplicate( p_bkmk );
            if( !p_bkmk->psz_name )
            {
                 asprintf( &p_bkmk->psz_name, _("Bookmark %i"),
                           p_input->i_bookmarks );
            }
            TAB_APPEND( p_input->i_bookmarks, p_input->pp_bookmarks, p_bkmk );

            /* Reflect the changes on the object var */
            var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
            {
                int i;
                for( i = 0; i < p_input->i_bookmarks; i++ )
                {
                    val.i_int = i;
                    text.psz_string = p_input->pp_bookmarks[i]->psz_name;
                    var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                                &val, &text );
                }
            }

            UpdateBookmarksOption( p_input );

            i_ret = VLC_SUCCESS;
            break;

        case INPUT_CHANGE_BOOKMARK:
             p_bkmk = (seekpoint_t *)va_arg( args, seekpoint_t * );
             i_bkmk = (int)va_arg( args, int );

             if( i_bkmk < p_input->i_bookmarks )
             {
                 p_input->pp_bookmarks[i_bkmk] = p_bkmk;

                 /* Reflect the changes on the object var */
                 var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
                 for( i = 0; i < p_input->i_bookmarks; i++ )
                 {
                     val.i_int = i;
                     text.psz_string = p_input->pp_bookmarks[i]->psz_name;
                     var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                                 &val, &text );
                 }
             }
             UpdateBookmarksOption( p_input );

             i_ret = VLC_SUCCESS;
             break;

        case INPUT_DEL_BOOKMARK:
            i_bkmk = (int)va_arg( args, int );
            if( i_bkmk < p_input->i_bookmarks )
            {
                int i;
                p_bkmk = p_input->pp_bookmarks[i_bkmk];
                TAB_REMOVE( p_input->i_bookmarks, p_input->pp_bookmarks,
                            p_bkmk );
                vlc_seekpoint_Delete( p_bkmk );

                /* Reflect the changes on the object var */
                var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
                for( i = 0; i < p_input->i_bookmarks; i++ )
                {
                    val.i_int = i;
                    text.psz_string = p_input->pp_bookmarks[i]->psz_name;
                    var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                                &val, &text );
                }

                UpdateBookmarksOption( p_input );

                i_ret = VLC_SUCCESS;
            }
            else i_ret = VLC_EGENERIC;
            break;

        case INPUT_GET_BOOKMARKS:
            ppp_bkmk = (seekpoint_t ***)va_arg( args, seekpoint_t *** );
            pi_bkmk = (int *)va_arg( args, int * );
            if( p_input->i_bookmarks )
            {
                int i;

                *pi_bkmk = p_input->i_bookmarks;
                *ppp_bkmk = malloc( sizeof(seekpoint_t *) *
                              p_input->i_bookmarks );
                for( i = 0; i < p_input->i_bookmarks; i++ )
                {
                    (*ppp_bkmk)[i] =
                        vlc_seekpoint_Duplicate(p_input->pp_bookmarks[i]);
                }
                i_ret = VLC_SUCCESS;
            }
            else
            {
                *ppp_bkmk = NULL;
                *pi_bkmk = 0;
                i_ret = VLC_EGENERIC;
            }
            break;

        case INPUT_CLEAR_BOOKMARKS:
            if( p_input->i_bookmarks )
            {
                int i;

                for( i = p_input->i_bookmarks - 1; i >= 0; i-- )
                {
                    p_bkmk = p_input->pp_bookmarks[i];
                    TAB_REMOVE( p_input->i_bookmarks, p_input->pp_bookmarks,
                                p_bkmk );
                    vlc_seekpoint_Delete( p_bkmk );
                }
                var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
            }

            UpdateBookmarksOption( p_input );

            i_ret = VLC_SUCCESS;
            break;

        case INPUT_SET_BOOKMARK:
            i_bkmk = (int)va_arg( args, int );
            if( i_bkmk >= 0 && i_bkmk < p_input->i_bookmarks )
            {
                vlc_value_t pos;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                if( p_input->pp_bookmarks[i_bkmk]->i_byte_offset ||
                    ( !p_input->pp_bookmarks[i_bkmk]->i_byte_offset &&
                      !p_input->pp_bookmarks[i_bkmk]->i_time_offset ) )
                {
                    pos.f_float = p_input->pp_bookmarks[i_bkmk]->i_byte_offset/
                        (double)p_input->stream.p_selected_area->i_size;
                    i_ret = var_Set( p_input, "position", pos );
                }
                else if( p_input->pp_bookmarks[i_bkmk]->i_time_offset )
                {
                    pos.i_time = p_input->pp_bookmarks[i_bkmk]->i_time_offset;
                    i_ret = var_Set( p_input, "time", pos );
                }
                vlc_mutex_lock( &p_input->stream.stream_lock );
            }
            else
            {
                i_ret = VLC_EGENERIC;
            }
            break;

        case INPUT_GET_SUBDELAY:
            pi = (int*)va_arg( args, int *);
            /* We work on the first subtitle */
            if( p_input->p_sys != NULL )
            {
                if( p_input->p_sys->i_sub > 0 )
                {
                    i_ret = var_Get( (vlc_object_t *)p_input->p_sys->sub[0],
                                      "sub-delay", &val );
                    *pi = val.i_int;
                }
                else
                {
                    msg_Dbg( p_input,"no subtitle track");
                    i_ret = VLC_EGENERIC;
                }
            }
            else
            {
                i_ret = VLC_EGENERIC;
            }
            break;

        case INPUT_SET_SUBDELAY:
            i = (int)va_arg( args, int );
            /* We work on the first subtitle */
            if( p_input->p_sys )
            {
                if( p_input->p_sys->i_sub > 0 )
                {
                    val.i_int = i;
                    i_ret = var_Set( (vlc_object_t *)p_input->p_sys->sub[0],
                                      "sub-delay", val );
                }
                else
                {
                    msg_Dbg( p_input,"no subtitle track");
                    i_ret = VLC_EGENERIC;
                }
            }
            else
            {
                i_ret = VLC_EGENERIC;
            }
            break;

        default:
            msg_Err( p_input, "unknown query in input_vaControl" );
            i_ret = VLC_EGENERIC;
            break;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return i_ret;
}

static void UpdateBookmarksOption( input_thread_t *p_input )
{
    int i, i_len = 0;
    char *psz_value = NULL, *psz_next = NULL;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    for( i = 0; i < p_input->i_bookmarks; i++ )
    {
        asprintf( &psz_value, "{name=%s,bytes="I64Fd",time="I64Fd"}",
                  p_input->pp_bookmarks[i]->psz_name,
                  p_input->pp_bookmarks[i]->i_byte_offset,
                  p_input->pp_bookmarks[i]->i_time_offset/1000000 );
        i_len += strlen( psz_value );
        free( psz_value );
    }
    for( i = 0; i < p_input->i_bookmarks; i++ )
    {
        if( !i ) psz_value = psz_next = malloc( i_len + p_input->i_bookmarks );

        sprintf( psz_next, "{name=%s,bytes="I64Fd",time="I64Fd"}",
                 p_input->pp_bookmarks[i]->psz_name,
                 p_input->pp_bookmarks[i]->i_byte_offset,
                 p_input->pp_bookmarks[i]->i_time_offset/1000000 );

        psz_next += strlen( psz_next );
        if( i < p_input->i_bookmarks - 1)
        {
            *psz_next = ','; psz_next++;
        }
    }
    input_Control( p_input, INPUT_ADD_OPTION, "bookmarks",
                   psz_value ? psz_value : "" );

    vlc_mutex_lock( &p_input->stream.stream_lock );
}
