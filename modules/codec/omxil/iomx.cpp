/*****************************************************************************
 * iomx.cpp: OpenMAX interface implementation based on IOMX
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Authors: Martin Storsjo <martin@martin.st>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <media/stagefright/OMXClient.h>
#include <media/IOMX.h>
#include <binder/MemoryDealer.h>
#include <OMX_Component.h>

#include "iomx.h"

using namespace android;

class IOMXContext {
public:
    IOMXContext() {
    }

    sp<IOMX> iomx;
    List<IOMX::ComponentInfo> components;
};

static IOMXContext *ctx;

class OMXNode;

class OMXCodecObserver : public BnOMXObserver {
public:
    OMXCodecObserver() {
        node = NULL;
    }
    void setNode(OMXNode* n) {
        node = n;
    }
    void onMessage(const omx_message &msg);
    void registerBuffers(const sp<IMemoryHeap> &) {
    }
private:
    OMXNode *node;
};

class OMXNode {
public:
    IOMX::node_id node;
    sp<OMXCodecObserver> observer;
    OMX_CALLBACKTYPE callbacks;
    OMX_PTR app_data;
    OMX_STATETYPE state;
    List<OMX_BUFFERHEADERTYPE*> buffers;
    OMX_HANDLETYPE handle;
    String8 component_name;
};

class OMXBuffer {
public:
    sp<MemoryDealer> dealer;
    IOMX::buffer_id id;
};

void OMXCodecObserver::onMessage(const omx_message &msg)
{
    if (!node)
        return;
    switch (msg.type) {
    case omx_message::EVENT:
        // TODO: Needs locking
        if (msg.u.event_data.event == OMX_EventCmdComplete && msg.u.event_data.data1 == OMX_CommandStateSet)
            node->state = (OMX_STATETYPE) msg.u.event_data.data2;
        node->callbacks.EventHandler(node->handle, node->app_data, msg.u.event_data.event, msg.u.event_data.data1, msg.u.event_data.data2, NULL);
        break;
    case omx_message::EMPTY_BUFFER_DONE:
        for( List<OMX_BUFFERHEADERTYPE*>::iterator it = node->buffers.begin(); it != node->buffers.end(); it++ ) {
            OMXBuffer* info = (OMXBuffer*) (*it)->pPlatformPrivate;
            if (msg.u.buffer_data.buffer == info->id) {
                node->callbacks.EmptyBufferDone(node->handle, node->app_data, *it);
                break;
            }
        }
        break;
    case omx_message::FILL_BUFFER_DONE:
        for( List<OMX_BUFFERHEADERTYPE*>::iterator it = node->buffers.begin(); it != node->buffers.end(); it++ ) {
            OMXBuffer* info = (OMXBuffer*) (*it)->pPlatformPrivate;
            if (msg.u.extended_buffer_data.buffer == info->id) {
                OMX_BUFFERHEADERTYPE *buffer = *it;
                buffer->nOffset = msg.u.extended_buffer_data.range_offset;
                buffer->nFilledLen = msg.u.extended_buffer_data.range_length;
                buffer->nFlags = msg.u.extended_buffer_data.flags;
                buffer->nTimeStamp = msg.u.extended_buffer_data.timestamp;
                node->callbacks.FillBufferDone(node->handle, node->app_data, buffer);
                break;
            }
        }
        break;
    default:
        break;
    }
}

static OMX_ERRORTYPE get_error(status_t err)
{
    if (err == OK)
        return OMX_ErrorNone;
    return OMX_ErrorUndefined;
}

static int get_param_size(OMX_INDEXTYPE param_index)
{
    switch (param_index) {
    case OMX_IndexParamPortDefinition:
        return sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    case OMX_IndexParamStandardComponentRole:
        return sizeof(OMX_PARAM_COMPONENTROLETYPE);
    case OMX_IndexParamVideoInit:
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit:
        return sizeof(OMX_PORT_PARAM_TYPE);
    case OMX_IndexParamNumAvailableStreams:
        return sizeof(OMX_PARAM_U32TYPE);
    case OMX_IndexParamAudioPcm:
        return sizeof(OMX_AUDIO_PARAM_PCMMODETYPE);
    case OMX_IndexParamAudioAdpcm:
        return sizeof(OMX_AUDIO_PARAM_AMRTYPE);
    case OMX_IndexParamAudioAmr:
        return sizeof(OMX_AUDIO_PARAM_AMRTYPE);
    case OMX_IndexParamAudioG723:
        return sizeof(OMX_AUDIO_PARAM_G723TYPE);
    case OMX_IndexParamAudioG726:
        return sizeof(OMX_AUDIO_PARAM_G726TYPE);
    case OMX_IndexParamAudioG729:
        return sizeof(OMX_AUDIO_PARAM_G729TYPE);
    case OMX_IndexParamAudioAac:
        return sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE);
    case OMX_IndexParamAudioMp3:
        return sizeof(OMX_AUDIO_PARAM_MP3TYPE);
    case OMX_IndexParamAudioSbc:
        return sizeof(OMX_AUDIO_PARAM_SBCTYPE);
    case OMX_IndexParamAudioVorbis:
        return sizeof(OMX_AUDIO_PARAM_VORBISTYPE);
    case OMX_IndexParamAudioWma:
        return sizeof(OMX_AUDIO_PARAM_WMATYPE);
    case OMX_IndexParamAudioRa:
        return sizeof(OMX_AUDIO_PARAM_RATYPE);
    default:
        return 0;
    }
}

static OMX_ERRORTYPE iomx_send_command(OMX_HANDLETYPE component, OMX_COMMANDTYPE command, OMX_U32 param1, OMX_PTR)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    return get_error(ctx->iomx->sendCommand(node->node, command, param1));
}

static OMX_ERRORTYPE iomx_get_parameter(OMX_HANDLETYPE component, OMX_INDEXTYPE param_index, OMX_PTR param)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    return get_error(ctx->iomx->getParameter(node->node, param_index, param, get_param_size(param_index)));
}

static OMX_ERRORTYPE iomx_set_parameter(OMX_HANDLETYPE component, OMX_INDEXTYPE param_index, OMX_PTR param)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    return get_error(ctx->iomx->setParameter(node->node, param_index, param, get_param_size(param_index)));
}

static OMX_ERRORTYPE iomx_get_state(OMX_HANDLETYPE component, OMX_STATETYPE *ptr) {
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    *ptr = node->state;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE iomx_allocate_buffer(OMX_HANDLETYPE component, OMX_BUFFERHEADERTYPE **bufferptr, OMX_U32 port_index, OMX_PTR app_private, OMX_U32 size)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    OMXBuffer* info = new OMXBuffer;
    info->dealer = new MemoryDealer(size + 4096); // Do we need to keep this around, or is it kept alive via the IMemory that references it?
    sp<IMemory> mem = info->dealer->allocate(size);
    int ret = ctx->iomx->allocateBufferWithBackup(node->node, port_index, mem, &info->id);
    if (ret != OK)
        return OMX_ErrorUndefined;
    OMX_BUFFERHEADERTYPE *buffer = (OMX_BUFFERHEADERTYPE*) calloc(1, sizeof(OMX_BUFFERHEADERTYPE));
    *bufferptr = buffer;
    buffer->pPlatformPrivate = info;
    buffer->pAppPrivate = app_private;
    buffer->nAllocLen = size;
    buffer->pBuffer = (OMX_U8*) mem->pointer();
    node->buffers.push_back(buffer);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE iomx_free_buffer(OMX_HANDLETYPE component, OMX_U32 port, OMX_BUFFERHEADERTYPE *buffer)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    OMXBuffer* info = (OMXBuffer*) buffer->pPlatformPrivate;
    status_t ret = ctx->iomx->freeBuffer(node->node, port, info->id);
    for( List<OMX_BUFFERHEADERTYPE*>::iterator it = node->buffers.begin(); it != node->buffers.end(); it++ ) {
        if (buffer == *it) {
            node->buffers.erase(it);
            break;
        }
    }
    free(buffer);
    delete info;
    return get_error(ret);
}

static OMX_ERRORTYPE iomx_empty_this_buffer(OMX_HANDLETYPE component, OMX_BUFFERHEADERTYPE *buffer)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    OMXBuffer* info = (OMXBuffer*) buffer->pPlatformPrivate;
    return get_error(ctx->iomx->emptyBuffer(node->node, info->id, buffer->nOffset, buffer->nFilledLen, buffer->nFlags, buffer->nTimeStamp));
}

static OMX_ERRORTYPE iomx_fill_this_buffer(OMX_HANDLETYPE component, OMX_BUFFERHEADERTYPE *buffer)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    OMXBuffer* info = (OMXBuffer*) buffer->pPlatformPrivate;
    return get_error(ctx->iomx->fillBuffer(node->node, info->id));
}

static OMX_ERRORTYPE iomx_component_role_enum(OMX_HANDLETYPE component, OMX_U8 *role, OMX_U32 index)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    for( List<IOMX::ComponentInfo>::iterator it = ctx->components.begin(); it != ctx->components.end(); it++ ) {
        if (node->component_name == it->mName) {
            if (index >= it->mRoles.size())
                return OMX_ErrorNoMore;
            List<String8>::iterator it2 = it->mRoles.begin();
            for( OMX_U32 i = 0; it2 != it->mRoles.end() && i < index; i++, it2++ ) ;
            strncpy((char*)role, it2->string(), OMX_MAX_STRINGNAME_SIZE);
            if (it2->length() >= OMX_MAX_STRINGNAME_SIZE)
                role[OMX_MAX_STRINGNAME_SIZE - 1] = '\0';
            return OMX_ErrorNone;
        }
    }
    return OMX_ErrorInvalidComponentName;
}

static OMX_ERRORTYPE iomx_get_extension_index(OMX_HANDLETYPE component, OMX_STRING parameter, OMX_INDEXTYPE *index)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    return get_error(ctx->iomx->getExtensionIndex(node->node, parameter, index));
}

static OMX_ERRORTYPE iomx_set_config(OMX_HANDLETYPE component, OMX_INDEXTYPE index, OMX_PTR param)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    /* TODO: Need a way to map index to param size */
    return get_error(ctx->iomx->setConfig(node->node, index, param, sizeof(OMX_BOOL)));
}

