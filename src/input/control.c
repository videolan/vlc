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
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "input_internal.h"
#include "vlc_playlist.h"


static void UpdateBookmarksOption( input_thread_t * );
static void NotifyPlaylist( input_thread_t * );

/****************************************************************************
 * input_Control
 ****************************************************************************/
/**
 * Control function for inputs.
 * \param p_input input handle
 * \param i_query query type
 * \return VLC_SUCCESS if ok
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
    seekpoint_t *p_bkmk, ***ppp_bkmk;
    int i_bkmk = 0;
    int *pi_bkmk;

    int i_int, *pi_int;
    double f, *pf;
    int64_t i_64, *pi_64;

    char *psz;
    vlc_value_t val;

    switch( i_query )
    {
        case INPUT_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            *pf = var_GetFloat( p_input, "position" );
            return VLC_SUCCESS;

        case INPUT_SET_POSITION:
            f = (double)va_arg( args, double );
            return var_SetFloat( p_input, "position", f );

        case INPUT_GET_LENGTH:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetTime( p_input, "length" );
            return VLC_SUCCESS;

        case INPUT_GET_TIME:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetTime( p_input, "time" );
            return VLC_SUCCESS;

        case INPUT_SET_TIME:
            i_64 = (int64_t)va_arg( args, int64_t );
            return var_SetTime( p_input, "time", i_64 );

        case INPUT_GET_RATE:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = var_GetInteger( p_input, "rate" );
            return VLC_SUCCESS;

        case INPUT_SET_RATE:
            i_int = (int)va_arg( args, int );
            return var_SetInteger( p_input, "rate", i_int );

        case INPUT_GET_STATE:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = var_GetInteger( p_input, "state" );
            return VLC_SUCCESS;

        case INPUT_SET_STATE:
            i_int = (int)va_arg( args, int );
            return var_SetInteger( p_input, "state", i_int );

        case INPUT_GET_AUDIO_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetTime( p_input, "audio-delay" );
            return VLC_SUCCESS;

        case INPUT_GET_SPU_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetTime( p_input, "spu-delay" );
            return VLC_SUCCESS;

        case INPUT_SET_AUDIO_DELAY:
            i_64 = (int64_t)va_arg( args, int64_t );
            return var_SetTime( p_input, "audio-delay", i_64 );

        case INPUT_SET_SPU_DELAY:
            i_64 = (int64_t)va_arg( args, int64_t );
            return var_SetTime( p_input, "spu-delay", i_64 );

        case INPUT_ADD_INFO:
        {
            /* FIXME : Impossible to use vlc_input_item_AddInfo because of
             * the ... problem ? */
            char *psz_cat = (char *)va_arg( args, char * );
            char *psz_name = (char *)va_arg( args, char * );
            char *psz_format = (char *)va_arg( args, char * );

            info_category_t *p_cat;
            info_t *p_info;
            int i;

            vlc_mutex_lock( &p_input->input.p_item->lock );
            for( i = 0; i < p_input->input.p_item->i_categories; i++ )
            {
                if( !strcmp( p_input->input.p_item->pp_categories[i]->psz_name,
                             psz_cat ) ) break;
            }

            if( i == p_input->input.p_item->i_categories )
            {
                p_cat = malloc( sizeof( info_category_t ) );
                if( !p_cat )
                {
                    vlc_mutex_lock( &p_input->input.p_item->lock );
                    return VLC_EGENERIC;
                }
                p_cat->psz_name = strdup( psz_cat );
                p_cat->i_infos = 0;
                p_cat->pp_infos = NULL;
                INSERT_ELEM( p_input->input.p_item->pp_categories,
                             p_input->input.p_item->i_categories,
                             p_input->input.p_item->i_categories, p_cat );
            }

            p_cat = p_input->input.p_item->pp_categories[i];

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
                if( !p_info )
                {
                    vlc_mutex_lock( &p_input->input.p_item->lock );
                    return VLC_EGENERIC;
                }

                INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos,
                             p_cat->i_infos, p_info );
                p_info->psz_name = strdup( psz_name );
            }

            p_info = p_cat->pp_infos[i];
            vasprintf( &p_info->psz_value, psz_format, args );

            vlc_mutex_unlock( &p_input->input.p_item->lock );

            NotifyPlaylist( p_input );
        }
        return VLC_SUCCESS;
        case INPUT_DEL_INFO:
        {
            char *psz_cat = (char *)va_arg( args, char * );
            char *psz_name = (char *)va_arg( args, char * );

            info_category_t *p_cat = NULL;
            int i;

            vlc_mutex_lock( &p_input->input.p_item->lock );
            for( i = 0; i < p_input->input.p_item->i_categories; i++ )
            {
                if( !strcmp( p_input->input.p_item->pp_categories[i]->psz_name,
                             psz_cat ) )
                {
                    p_cat = p_input->input.p_item->pp_categories[i];
                    break;
                }
            }
            if( p_cat == NULL )
            {
                vlc_mutex_unlock( &p_input->input.p_item->lock );
                return VLC_EGENERIC;
            }

            for( i = 0; i < p_cat->i_infos; i++ )
            {
                if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
                {
                    REMOVE_ELEM( p_cat->pp_infos, p_cat->i_infos, i );
                    break;
                }
            }
            vlc_mutex_unlock( &p_input->input.p_item->lock );

            if( i >= p_cat->i_infos )
                return VLC_EGENERIC;

            NotifyPlaylist( p_input );
        }
        return VLC_SUCCESS;


        case INPUT_GET_INFO:
        {
            char *psz_cat = (char *)va_arg( args, char * );
            char *psz_name = (char *)va_arg( args, char * );
            char **ppsz_value = (char **)va_arg( args, char ** );
            int i_ret = VLC_EGENERIC;
            *ppsz_value = NULL;

            *ppsz_value = vlc_input_item_GetInfo( p_input->input.p_item,
                                                  psz_cat, psz_name );
            return i_ret;
        }

        case INPUT_SET_NAME:
        {
            char *psz_name = (char *)va_arg( args, char * );

            if( !psz_name ) return VLC_EGENERIC;

            vlc_mutex_lock( &p_input->input.p_item->lock );
            if( p_input->input.p_item->psz_name )
                free( p_input->input.p_item->psz_name );
            p_input->input.p_item->psz_name = strdup( psz_name );
            vlc_mutex_unlock( &p_input->input.p_item->lock );

            NotifyPlaylist( p_input );

            return VLC_SUCCESS;
        }

        case INPUT_ADD_BOOKMARK:
            p_bkmk = (seekpoint_t *)va_arg( args, seekpoint_t * );
            p_bkmk = vlc_seekpoint_Duplicate( p_bkmk );

            vlc_mutex_lock( &p_input->input.p_item->lock );
            if( !p_bkmk->psz_name )
            {
                 asprintf( &p_bkmk->psz_name, _("Bookmark %i"),
                           p_input->i_bookmark );
            }

            TAB_APPEND( p_input->i_bookmark, p_input->bookmark, p_bkmk );

            /* Reflect the changes on the object var */
            var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
            {
                vlc_value_t val, text;
                int i;

                for( i = 0; i < p_input->i_bookmark; i++ )
                {
                    val.i_int = i;
                    text.psz_string = p_input->bookmark[i]->psz_name;
                    var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                                &val, &text );
                }
            }
            vlc_mutex_unlock( &p_input->input.p_item->lock );

            UpdateBookmarksOption( p_input );

            return VLC_SUCCESS;

        case INPUT_CHANGE_BOOKMARK:
            p_bkmk = (seekpoint_t *)va_arg( args, seekpoint_t * );
            i_bkmk = (int)va_arg( args, int );

            vlc_mutex_lock( &p_input->input.p_item->lock );
            if( i_bkmk < p_input->i_bookmark )
            {
                vlc_value_t val, text;
                int i;

                p_input->bookmark[i_bkmk] = p_bkmk;

                /* Reflect the changes on the object var */
                var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
                for( i = 0; i < p_input->i_bookmark; i++ )
                {
                    val.i_int = i;
                    text.psz_string = p_input->bookmark[i]->psz_name;
                    var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                                &val, &text );
                }
            }
            vlc_mutex_unlock( &p_input->input.p_item->lock );

            UpdateBookmarksOption( p_input );

            return VLC_SUCCESS;

        case INPUT_DEL_BOOKMARK:
            i_bkmk = (int)va_arg( args, int );

            vlc_mutex_lock( &p_input->input.p_item->lock );
            if( i_bkmk < p_input->i_bookmark )
            {
                vlc_value_t val, text;
                int i;

                p_bkmk = p_input->bookmark[i_bkmk];
                TAB_REMOVE( p_input->i_bookmark, p_input->bookmark,
                            p_bkmk );
                vlc_seekpoint_Delete( p_bkmk );

                /* Reflect the changes on the object var */
                var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
                for( i = 0; i < p_input->i_bookmark; i++ )
                {
                    val.i_int = i;
                    text.psz_string = p_input->bookmark[i]->psz_name;
                    var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                                &val, &text );
                }
                vlc_mutex_unlock( &p_input->input.p_item->lock );

                UpdateBookmarksOption( p_input );

                return VLC_SUCCESS;
            }
            vlc_mutex_unlock( &p_input->input.p_item->lock );

            return VLC_EGENERIC;

        case INPUT_GET_BOOKMARKS:
            ppp_bkmk = (seekpoint_t ***)va_arg( args, seekpoint_t *** );
            pi_bkmk = (int *)va_arg( args, int * );

            vlc_mutex_lock( &p_input->input.p_item->lock );
            if( p_input->i_bookmark )
            {
                int i;

                *pi_bkmk = p_input->i_bookmark;
                *ppp_bkmk = malloc( sizeof(seekpoint_t *) *
                                    p_input->i_bookmark );
                for( i = 0; i < p_input->i_bookmark; i++ )
                {
                    (*ppp_bkmk)[i] =
                        vlc_seekpoint_Duplicate(p_input->bookmark[i]);
                }

                vlc_mutex_unlock( &p_input->input.p_item->lock );
                return VLC_SUCCESS;
            }
            else
            {
                *ppp_bkmk = NULL;
                *pi_bkmk = 0;

                vlc_mutex_unlock( &p_input->input.p_item->lock );
                return VLC_EGENERIC;
            }
            break;

        case INPUT_CLEAR_BOOKMARKS:

            vlc_mutex_lock( &p_input->input.p_item->lock );
            if( p_input->i_bookmark )
            {
                int i;

                for( i = p_input->i_bookmark - 1; i >= 0; i-- )
                {
                    p_bkmk = p_input->bookmark[i];
                    TAB_REMOVE( p_input->i_bookmark, p_input->bookmark,
                                p_bkmk );
                    vlc_seekpoint_Delete( p_bkmk );
                }
                var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );
            }
            vlc_mutex_unlock( &p_input->input.p_item->lock );

            UpdateBookmarksOption( p_input );

            return VLC_SUCCESS;

        case INPUT_SET_BOOKMARK:
            i_bkmk = (int)va_arg( args, int );

            vlc_mutex_lock( &p_input->input.p_item->lock );
            if( i_bkmk >= 0 && i_bkmk < p_input->i_bookmark )
            {
                vlc_value_t pos;
                int i_ret;

                if( p_input->bookmark[i_bkmk]->i_time_offset != -1 )
                {
                    pos.i_time = p_input->bookmark[i_bkmk]->i_time_offset;
                    i_ret = var_Set( p_input, "time", pos );
                }
                else if( p_input->bookmark[i_bkmk]->i_byte_offset != -1 )
                {
                    // don't crash on bookmarks in live streams
                    if( stream_Size( p_input->input.p_stream ) == 0 )
                    {
                        vlc_mutex_unlock( &p_input->input.p_item->lock );
                        return VLC_EGENERIC;
                    }
                    pos.f_float = !p_input->input.p_stream ? 0 :
                        p_input->bookmark[i_bkmk]->i_byte_offset /
                        stream_Size( p_input->input.p_stream );
                    i_ret = var_Set( p_input, "position", pos );
                }
                else
                {
                    pos.f_float = 0;
                    i_ret = var_Set( p_input, "position", pos );
                }

                vlc_mutex_unlock( &p_input->input.p_item->lock );
                return i_ret;
            }
            else
            {
                vlc_mutex_unlock( &p_input->input.p_item->lock );
                return VLC_EGENERIC;
            }

            break;

        case INPUT_ADD_OPTION:
        {
            char *psz_option = (char *)va_arg( args, char * );
            char *psz_value = (char *)va_arg( args, char * );
            int i;

            vlc_mutex_lock( &p_input->input.p_item->lock );
            /* Check if option already exists */
            for( i = 0; i < p_input->input.p_item->i_options; i++ )
            {
                if( !strncmp( p_input->input.p_item->ppsz_options[i],
                              psz_option, strlen( psz_option ) ) &&
                    p_input->input.p_item->ppsz_options[i][strlen(psz_option)]
                      == '=' )
                {
                    free( p_input->input.p_item->ppsz_options[i] );
                    break;
                }
            }
            if( i == p_input->input.p_item->i_options )
            {
                p_input->input.p_item->i_options++;
                p_input->input.p_item->ppsz_options =
                    realloc( p_input->input.p_item->ppsz_options,
                             p_input->input.p_item->i_options *
                             sizeof(char **) );
            }

            asprintf( &p_input->input.p_item->ppsz_options[i],
                      "%s=%s", psz_option, psz_value ) ;
            vlc_mutex_unlock( &p_input->input.p_item->lock );

            return VLC_SUCCESS;
        }

        case INPUT_GET_BYTE_POSITION:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = !p_input->input.p_stream ? 0 :
                stream_Tell( p_input->input.p_stream );
            return VLC_SUCCESS;

        case INPUT_SET_BYTE_SIZE:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = !p_input->input.p_stream ? 0 :
                stream_Size( p_input->input.p_stream );
            return VLC_SUCCESS;

        case INPUT_ADD_SLAVE:
            psz = (char*)va_arg( args, char * );
            if( psz && *psz )
            {
                val.psz_string = strdup( psz );
                input_ControlPush( p_input, INPUT_CONTROL_ADD_SLAVE, &val );
            }
            return VLC_SUCCESS;

        default:
            msg_Err( p_input, "unknown query in input_vaControl" );
            return VLC_EGENERIC;
    }
}

