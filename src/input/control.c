/*****************************************************************************
 * control.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: stream.c 7041 2004-03-11 16:48:27Z gbazin $
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

#include "ninput.h"

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
    int i_bkmk, *pi_bkmk;
    vlc_value_t val, text;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    switch( i_query )
    {
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

            i_ret = VLC_SUCCESS;
            break;

        case INPUT_DEL_BOOKMARK:
            p_bkmk = (seekpoint_t *)va_arg( args, seekpoint_t * );
            if( p_input->i_bookmarks )
            {
                int i;
                for( i = 0; i < p_input->i_bookmarks; i++ )
                {
                    if( ( p_bkmk->i_byte_offset &&
                          p_input->pp_bookmarks[i]->i_byte_offset ==
                            p_bkmk->i_byte_offset ) ||
                        ( p_bkmk->i_time_offset &&
                          p_input->pp_bookmarks[i]->i_time_offset ==
                            p_bkmk->i_time_offset ) ||
                        ( !p_bkmk->i_byte_offset && !p_bkmk->i_time_offset &&
                          p_input->pp_bookmarks[i]->i_byte_offset ==
                            p_bkmk->i_byte_offset ) )
                    {
                        p_bkmk = p_input->pp_bookmarks[i];
                        break;
                    }
                }
                if( i < p_input->i_bookmarks )
                {
                    TAB_REMOVE( p_input->i_bookmarks, p_input->pp_bookmarks,
                                p_bkmk );
                    vlc_seekpoint_Delete( p_bkmk );

                    /* Reflect the changes on the object var */
                    var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES,
                                0, 0 );
                    for( i = 0; i < p_input->i_bookmarks; i++ )
                    {
                        val.i_int = i;
                        text.psz_string = p_input->pp_bookmarks[i]->psz_name;
                        var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                                    &val, &text );
                    }
                }
            }
            i_ret = VLC_SUCCESS;
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
            }
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

        default:
            msg_Err( p_input, "unknown query in input_vaControl" );
            i_ret = VLC_EGENERIC;
            break;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return i_ret;
}
