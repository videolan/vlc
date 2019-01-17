/*****************************************************************************
 * omxil.c: Video decoder module making use of OpenMAX IL components.
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dlfcn.h>
#define dll_open(name) dlopen( name, RTLD_NOW )
#define dll_close(handle) dlclose(handle)

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_cpu.h>

#include "omxil.h"
#include "omxil_core.h"

/*****************************************************************************
 * List of OpenMAX IL core we will try in order
 *****************************************************************************/
static const char *ppsz_dll_list[] =
{
#if defined(RPI_OMX)
    "/opt/vc/lib/libopenmaxil.so",  /* Broadcom IL core */
#elif 1
    "libOMX_Core.so", /* TI OMAP IL core */
    "libOmxCore.so", /* Qualcomm IL core */
    "libnvomx.so", /* Tegra3 IL core */
#else
    "libomxil-bellagio.so",  /* Bellagio IL core reference implementation */
#endif
    0
};

#ifdef RPI_OMX
static const char *ppsz_extra_dll_list[] =
{
    "/opt/vc/lib/libbcm_host.so",  /* Broadcom host library */
    0
};
#endif

/*****************************************************************************
 * Global OMX Core instance, shared between module instances
 *****************************************************************************/
static vlc_mutex_t omx_core_mutex = VLC_STATIC_MUTEX;
static unsigned int omx_refcount = 0;
static void *dll_handle;
OMX_ERRORTYPE (*pf_init) (void);
OMX_ERRORTYPE (*pf_deinit) (void);
OMX_ERRORTYPE (*pf_get_handle) (OMX_HANDLETYPE *, OMX_STRING,
                                OMX_PTR, OMX_CALLBACKTYPE *);
OMX_ERRORTYPE (*pf_free_handle) (OMX_HANDLETYPE);
OMX_ERRORTYPE (*pf_component_enum)(OMX_STRING, OMX_U32, OMX_U32);
OMX_ERRORTYPE (*pf_get_roles_of_component)(OMX_STRING, OMX_U32 *, OMX_U8 **);

#ifdef RPI_OMX
static void *extra_dll_handle;
static void (*pf_host_init)(void);
static void (*pf_host_deinit)(void);

static void CloseExtraDll()
{
    if (pf_host_deinit)
        pf_host_deinit();
    // Intentionally not unloading the host library, since it cannot be
    // unloaded cleanly after it has been initialized.
}
#else
#define CloseExtraDll()
#endif

int InitOmxCore(vlc_object_t *p_this)
{
    int i;
    vlc_mutex_lock( &omx_core_mutex );
    if( omx_refcount > 0 ) {
        omx_refcount++;
        vlc_mutex_unlock( &omx_core_mutex );
        return VLC_SUCCESS;
    }

#ifdef RPI_OMX
    /* Load an extra library first, if available */
    extra_dll_handle = NULL;
    for( i = 0; ppsz_extra_dll_list[i]; i++ )
    {
        extra_dll_handle = dll_open( ppsz_extra_dll_list[i] );
        if( extra_dll_handle ) break;
    }
    if( extra_dll_handle )
    {
        pf_host_init = dlsym( extra_dll_handle, "bcm_host_init" );
        pf_host_deinit = dlsym( extra_dll_handle, "bcm_host_deinit" );
        if (pf_host_init)
            pf_host_init();
    }
#endif

    /* Load the OMX core */
    for( i = 0; ppsz_dll_list[i]; i++ )
    {
        dll_handle = dll_open( ppsz_dll_list[i] );
        if( dll_handle ) break;
    }
    if( !dll_handle )
    {
        CloseExtraDll();
        vlc_mutex_unlock( &omx_core_mutex );
        return VLC_EGENERIC;
    }

    pf_init = dlsym( dll_handle, "OMX_Init" );
    pf_deinit = dlsym( dll_handle, "OMX_Deinit" );
    pf_get_handle = dlsym( dll_handle, "OMX_GetHandle" );
    pf_free_handle = dlsym( dll_handle, "OMX_FreeHandle" );
    pf_component_enum = dlsym( dll_handle, "OMX_ComponentNameEnum" );
    pf_get_roles_of_component = dlsym( dll_handle, "OMX_GetRolesOfComponent" );
    if( !pf_init || !pf_deinit || !pf_get_handle || !pf_free_handle ||
        !pf_component_enum || !pf_get_roles_of_component )
    {
        msg_Warn( p_this, "cannot find OMX_* symbols in `%s' (%s)",
                  ppsz_dll_list[i], dlerror() );
        dll_close(dll_handle);
        CloseExtraDll();
        vlc_mutex_unlock( &omx_core_mutex );
        return VLC_EGENERIC;
    }

    /* Initialise the OMX core */
    OMX_ERRORTYPE omx_error = pf_init();
    if(omx_error != OMX_ErrorNone)
    {
        msg_Warn( p_this, "OMX_Init failed (%x: %s)", omx_error,
                  ErrorToString(omx_error) );
        dll_close(dll_handle);
        CloseExtraDll();
        vlc_mutex_unlock( &omx_core_mutex );
        return VLC_EGENERIC;
    }
    omx_refcount++;
    vlc_mutex_unlock( &omx_core_mutex );
    return VLC_SUCCESS;
}

