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
#include <vlc_memstream.h>
#include <vlc_renderer_discovery.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "input_internal.h"
#include "event.h"
#include "resource.h"
#include "es_out.h"


static void UpdateBookmarksOption( input_thread_t * );

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

    int i_int, *pi_int;
    bool b_bool, *pb_bool;
    double f, *pf;
    int64_t i_64, *pi_64;

    char *psz;
    vlc_value_t val;

    switch( i_query )
    {
        case INPUT_GET_POSITION:
            pf = va_arg( args, double * );
            *pf = var_GetFloat( p_input, "position" );
            return VLC_SUCCESS;

        case INPUT_SET_POSITION:
            f = va_arg( args, double );
            return var_SetFloat( p_input, "position", f );

        case INPUT_GET_LENGTH:
            pi_64 = va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_input, "length" );
            return VLC_SUCCESS;

        case INPUT_GET_TIME:
            pi_64 = va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_input, "time" );
            return VLC_SUCCESS;

        case INPUT_SET_TIME:
            i_64 = va_arg( args, int64_t );
            return var_SetInteger( p_input, "time", i_64 );

        case INPUT_GET_RATE:
            pi_int = va_arg( args, int * );
            *pi_int = INPUT_RATE_DEFAULT / var_GetFloat( p_input, "rate" );
            return VLC_SUCCESS;

        case INPUT_SET_RATE:
            i_int = va_arg( args, int );
            return var_SetFloat( p_input, "rate",
                                 (float)INPUT_RATE_DEFAULT / (float)i_int );

        case INPUT_GET_STATE:
            pi_int = va_arg( args, int * );
            *pi_int = var_GetInteger( p_input, "state" );
            return VLC_SUCCESS;

        case INPUT_SET_STATE:
            i_int = va_arg( args, int );
            return var_SetInteger( p_input, "state", i_int );

        case INPUT_GET_AUDIO_DELAY:
            pi_64 = va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_input, "audio-delay" );
            return VLC_SUCCESS;

        case INPUT_GET_SPU_DELAY:
            pi_64 = va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_input, "spu-delay" );
            return VLC_SUCCESS;

        case INPUT_SET_AUDIO_DELAY:
            i_64 = va_arg( args, int64_t );
            return var_SetInteger( p_input, "audio-delay", i_64 );

        case INPUT_SET_SPU_DELAY:
            i_64 = va_arg( args, int64_t );
            return var_SetInteger( p_input, "spu-delay", i_64 );

        case INPUT_NAV_ACTIVATE:
        case INPUT_NAV_UP:
        case INPUT_NAV_DOWN:
        case INPUT_NAV_LEFT:
        case INPUT_NAV_RIGHT:
        case INPUT_NAV_POPUP:
        case INPUT_NAV_MENU:
            input_ControlPush( p_input, i_query - INPUT_NAV_ACTIVATE
                               + INPUT_CONTROL_NAV_ACTIVATE, NULL );
            return VLC_SUCCESS;

        case INPUT_ADD_INFO:
        {
            char *psz_cat = va_arg( args, char * );
            char *psz_name = va_arg( args, char * );
            char *psz_format = va_arg( args, char * );

            char *psz_value;

            if( vasprintf( &psz_value, psz_format, args ) == -1 )
                return VLC_EGENERIC;

            int i_ret = input_item_AddInfo( priv->p_item, psz_cat, psz_name,
                                            "%s", psz_value );
            free( psz_value );

            if( !priv->b_preparsing && !i_ret )
                input_SendEventMetaInfo( p_input );
            return i_ret;
        }
        case INPUT_REPLACE_INFOS:
        case INPUT_MERGE_INFOS:
        {
            info_category_t *p_cat = va_arg( args, info_category_t * );

            if( i_query == INPUT_REPLACE_INFOS )
                input_item_ReplaceInfos( priv->p_item, p_cat );
            else
                input_item_MergeInfos( priv->p_item, p_cat );

            if( !priv->b_preparsing )
                input_SendEventMetaInfo( p_input );
            return VLC_SUCCESS;
        }
        case INPUT_DEL_INFO:
        {
            char *psz_cat = va_arg( args, char * );
            char *psz_name = va_arg( args, char * );

            int i_ret = input_item_DelInfo( priv->p_item, psz_cat, psz_name );

            if( !priv->b_preparsing && !i_ret )
                input_SendEventMetaInfo( p_input );
            return i_ret;
        }
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

            UpdateBookmarksOption( p_input );

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

            UpdateBookmarksOption( p_input );

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

                UpdateBookmarksOption( p_input );

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

            UpdateBookmarksOption( p_input );
            return VLC_SUCCESS;

        case INPUT_SET_BOOKMARK:
            i_bkmk = va_arg( args, int );

            val.i_int = i_bkmk;
            input_ControlPush( p_input, INPUT_CONTROL_SET_BOOKMARK, &val );

            return VLC_SUCCESS;

        case INPUT_GET_BOOKMARK:
            p_bkmk = va_arg( args, seekpoint_t * );

            vlc_mutex_lock( &priv->p_item->lock );
            *p_bkmk = priv->bookmark;
            vlc_mutex_unlock( &priv->p_item->lock );
            return VLC_SUCCESS;

        case INPUT_GET_TITLE_INFO:
        {
            input_title_t **p_title = va_arg( args, input_title_t ** );
            int *pi_req_title_offset = va_arg( args, int * );

            vlc_mutex_lock( &priv->p_item->lock );

            int i_current_title = var_GetInteger( p_input, "title" );
            if ( *pi_req_title_offset < 0 ) /* return current title if -1 */
                *pi_req_title_offset = i_current_title;

            if( priv->i_title && priv->i_title > *pi_req_title_offset )
            {
                *p_title = vlc_input_title_Duplicate( priv->title[*pi_req_title_offset] );
                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_SUCCESS;
            }
            else
            {
                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_EGENERIC;
            }
        }

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

        case INPUT_GET_SEEKPOINTS:
        {
            seekpoint_t ***array = va_arg( args, seekpoint_t *** );
            int *pi_title_to_fetch = va_arg( args, int * );

            vlc_mutex_lock( &priv->p_item->lock );

            if ( *pi_title_to_fetch < 0 ) /* query current title if -1 */
                *pi_title_to_fetch = var_GetInteger( p_input, "title" );

            if( priv->i_title == 0 || priv->i_title <= *pi_title_to_fetch )
            {
                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_EGENERIC;
            }

            const input_title_t *p_title = priv->title[*pi_title_to_fetch];

            /* set arg2 to the number of seekpoints we found */
            const int i_chapters = p_title->i_seekpoint;
            *pi_title_to_fetch = i_chapters;

            if ( i_chapters == 0 )
            {
                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_SUCCESS;
            }

            *array = calloc( p_title->i_seekpoint, sizeof(**array) );
            if( unlikely(array == NULL) )
            {
                vlc_mutex_unlock( &priv->p_item->lock );
                return VLC_ENOMEM;
            }
            for( int i = 0; i < i_chapters; i++ )
            {
                (*array)[i] = vlc_seekpoint_Duplicate( p_title->seekpoint[i] );
            }

            vlc_mutex_unlock( &priv->p_item->lock );

            return VLC_SUCCESS;
        }

        case INPUT_ADD_SLAVE:
        {
            enum slave_type type =  (enum slave_type) va_arg( args, enum slave_type );
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
            input_ControlPush( p_input, INPUT_CONTROL_ADD_SLAVE, &val );
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

        case INPUT_GET_ATTACHMENTS: /* arg1=input_attachment_t***, arg2=int*  res=can fail */
        {
            input_attachment_t ***ppp_attachment = va_arg( args, input_attachment_t *** );
            int *pi_attachment = va_arg( args, int * );

            vlc_mutex_lock( &priv->p_item->lock );
            if( priv->i_attachment <= 0 )
            {
                vlc_mutex_unlock( &priv->p_item->lock );
                *ppp_attachment = NULL;
                *pi_attachment = 0;
                return VLC_EGENERIC;
            }
            *pi_attachment = priv->i_attachment;
            *ppp_attachment = vlc_alloc( priv->i_attachment, sizeof(input_attachment_t*));
            for( int i = 0; i < priv->i_attachment; i++ )
                (*ppp_attachment)[i] = vlc_input_attachment_Duplicate( priv->attachment[i] );

            vlc_mutex_unlock( &priv->p_item->lock );
            return VLC_SUCCESS;
        }

        case INPUT_GET_ATTACHMENT:  /* arg1=input_attachment_t**, arg2=char*  res=can fail */
        {
            input_attachment_t **pp_attachment = va_arg( args, input_attachment_t ** );
            const char *psz_name = va_arg( args, const char * );

            vlc_mutex_lock( &priv->p_item->lock );
            for( int i = 0; i < priv->i_attachment; i++ )
            {
                if( !strcmp( priv->attachment[i]->psz_name, psz_name ) )
                {
                    *pp_attachment = vlc_input_attachment_Duplicate(priv->attachment[i] );
                    vlc_mutex_unlock( &priv->p_item->lock );
                    return VLC_SUCCESS;
                }
            }
            *pp_attachment = NULL;
            vlc_mutex_unlock( &priv->p_item->lock );
            return VLC_EGENERIC;
        }

        case INPUT_SET_RECORD_STATE:
            b_bool = va_arg( args, int );
            var_SetBool( p_input, "record", b_bool );
            return VLC_SUCCESS;

        case INPUT_GET_RECORD_STATE:
            pb_bool = va_arg( args, bool* );
            *pb_bool = var_GetBool( p_input, "record" );
            return VLC_SUCCESS;

        case INPUT_RESTART_ES:
            val.i_int = va_arg( args, int );
            input_ControlPush( p_input, INPUT_CONTROL_RESTART_ES, &val );
            return VLC_SUCCESS;

        case INPUT_UPDATE_VIEWPOINT:
        case INPUT_SET_INITIAL_VIEWPOINT:
        {
            vlc_viewpoint_t *p_viewpoint = malloc( sizeof(*p_viewpoint) );
            if( unlikely(p_viewpoint == NULL) )
                return VLC_ENOMEM;
            val.p_address = p_viewpoint;
            *p_viewpoint = *va_arg( args, const vlc_viewpoint_t* );
            if ( i_query == INPUT_SET_INITIAL_VIEWPOINT )
                input_ControlPush( p_input, INPUT_CONTROL_SET_INITIAL_VIEWPOINT,
                                   &val );
            else if ( va_arg( args, int ) )
                input_ControlPush( p_input, INPUT_CONTROL_SET_VIEWPOINT, &val );
            else
                input_ControlPush( p_input, INPUT_CONTROL_UPDATE_VIEWPOINT, &val );
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
            if( *pi_vout <= 0 )
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
            mtime_t *pi_system = va_arg( args, mtime_t * );
            mtime_t *pi_delay  = va_arg( args, mtime_t * );
            return es_out_ControlGetPcrSystem( priv->p_es_out_display, pi_system, pi_delay );
        }

        case INPUT_MODIFY_PCR_SYSTEM:
        {
            bool b_absolute = va_arg( args, int );
            mtime_t i_system = va_arg( args, mtime_t );
            return es_out_ControlModifyPcrSystem( priv->p_es_out_display, b_absolute, i_system );
        }

        case INPUT_SET_RENDERER:
        {
            vlc_renderer_item_t* p_item = va_arg( args, vlc_renderer_item_t* );
            val.p_address = p_item ? vlc_renderer_item_hold( p_item ) : NULL;
            input_ControlPush( p_input, INPUT_CONTROL_SET_RENDERER, &val );
            return VLC_SUCCESS;
        }

        default:
            msg_Err( p_input, "unknown query 0x%x in %s", i_query, __func__ );
            return VLC_EGENERIC;
    }
}