static void NotifyPlaylist( input_thread_t *p_input )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                       FIND_PARENT );
    if( p_playlist )
    {
        var_SetInteger( p_playlist, "item-change", p_playlist->i_index );
        vlc_object_release( p_playlist );
    }
}

static void UpdateBookmarksOption( input_thread_t *p_input )
{
    int i, i_len = 0;
    char *psz_value = NULL, *psz_next = NULL;

    vlc_mutex_lock( &p_input->input.p_item->lock );
    for( i = 0; i < p_input->i_bookmark; i++ )
    {
        asprintf( &psz_value, "{name=%s,bytes="I64Fd",time="I64Fd"}",
                  p_input->bookmark[i]->psz_name,
                  p_input->bookmark[i]->i_byte_offset,
                  p_input->bookmark[i]->i_time_offset/1000000 );
        i_len += strlen( psz_value );
        free( psz_value );
    }
    for( i = 0; i < p_input->i_bookmark; i++ )
    {
        if( !i ) psz_value = psz_next = malloc( i_len + p_input->i_bookmark );

        sprintf( psz_next, "{name=%s,bytes="I64Fd",time="I64Fd"}",
                 p_input->bookmark[i]->psz_name,
                 p_input->bookmark[i]->i_byte_offset,
                 p_input->bookmark[i]->i_time_offset/1000000 );

        psz_next += strlen( psz_next );
        if( i < p_input->i_bookmark - 1)
        {
            *psz_next = ','; psz_next++;
        }
    }
    vlc_mutex_unlock( &p_input->input.p_item->lock );

    input_Control( p_input, INPUT_ADD_OPTION, "bookmarks",
                   psz_value ? psz_value : "" );
}
