// SPDX-License-Identifier: LGPL-2.1-or-later

// amf_helper.c: AMD Advanced Media Framework helper
// Copyright © 2024 VideoLabs, VLC authors and VideoLAN

// Authors: Steve Lhomme <robux4@videolabs.io>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "amf_helper.h"

#include <vlc_common.h>

int vlc_AMFCreateContext(struct vlc_amf_context *c)
{
#ifdef _WIN32
# if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HMODULE hLib = LoadLibraryA(AMF_DLL_NAMEA);
    if (hLib == NULL)
        return (-ENOTSUP);

    amf_uint64 version = 0;
    AMF_RESULT res;
    AMFQueryVersion_Fn queryVersion = (AMFQueryVersion_Fn)GetProcAddress(hLib, AMF_QUERY_VERSION_FUNCTION_NAME);
    if (unlikely(queryVersion == NULL))
        goto error;
    res = queryVersion(&version);
    if (unlikely(res != AMF_OK))
        goto error;

    c->pFactory = NULL;
    c->Context = NULL;

    AMFInit_Fn init = (AMFInit_Fn)GetProcAddress(hLib, AMF_INIT_FUNCTION_NAME);
    res = init(version, &c->pFactory);
    if (unlikely(res != AMF_OK))
        goto error;

    res = c->pFactory->pVtbl->CreateContext(c->pFactory, &c->Context);
    if (res != AMF_OK || c->Context == NULL)
        goto error;

    c->Private = hLib;
    return VLC_SUCCESS;

error:
    FreeLibrary(hLib);
    return (-ENOTSUP);

# else // !WINAPI_PARTITION_DESKTOP
    // we can't load external DLLs in UWP
    return (-ENOTSUP);
# endif // !WINAPI_PARTITION_DESKTOP

#else
    return NULL; // TODO
#endif
}

void vlc_AMFReleaseContext(struct vlc_amf_context *c)
{
    c->Context->pVtbl->Terminate(c->Context);
    c->Context->pVtbl->Release(c->Context);
#ifdef _WIN32
    FreeLibrary(c->Private);
#endif
}