void DeinitOmxCore(void)
{
    vlc_mutex_lock( &omx_core_mutex );
    omx_refcount--;
    if( omx_refcount == 0 )
    {
        pf_deinit();
        dll_close( dll_handle );
        CloseExtraDll();
    }
    vlc_mutex_unlock( &omx_core_mutex );
}

/*****************************************************************************
 * CreateComponentsList: creates a list of components matching the given role
 *****************************************************************************/

static const struct
{
    const char *psz_role;
    const char *psz_name;
} role_mappings[] =
{
#ifdef RPI_OMX
    { "video_decoder.avc", "OMX.broadcom.video_decode" },
    { "video_decoder.mpeg2", "OMX.broadcom.video_decode" },
    { "iv_renderer", "OMX.broadcom.video_render" },
#endif
    { 0, 0 }
};

int CreateComponentsList(vlc_object_t *p_this, const char *psz_role,
                         char ppsz_components[MAX_COMPONENTS_LIST_SIZE][OMX_MAX_STRINGNAME_SIZE])
{
    char psz_name[OMX_MAX_STRINGNAME_SIZE];
    OMX_ERRORTYPE omx_error;
    OMX_U32 roles = 0;
    OMX_U8 **ppsz_roles = 0;
    unsigned int i, j, components = 0;

    if(!psz_role) goto end;

    for( i = 0; ; i++ )
    {
        bool b_found = false;

        omx_error = pf_component_enum(psz_name, OMX_MAX_STRINGNAME_SIZE, i);
        if(omx_error != OMX_ErrorNone) break;

        msg_Dbg(p_this, "component %s", psz_name);

        for( unsigned int j = 0; role_mappings[j].psz_role; j++ ) {
            if( !strcmp( psz_role, role_mappings[j].psz_role ) &&
                !strcmp( psz_name, role_mappings[j].psz_name ) ) {
                goto found;
            }
        }

        omx_error = pf_get_roles_of_component(psz_name, &roles, 0);
        if(omx_error != OMX_ErrorNone || !roles) continue;

        ppsz_roles = malloc(roles * (sizeof(OMX_U8*) + OMX_MAX_STRINGNAME_SIZE));
        if(!ppsz_roles) continue;

        for( j = 0; j < roles; j++ )
            ppsz_roles[j] = ((OMX_U8 *)(&ppsz_roles[roles])) +
                j * OMX_MAX_STRINGNAME_SIZE;

        omx_error = pf_get_roles_of_component(psz_name, &roles, ppsz_roles);
        if(omx_error != OMX_ErrorNone) roles = 0;

        for(j = 0; j < roles; j++)
        {
            msg_Dbg(p_this, "  - role: %s", ppsz_roles[j]);
            if(!strcmp((char *)ppsz_roles[j], psz_role)) b_found = true;
        }

        free(ppsz_roles);

        if(!b_found) continue;

found:
        if(components >= MAX_COMPONENTS_LIST_SIZE)
        {
            msg_Dbg(p_this, "too many matching components");
            continue;
        }

        strncpy(ppsz_components[components], psz_name,
                OMX_MAX_STRINGNAME_SIZE-1);
        components++;
    }

 end:
    msg_Dbg(p_this, "found %i matching components for role %s",
            components, psz_role);
    for( i = 0; i < components; i++ )
        msg_Dbg(p_this, "- %s", ppsz_components[i]);

    return components;
}

