// SPDX-License-Identifier: LGPL-2.1-or-later

// amf_helper.h: AMD Advanced Media Framework helper
// Copyright © 2024 VideoLabs, VLC authors and VideoLAN

// Authors: Steve Lhomme <robux4@videolabs.io>

#ifndef VLC_AMF_HELPER_H
#define VLC_AMF_HELPER_H

#include <AMF/core/Context.h>
#include <AMF/core/Factory.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define AMT_TYPE(t)  amf::t
#else
#define AMT_TYPE(t)  t
#endif

struct vlc_amf_context
{
    AMT_TYPE(AMFFactory) *pFactory;
    AMT_TYPE(AMFContext) *Context;
    void            *Private;
};

int vlc_AMFCreateContext(struct vlc_amf_context *);
void vlc_AMFReleaseContext(struct vlc_amf_context *);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VLC_AMF_HELPER_H
