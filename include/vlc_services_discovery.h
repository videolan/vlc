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

#include <vlc_input.h>
#include <vlc_events.h>
#include <vlc_probe.h>

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

struct services_discovery_t
{
    VLC_COMMON_MEMBERS
    module_t *          p_module;

    vlc_event_manager_t event_manager;      /* Accessed through Setters for non class function */

    char *psz_name;
    config_chain_t *p_cfg;

    int ( *pf_control ) ( services_discovery_t *, int, va_list );

    services_discovery_sys_t *p_sys;
};

/**
 * Service discovery categories
 */
enum services_discovery_category_e
{
    SD_CAT_DEVICES = 1,
    SD_CAT_LAN,
    SD_CAT_INTERNET,
    SD_CAT_MYCOMPUTER
};

/**
 * Service discovery control commands
 */
enum services_discovery_command_e
{
    SD_CMD_SEARCH = 1,          /**< arg1 = query */
    SD_CMD_DESCRIPTOR           /**< arg1 = services_discovery_descriptor_t* */
};

/**
 * Service discovery capabilities
 */
enum services_discovery_capability_e
{
    SD_CAP_SEARCH = 1
};

/**
 * Service discovery descriptor
 */
typedef struct
{
    char *psz_short_desc;
    char *psz_icon_url;
    char *psz_url;
    int   i_capabilities;
} services_discovery_descriptor_t;

/***********************************************************************
 * Service Discovery
 ***********************************************************************/

/**
 * Ask for a research in the SD
 * @param p_sd: the Service Discovery
 * @param i_control: the command to issue
 * @param args: the argument list
 * @return VLC_SUCCESS in case of success, the error code overwise
 */
static inline int vlc_sd_control( services_discovery_t *p_sd, int i_control, va_list args )
{
    if( p_sd->pf_control )
        return p_sd->pf_control( p_sd, i_control, args );
    else
        return VLC_EGENERIC;
}

/* Get the services discovery modules names to use in Create(), in a null
 * terminated string array. Array and string must be freed after use. */
VLC_EXPORT( char **, vlc_sd_GetNames, ( vlc_object_t *, char ***, int ** ) LIBVLC_USED );
#define vlc_sd_GetNames(obj, pln, pcat ) \
        vlc_sd_GetNames(VLC_OBJECT(obj), pln, pcat)

/* Creation of a services_discovery object */
VLC_EXPORT( services_discovery_t *, vlc_sd_Create, ( vlc_object_t *, const char * ) LIBVLC_USED );
VLC_EXPORT( bool, vlc_sd_Start, ( services_discovery_t * ) );
VLC_EXPORT( void, vlc_sd_Stop, ( services_discovery_t * ) );
VLC_EXPORT( void, vlc_sd_Destroy, ( services_discovery_t * ) );

static inline void vlc_sd_StopAndDestroy( services_discovery_t * p_this )
{
    vlc_sd_Stop( p_this );
    vlc_sd_Destroy( p_this );
}

/* Read info from discovery object */
VLC_EXPORT( char *,                 services_discovery_GetLocalizedName, ( services_discovery_t * p_this ) LIBVLC_USED );

/* Receive event notification (preferred way to get new items) */
VLC_EXPORT( vlc_event_manager_t *,  services_discovery_EventManager, ( services_discovery_t * p_this ) LIBVLC_USED );

/* Used by services_discovery to post update about their items */
    /* About the psz_category, it is a legacy way to add info to the item,
     * for more options, directly set the (meta) data on the input item */
VLC_EXPORT( void,                   services_discovery_AddItem, ( services_discovery_t * p_this, input_item_t * p_item, const char * psz_category ) );
VLC_EXPORT( void,                   services_discovery_RemoveItem, ( services_discovery_t * p_this, input_item_t * p_item ) );


/* SD probing */

VLC_EXPORT(int, vlc_sd_probe_Add, (vlc_probe_t *, const char *, const char *, int category));

#define VLC_SD_PROBE_SUBMODULE \
    add_submodule() \
        set_capability( "services probe", 100 ) \
        set_callbacks( vlc_sd_probe_Open, NULL )

#define VLC_SD_PROBE_HELPER(name, longname, cat) \
static int vlc_sd_probe_Open (vlc_object_t *obj) \
{ \
    return vlc_sd_probe_Add ((struct vlc_probe_t *)obj, \
                             name "{longname=\"" longname "\"}", \
                             longname, cat); \
}

/** @} */
# ifdef __cplusplus
}
# endif

#endif
