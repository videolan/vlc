/*****************************************************************************
 * vlm_event.c: Events
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vlm.h>
#include "vlm_internal.h"
#include "vlm_event.h"
#include <assert.h>

/* */
static void Trigger( vlm_t *, int i_type, int64_t id, const char *psz_name );
static void TriggerInstanceState( vlm_t *, int i_type, int64_t id, const char *psz_name, const char *psz_instance_name, input_state_e input_state );

/*****************************************************************************
 *
 *****************************************************************************/
void vlm_SendEventMediaAdded( vlm_t *p_vlm, int64_t id, const char *psz_name )
{
    Trigger( p_vlm, VLM_EVENT_MEDIA_ADDED, id, psz_name );
}
void vlm_SendEventMediaRemoved( vlm_t *p_vlm, int64_t id, const char *psz_name )
{
    Trigger( p_vlm, VLM_EVENT_MEDIA_REMOVED, id, psz_name );
}
void vlm_SendEventMediaChanged( vlm_t *p_vlm, int64_t id, const char *psz_name )
{
    Trigger( p_vlm, VLM_EVENT_MEDIA_CHANGED, id, psz_name );
}

void vlm_SendEventMediaInstanceStarted( vlm_t *p_vlm, int64_t id, const char *psz_name )
{
    Trigger( p_vlm, VLM_EVENT_MEDIA_INSTANCE_STARTED, id, psz_name );
}
void vlm_SendEventMediaInstanceStopped( vlm_t *p_vlm, int64_t id, const char *psz_name )
{
    Trigger( p_vlm, VLM_EVENT_MEDIA_INSTANCE_STOPPED, id, psz_name );
}

void vlm_SendEventMediaInstanceState( vlm_t *p_vlm, int64_t id, const char *psz_name, const char *psz_instance_name, input_state_e state )
{
    TriggerInstanceState( p_vlm, VLM_EVENT_MEDIA_INSTANCE_STATE, id, psz_name, psz_instance_name, state );
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Trigger( vlm_t *p_vlm, int i_type, int64_t id, const char *psz_name )
{
    vlm_event_t event;

    event.i_type = i_type;
    event.id = id;
    event.psz_name = psz_name;
    event.input_state = 0;
    event.psz_instance_name = NULL;
    var_SetAddress( p_vlm, "intf-event", &event );
}

static void TriggerInstanceState( vlm_t *p_vlm, int i_type, int64_t id, const char *psz_name, const char *psz_instance_name, input_state_e input_state )
{
    vlm_event_t event;

    event.i_type = i_type;
    event.id = id;
    event.psz_name = psz_name;
    event.input_state = input_state;
    event.psz_instance_name = psz_instance_name;
    var_SetAddress( p_vlm, "intf-event", &event );
}
