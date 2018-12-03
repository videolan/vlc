/*****************************************************************************
 * control.c
 *****************************************************************************
 * Copyright (C) 1999-2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "input_internal.h"
#include "event.h"
#include "resource.h"
#include "es_out.h"

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
    input_thread_private_t *priv = input_priv(p_input);
    seekpoint_t *p_bkmk, ***ppp_bkmk;
    int i_bkmk = 0;
    int *pi_bkmk;
    bool b_bool;

    char *psz;
    vlc_value_t val;

    switch( i_query )
    {
        case INPUT_NAV_ACTIVATE:
        case INPUT_NAV_UP:
        case INPUT_NAV_DOWN:
        case INPUT_NAV_LEFT:
        case INPUT_NAV_RIGHT:
        case INPUT_NAV_POPUP:
        case INPUT_NAV_MENU:
            input_ControlPushHelper( p_input, i_query - INPUT_NAV_ACTIVATE
                               + INPUT_CONTROL_NAV_ACTIVATE, NULL );
            return VLC_SUCCESS;

        case INPUT_ADD_BOOKMARK:
            p_bkmk = va_arg( args, seekpoint_t * );
            p_bkmk = vlc_seekpoint_Duplicate( p_bkmk );

            vlc_mutex_lock( &priv->p_item->lock );
            if( !p_bkmk->psz_name )
            {
                 if( asprintf( &p_bkmk->psz_name, _("Bookmark %i"),
                               priv->i_bookmark ) == -1 )
                     p_bkmk->psz_name = NULL;
            }

            if( p_bkmk->psz_name )
                TAB_APPEND( priv->i_bookmark, priv->pp_bookmark, p_bkmk );
            else
            {
                vlc_seekpoint_Delete( p_bkmk );
                p_bkmk = NULL;
            }
            vlc_mutex_unlock( &priv->p_item->lock );

            input_SendEventBookmark( p_input );

            return p_bkmk ? VLC_SUCCESS : VLC_EGENERIC;

        case INPUT_CHANGE_BOOKMARK:
            p_bkmk = va_arg( args, seekpoint_t * );
            i_bkmk = va_arg( args, int );

            vlc_mutex_lock( &priv->p_item->lock );
            if( i_bkmk < priv->i_bookmark )
            {
                p_bkmk = vlc_seekpoint_Duplicate( p_bkmk );
                if( p_bkmk )
                {
                    vlc_seekpoint_Delete( priv->pp_bookmark[i_bkmk] );
                    priv->pp_bookmark[i_bkmk] = p_bkmk;
                }
            }
            else p_bkmk = NULL;
            vlc_mutex_unlock( &priv->p_item->lock );

            input_SendEventBookmark( p_input );

            return p_bkmk ? VLC_SUCCESS : VLC_EGENERIC;

        case INPUT_DEL_BOOKMARK:
            i_bkmk = va_arg( args, int );

            vlc_mutex_lock( &priv->p_item->lock );
            if( i_bkmk < priv->i_bookmark )
            {
                p_bkmk = priv->pp_bookmark[i_bkmk];
                TAB_REMOVE( priv->i_bookmark, priv->pp_bookmark, p_bkmk );
                vlc_seekpoint_Delete( p_bkmk );

                vlc_mutex_unlock( &priv->p_item->lock );

                input_SendEventBookmark( p_input );

                return VLC_SUCCESS;
            }
            vlc_mutex_unlock( &priv->p_item->lock );

            return VLC_EGENERIC;

        case INPUT_GET_BOOKMARKS:
            ppp_bkmk = va_arg( args, seekpoint_t *** );
            pi_bkmk = va_arg( args, int * );

            vlc_mutex_lock( &priv->p_item->lock );
            if( priv->i_bookmark )
            {
                int i;

                *pi_bkmk = priv->i_bookmark;
                *ppp_bkmk = vlc_alloc( priv->i_bookmark, sizeof(seekpoint_t *) );
                for( i = 0; i < priv->i_bookmark; i++ )
                {
                    (*ppp_bkmk)[i] =
                        vlc_seekpoint_Duplicate( input_priv(p_input)->pp_bookmark[i] );
                }

                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_SUCCESS;
            }
            else
            {
                *ppp_bkmk = NULL;
                *pi_bkmk = 0;

                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_EGENERIC;
            }
            break;

        case INPUT_CLEAR_BOOKMARKS:
            vlc_mutex_lock( &priv->p_item->lock );
            for( int i = 0; i < priv->i_bookmark; ++i )
                vlc_seekpoint_Delete( priv->pp_bookmark[i] );

            TAB_CLEAN( priv->i_bookmark, priv->pp_bookmark );
            vlc_mutex_unlock( &priv->p_item->lock );

            input_SendEventBookmark( p_input );
            return VLC_SUCCESS;

        case INPUT_SET_BOOKMARK:
            i_bkmk = va_arg( args, int );

            val.i_int = i_bkmk;
            input_ControlPushHelper( p_input, INPUT_CONTROL_SET_BOOKMARK, &val );

            return VLC_SUCCESS;

        case INPUT_GET_BOOKMARK:
            p_bkmk = va_arg( args, seekpoint_t * );

            vlc_mutex_lock( &priv->p_item->lock );
            *p_bkmk = priv->bookmark;
            vlc_mutex_unlock( &priv->p_item->lock );
            return VLC_SUCCESS;

        case INPUT_GET_FULL_TITLE_INFO:
        {
            vlc_mutex_lock( &priv->p_item->lock );
            unsigned count = priv->i_title;
            input_title_t **array = vlc_alloc( count, sizeof (*array) );

            if( count > 0 && unlikely(array == NULL) )
            {
                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_ENOMEM;
            }

            for( unsigned i = 0; i < count; i++ )
                array[i] = vlc_input_title_Duplicate( priv->title[i] );

            vlc_mutex_unlock( &priv->p_item->lock );

            *va_arg( args, input_title_t *** ) = array;
            *va_arg( args, int * ) = count;
            return VLC_SUCCESS;
        }

        case INPUT_ADD_SLAVE:
        {
            enum slave_type type =  va_arg( args, enum slave_type );
            psz = va_arg( args, char * );
            b_bool = va_arg( args, int );
            bool b_notify = va_arg( args, int );
            bool b_check_ext = va_arg( args, int );

            if( !psz || ( type != SLAVE_TYPE_SPU && type != SLAVE_TYPE_AUDIO ) )
                return VLC_EGENERIC;
            if( b_check_ext && type == SLAVE_TYPE_SPU &&
                !subtitles_Filter( psz ) )
                return VLC_EGENERIC;

            input_item_slave_t *p_slave =
                input_item_slave_New( psz, type, SLAVE_PRIORITY_USER );
            if( !p_slave )
                return VLC_ENOMEM;
            p_slave->b_forced = b_bool;

            val.p_address = p_slave;
            input_ControlPushHelper( p_input, INPUT_CONTROL_ADD_SLAVE, &val );
            if( b_notify )
            {
                vout_thread_t *p_vout = input_GetVout( p_input );
                if( p_vout )
                {
                    switch( type )
                    {
                        case SLAVE_TYPE_AUDIO:
                            vout_OSDMessage(p_vout, VOUT_SPU_CHANNEL_OSD, "%s",
                                            vlc_gettext("Audio track added"));
                            break;
                        case SLAVE_TYPE_SPU:
                            vout_OSDMessage(p_vout, VOUT_SPU_CHANNEL_OSD, "%s",
                                            vlc_gettext("Subtitle track added"));
                            break;
                    }
                    vlc_object_release( (vlc_object_t *)p_vout );
                }
            }
            return VLC_SUCCESS;
        }

        case INPUT_RESTART_ES_BY_ID:
            val.i_int = va_arg( args, int );
            input_ControlPushHelper( p_input, INPUT_CONTROL_RESTART_ES_BY_ID, &val );
            return VLC_SUCCESS;

        case INPUT_UPDATE_VIEWPOINT:
        case INPUT_SET_INITIAL_VIEWPOINT:
        {
            input_control_param_t param;
            param.viewpoint = *va_arg( args, const vlc_viewpoint_t* );
            if ( i_query == INPUT_SET_INITIAL_VIEWPOINT )
                input_ControlPush( p_input, INPUT_CONTROL_SET_INITIAL_VIEWPOINT,
                                   &param );
            else if ( va_arg( args, int ) )
                input_ControlPush( p_input, INPUT_CONTROL_SET_VIEWPOINT, &param);
            else
                input_ControlPush( p_input, INPUT_CONTROL_UPDATE_VIEWPOINT, &param );
            return VLC_SUCCESS;
        }

        case INPUT_GET_AOUT:
        {
            audio_output_t *p_aout = input_resource_HoldAout( priv->p_resource );
            if( !p_aout )
                return VLC_EGENERIC;

            audio_output_t **pp_aout = va_arg( args, audio_output_t** );
            *pp_aout = p_aout;
            return VLC_SUCCESS;
        }

        case INPUT_GET_VOUTS:
        {
            vout_thread_t ***ppp_vout = va_arg( args, vout_thread_t*** );
            size_t *pi_vout = va_arg( args, size_t * );

            input_resource_HoldVouts( priv->p_resource, ppp_vout, pi_vout );
            if( *pi_vout == 0 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }

        case INPUT_GET_ES_OBJECTS:
        {
            const int i_id = va_arg( args, int );
            vlc_object_t **pp_decoder = va_arg( args, vlc_object_t ** );
            vout_thread_t **pp_vout = va_arg( args, vout_thread_t ** );
            audio_output_t **pp_aout = va_arg( args, audio_output_t ** );

            return es_out_Control( priv->p_es_out_display,
                                   ES_OUT_GET_ES_OBJECTS_BY_ID, i_id,
                                   pp_decoder, pp_vout, pp_aout );
        }

        case INPUT_GET_PCR_SYSTEM:
        {
            vlc_tick_t *pi_system = va_arg( args, vlc_tick_t * );
            vlc_tick_t *pi_delay  = va_arg( args, vlc_tick_t * );
            return es_out_ControlGetPcrSystem( priv->p_es_out_display, pi_system, pi_delay );
        }

        case INPUT_MODIFY_PCR_SYSTEM:
        {
            bool b_absolute = va_arg( args, int );
            vlc_tick_t i_system = va_arg( args, vlc_tick_t );
            return es_out_ControlModifyPcrSystem( priv->p_es_out_display, b_absolute, i_system );
        }

        case INPUT_SET_RENDERER:
        {
            vlc_renderer_item_t* p_item = va_arg( args, vlc_renderer_item_t* );
            val.p_address = p_item ? vlc_renderer_item_hold( p_item ) : NULL;
            input_ControlPushHelper( p_input, INPUT_CONTROL_SET_RENDERER, &val );
            return VLC_SUCCESS;
        }

        default:
            msg_Err( p_input, "unknown query 0x%x in %s", i_query, __func__ );
            return VLC_EGENERIC;
    }
}