static void UpdateBookmarksOption( input_thread_t *p_input )
{
    input_thread_private_t *priv = input_priv(p_input);
    input_item_t* item = priv->p_item;
    struct vlc_memstream vstr;

    vlc_memstream_open( &vstr );
    vlc_memstream_puts( &vstr, "bookmarks=" );

    vlc_mutex_lock( &item->lock );
    var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES, 0, 0 );

    for( int i = 0; i < priv->i_bookmark; i++ )
    {
        seekpoint_t const* sp = priv->pp_bookmark[i];

        /* Add bookmark to choice-list */
        var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                    &(vlc_value_t){ .i_int = i },
                    &(vlc_value_t){ .psz_string = sp->psz_name } );

        /* Append bookmark to option-buffer */
        /* TODO: escape inappropriate values */
        vlc_memstream_printf( &vstr, "%s{name=%s,time=%.3f}",
            i > 0 ? "," : "", sp->psz_name, ( 1. * sp->i_time_offset ) / CLOCK_FREQ );
    }

    if( vlc_memstream_close( &vstr ) )
    {
        vlc_mutex_unlock( &item->lock );
        return;
    }

    /* XXX: The below is ugly and should be fixed elsewhere, but in order to
     * not have more than one "bookmarks=" option associated with the item, we
     * need to remove any existing ones before adding the new one. This logic
     * should exist in input_item_AddOption with "OPTION_UNIQUE & <an overwrite
     * flag>, but until then we handle it here. */

    char** const orig_beg = &item->ppsz_options[0];
    char** const orig_end = orig_beg + item->i_options;
    char** end = orig_end;

    for( char** option = orig_beg; option != end; )
    {
        if( strncmp( *option, "bookmarks=", 10 ) )
            ++option;
        else
        {
            free( *option );
            /* It might be tempting to optimize the below by overwriting
             * *option with the value of the last element, however; we want to
             * preserve the order of the other options (as behavior might
             * depend on it) */
            memmove( option, option + 1, ( --end - option ) * sizeof *end );
        }
    }

    if( end != orig_end ) /* we removed at least 1 option */
    {
        *end = vstr.ptr;
        item->i_options = end - orig_beg + 1;
        vlc_mutex_unlock( &item->lock );
    }
    else /* nothing removed, add the usual way */
    {
        vlc_mutex_unlock( &item->lock );
        input_item_AddOption( item, vstr.ptr, VLC_INPUT_OPTION_UNIQUE );
        free( vstr.ptr );
    }

    input_SendEventBookmark( p_input );
}

