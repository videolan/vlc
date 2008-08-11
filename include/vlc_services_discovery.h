/*****************************************************************************
 * vlc_services_discovery.h : Services Discover functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#ifndef VLC_SERVICES_DISCOVERY_H_
#define VLC_SERVICES_DISCOVERY_H_

/**
 * \file
 * This file functions and structures for service discovery in vlc
 */

# ifdef __cplusplus
extern "C" {
# endif

/*
 * @{
 */

#include <vlc_input.h>
#include <vlc_events.h>

struct services_discovery_t
{
    VLC_COMMON_MEMBERS
    char *              psz_module;
    module_t *          p_module;

    char *              psz_localized_name; /* Accessed through Setters for non class function */
    vlc_event_manager_t event_manager;      /* Accessed through Setters for non class function */

    services_discovery_sys_t *p_sys;
    void (*pf_run) ( services_discovery_t *);
};


/***********************************************************************
 * Service Discovery
 ***********************************************************************/

/* Get the services discovery modules names to use in Create(), in a null
 * terminated string array. Array and string must be freed after use. */
VLC_EXPORT( char **, __services_discovery_GetServicesNames, ( vlc_object_t * p_super, char ***pppsz_longnames ) );
#define services_discovery_GetServicesNames(a,b) \
        __services_discovery_GetServicesNames(VLC_OBJECT(a),b)

/* Creation of a service_discovery object */
VLC_EXPORT( services_discovery_t *, services_discovery_Create, ( vlc_object_t * p_super, const char * psz_service_name ) );
VLC_EXPORT( void,                   services_discovery_Destroy, ( services_discovery_t * p_this ) );
VLC_EXPORT( int,                    services_discovery_Start, ( services_discovery_t * p_this ) );
VLC_EXPORT( void,                   services_discovery_Stop, ( services_discovery_t * p_this ) );

/* Read info from discovery object */
VLC_EXPORT( char *,                 services_discovery_GetLocalizedName, ( services_discovery_t * p_this ) );

/* Receive event notification (preferred way to get new items) */
VLC_EXPORT( vlc_event_manager_t *,  services_discovery_EventManager, ( services_discovery_t * p_this ) );

/* Used by services_discovery to post update about their items */
VLC_EXPORT( void,                   services_discovery_SetLocalizedName, ( services_discovery_t * p_this, const char * ) );
    /* About the psz_category, it is a legacy way to add info to the item,
     * for more options, directly set the (meta) data on the input item */
VLC_EXPORT( void,                   services_discovery_AddItem, ( services_discovery_t * p_this, input_item_t * p_item, const char * psz_category ) );
VLC_EXPORT( void,                   services_discovery_RemoveItem, ( services_discovery_t * p_this, input_item_t * p_item ) );

/** @} */
# ifdef __cplusplus
}
# endif

#endif
