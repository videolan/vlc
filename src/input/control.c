/*****************************************************************************
 * control.c
 *****************************************************************************
 * Copyright (C) 1999-2015 VLC authors and VideoLAN
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

        default:
            msg_Err( p_input, "unknown query 0x%x in %s", i_query, __func__ );
            return VLC_EGENERIC;
    }
}