static OMX_ERRORTYPE iomx_get_handle(OMX_HANDLETYPE *handle_ptr, const char *component_name, OMX_PTR app_data, const OMX_CALLBACKTYPE *callbacks)
{
    OMXNode* node = new OMXNode();
    node->app_data = app_data;
    node->callbacks = *callbacks;
    node->observer = new OMXCodecObserver();
    node->observer->setNode(node);
    node->state = OMX_StateLoaded;
    node->component_name = component_name;

    OMX_COMPONENTTYPE* component = (OMX_COMPONENTTYPE*) malloc(sizeof(OMX_COMPONENTTYPE));
    memset(component, 0, sizeof(OMX_COMPONENTTYPE));
    component->nSize = sizeof(OMX_COMPONENTTYPE);
    component->nVersion.s.nVersionMajor = 1;
    component->nVersion.s.nVersionMinor = 1;
    component->nVersion.s.nRevision = 0;
    component->nVersion.s.nStep = 0;
    component->pComponentPrivate = node;
    component->SendCommand = iomx_send_command;
    component->GetParameter = iomx_get_parameter;
    component->SetParameter = iomx_set_parameter;
    component->FreeBuffer = iomx_free_buffer;
    component->EmptyThisBuffer = iomx_empty_this_buffer;
    component->FillThisBuffer = iomx_fill_this_buffer;
    component->GetState = iomx_get_state;
    component->AllocateBuffer = iomx_allocate_buffer;
    component->ComponentRoleEnum = iomx_component_role_enum;
    component->GetExtensionIndex = iomx_get_extension_index;
    component->SetConfig = iomx_set_config;

    *handle_ptr = component;
    node->handle = component;
    status_t ret;
    if ((ret = ctx->iomx->allocateNode( component_name, node->observer, &node->node )) != OK)
        return OMX_ErrorUndefined;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE iomx_free_handle(OMX_HANDLETYPE handle)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)handle)->pComponentPrivate;
    ctx->iomx->freeNode( node->node );
    node->observer->setNode(NULL);
    delete node;
    free(handle);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE iomx_init()
{
    OMXClient client;
    if (client.connect() != OK)
        return OMX_ErrorUndefined;

    ctx->iomx = client.interface();
    ctx->iomx->listNodes(&ctx->components);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE iomx_deinit()
{
    ctx->iomx = NULL;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE iomx_component_name_enum(OMX_STRING component_name, OMX_U32 name_length, OMX_U32 index)
{
    if (index >= ctx->components.size())
        return OMX_ErrorNoMore;
    List<IOMX::ComponentInfo>::iterator it = ctx->components.begin();
    for( OMX_U32 i = 0; i < index; i++ )
        it++;
    strncpy(component_name, it->mName.string(), name_length);
    component_name[name_length - 1] = '\0';
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE iomx_get_roles_of_component(OMX_STRING component_name, OMX_U32 *num_roles, OMX_U8 **roles)
{
    for( List<IOMX::ComponentInfo>::iterator it = ctx->components.begin(); it != ctx->components.end(); it++ ) {
        if (!strcmp(component_name, it->mName.string())) {
            if (!roles) {
                *num_roles = it->mRoles.size();
                return OMX_ErrorNone;
            }
            if (*num_roles < it->mRoles.size())
                return OMX_ErrorInsufficientResources;
            *num_roles = it->mRoles.size();
            OMX_U32 i = 0;
            for( List<String8>::iterator it2 = it->mRoles.begin(); it2 != it->mRoles.end(); i++, it2++ ) {
                strncpy((char*)roles[i], it2->string(), OMX_MAX_STRINGNAME_SIZE);
                roles[i][OMX_MAX_STRINGNAME_SIZE - 1] = '\0';
            }
            return OMX_ErrorNone;
        }
    }
    return OMX_ErrorInvalidComponentName;
}

void* iomx_dlopen(const char *)
{
    if (!ctx)
        ctx = new IOMXContext();
    return ctx;
}

void iomx_dlclose(void *handle)
{
    IOMXContext *ctx = (IOMXContext*) handle;
    delete ctx;
    ::ctx = NULL;
}

void *iomx_dlsym(void *, const char *name)
{
    if (!strcmp(name, "OMX_Init"))
        return (void*) iomx_init;
    if (!strcmp(name, "OMX_Deinit"))
        return (void*) iomx_deinit;
    if (!strcmp(name, "OMX_GetHandle"))
        return (void*) iomx_get_handle;
    if (!strcmp(name, "OMX_FreeHandle"))
        return (void*) iomx_free_handle;
    if (!strcmp(name, "OMX_ComponentNameEnum"))
        return (void*) iomx_component_name_enum;
    if (!strcmp(name, "OMX_GetRolesOfComponent"))
        return (void*) iomx_get_roles_of_component;
    return NULL;
}

