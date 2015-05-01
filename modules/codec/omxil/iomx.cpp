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

#include <media/stagefright/OMXClient.h>
#include <media/IOMX.h>
#include <binder/MemoryDealer.h>
#include <OMX_Component.h>

#define PREFIX(x) I ## x

#if ANDROID_API >= 11
#define HAS_USE_BUFFER
#endif

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
#ifdef HAS_USE_BUFFER
    sp<GraphicBuffer> graphicBuffer;
#endif
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
        for( List<OMX_BUFFERHEADERTYPE*>::iterator it = node->buffers.begin(); it != node->buffers.end(); ++it ) {
            OMXBuffer* info = (OMXBuffer*) (*it)->pPlatformPrivate;
            if (msg.u.buffer_data.buffer == info->id) {
                node->callbacks.EmptyBufferDone(node->handle, node->app_data, *it);
                break;
            }
        }
        break;
    case omx_message::FILL_BUFFER_DONE:
        for( List<OMX_BUFFERHEADERTYPE*>::iterator it = node->buffers.begin(); it != node->buffers.end(); ++it ) {
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

static OMX_ERRORTYPE iomx_send_command(OMX_HANDLETYPE component, OMX_COMMANDTYPE command, OMX_U32 param1, OMX_PTR)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    return get_error(ctx->iomx->sendCommand(node->node, command, param1));
}

static OMX_ERRORTYPE iomx_get_parameter(OMX_HANDLETYPE component, OMX_INDEXTYPE param_index, OMX_PTR param)
{
    /*
     * Some QCOM OMX_getParameter implementations override the nSize element to
     * a bad value. So, save the initial nSize in order to restore it after.
     */
    OMX_U32 nSize = *(OMX_U32*)param;
    OMX_ERRORTYPE error;
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;

    error = get_error(ctx->iomx->getParameter(node->node, param_index, param, nSize));
    *(OMX_U32*)param = nSize;
    return error;
}

static OMX_ERRORTYPE iomx_set_parameter(OMX_HANDLETYPE component, OMX_INDEXTYPE param_index, OMX_PTR param)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    return get_error(ctx->iomx->setParameter(node->node, param_index, param, *(OMX_U32*)param));
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
#ifdef HAS_USE_BUFFER
    info->graphicBuffer = NULL;
#endif
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

#ifdef HAS_USE_BUFFER
static OMX_ERRORTYPE iomx_use_buffer(OMX_HANDLETYPE component, OMX_BUFFERHEADERTYPE **bufferptr, OMX_U32 port_index, OMX_PTR app_private, OMX_U32 size, OMX_U8* data)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    OMXBuffer* info = new OMXBuffer;
    info->dealer = NULL;
#if ANDROID_API <= 13
    info->graphicBuffer = new GraphicBuffer((android_native_buffer_t*) data, false);
#else
    info->graphicBuffer = new GraphicBuffer((ANativeWindowBuffer*) data, false);
#endif
    int ret = ctx->iomx->useGraphicBuffer(node->node, port_index, info->graphicBuffer, &info->id);
    if (ret != OK)
        return OMX_ErrorUndefined;
    OMX_BUFFERHEADERTYPE *buffer = (OMX_BUFFERHEADERTYPE*) calloc(1, sizeof(OMX_BUFFERHEADERTYPE));
    *bufferptr = buffer;
    buffer->pPlatformPrivate = info;
    buffer->pAppPrivate = app_private;
    buffer->nAllocLen = size;
    buffer->pBuffer = data;
    node->buffers.push_back(buffer);
    return OMX_ErrorNone;
}
#endif

static OMX_ERRORTYPE iomx_free_buffer(OMX_HANDLETYPE component, OMX_U32 port, OMX_BUFFERHEADERTYPE *buffer)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    OMXBuffer* info = (OMXBuffer*) buffer->pPlatformPrivate;
    status_t ret = ctx->iomx->freeBuffer(node->node, port, info->id);
    for( List<OMX_BUFFERHEADERTYPE*>::iterator it = node->buffers.begin(); it != node->buffers.end(); ++it ) {
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
    for( List<IOMX::ComponentInfo>::iterator it = ctx->components.begin(); it != ctx->components.end(); ++it ) {
        if (node->component_name == it->mName) {
            if (index >= it->mRoles.size())
                return OMX_ErrorNoMore;
            List<String8>::iterator it2 = it->mRoles.begin();
            for( OMX_U32 i = 0; it2 != it->mRoles.end() && i < index; i++, ++it2 ) ;
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
    return get_error(ctx->iomx->setConfig(node->node, index, param, *(OMX_U32*)param));
}

static OMX_ERRORTYPE iomx_get_config(OMX_HANDLETYPE component, OMX_INDEXTYPE index, OMX_PTR param)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    return get_error(ctx->iomx->getConfig(node->node, index, param, *(OMX_U32*)param));
}

extern "C" {
OMX_ERRORTYPE PREFIX(OMX_GetHandle)(OMX_HANDLETYPE *handle_ptr, OMX_STRING component_name, OMX_PTR app_data, OMX_CALLBACKTYPE *callbacks)
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
#ifdef HAS_USE_BUFFER
    component->UseBuffer = iomx_use_buffer;
#else
    component->UseBuffer = NULL;
#endif
    component->ComponentRoleEnum = iomx_component_role_enum;
    component->GetExtensionIndex = iomx_get_extension_index;
    component->SetConfig = iomx_set_config;
    component->GetConfig = iomx_get_config;

    *handle_ptr = component;
    node->handle = component;
    status_t ret;
    if ((ret = ctx->iomx->allocateNode( component_name, node->observer, &node->node )) != OK)
        return OMX_ErrorUndefined;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PREFIX(OMX_FreeHandle)(OMX_HANDLETYPE handle)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)handle)->pComponentPrivate;
    ctx->iomx->freeNode( node->node );
    node->observer->setNode(NULL);
    delete node;
    free(handle);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PREFIX(OMX_Init)(void)
{
    OMXClient client;
    if (client.connect() != OK)
        return OMX_ErrorUndefined;

    if (!ctx)
        ctx = new IOMXContext();
    ctx->iomx = client.interface();
    ctx->iomx->listNodes(&ctx->components);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PREFIX(OMX_Deinit)(void)
{
    ctx->iomx = NULL;
    delete ctx;
    ctx = NULL;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PREFIX(OMX_ComponentNameEnum)(OMX_STRING component_name, OMX_U32 name_length, OMX_U32 index)
{
    if (index >= ctx->components.size())
        return OMX_ErrorNoMore;
    List<IOMX::ComponentInfo>::iterator it = ctx->components.begin();
    for( OMX_U32 i = 0; i < index; i++ )
        ++it;
    strncpy(component_name, it->mName.string(), name_length);
    component_name[name_length - 1] = '\0';
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PREFIX(OMX_GetRolesOfComponent)(OMX_STRING component_name, OMX_U32 *num_roles, OMX_U8 **roles)
{
    for( List<IOMX::ComponentInfo>::iterator it = ctx->components.begin(); it != ctx->components.end(); ++it ) {
        if (!strcmp(component_name, it->mName.string())) {
            if (!roles) {
                *num_roles = it->mRoles.size();
                return OMX_ErrorNone;
            }
            if (*num_roles < it->mRoles.size())
                return OMX_ErrorInsufficientResources;
            *num_roles = it->mRoles.size();
            OMX_U32 i = 0;
            for( List<String8>::iterator it2 = it->mRoles.begin(); it2 != it->mRoles.end(); i++, ++it2 ) {
                strncpy((char*)roles[i], it2->string(), OMX_MAX_STRINGNAME_SIZE);
                roles[i][OMX_MAX_STRINGNAME_SIZE - 1] = '\0';
            }
            return OMX_ErrorNone;
        }
    }
    return OMX_ErrorInvalidComponentName;
}

OMX_ERRORTYPE PREFIX(OMX_GetComponentsOfRole)(OMX_STRING role, OMX_U32 *num_comps, OMX_U8 **comp_names)
{
    OMX_U32 i = 0;
    for( List<IOMX::ComponentInfo>::iterator it = ctx->components.begin(); it != ctx->components.end(); ++it ) {
        for( List<String8>::iterator it2 = it->mRoles.begin(); it2 != it->mRoles.end(); ++it2 ) {
            if (!strcmp(it2->string(), role)) {
                if (comp_names) {
                    if (*num_comps < i)
                        return OMX_ErrorInsufficientResources;
                    strncpy((char*)comp_names[i], it->mName.string(), OMX_MAX_STRINGNAME_SIZE);
                    comp_names[i][OMX_MAX_STRINGNAME_SIZE - 1] = '\0';
                }
                i++;
                break;
            }
        }
    }
    *num_comps = i;
    return OMX_ErrorNone;
}

#ifdef HAS_USE_BUFFER
OMX_ERRORTYPE PREFIX(OMXAndroid_EnableGraphicBuffers)(OMX_HANDLETYPE component, OMX_U32 port_index, OMX_BOOL enable)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    int ret = ctx->iomx->enableGraphicBuffers(node->node, port_index, enable);
    if (ret != OK)
        return OMX_ErrorUndefined;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PREFIX(OMXAndroid_GetGraphicBufferUsage)(OMX_HANDLETYPE component, OMX_U32 port_index, OMX_U32* usage)
{
    OMXNode* node = (OMXNode*) ((OMX_COMPONENTTYPE*)component)->pComponentPrivate;
    int ret = ctx->iomx->getGraphicBufferUsage(node->node, port_index, usage);
    if (ret != OK)
        return OMX_ErrorUndefined;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PREFIX(OMXAndroid_GetHalFormat)( const char *comp_name, int* hal_format )
{
    if( !strncmp( comp_name, "OMX.SEC.", 8 ) ) {
        switch( *hal_format ) {
        case OMX_COLOR_FormatYUV420SemiPlanar:
            *hal_format = 0x105; // HAL_PIXEL_FORMAT_YCbCr_420_SP
            break;
        case OMX_COLOR_FormatYUV420Planar:
            *hal_format = 0x101; // HAL_PIXEL_FORMAT_YCbCr_420_P
            break;
        }
    }
    else if( !strcmp( comp_name, "OMX.TI.720P.Decoder" ) ||
        !strcmp( comp_name, "OMX.TI.Video.Decoder" ) )
        *hal_format = 0x14; // HAL_PIXEL_FORMAT_YCbCr_422_I
#if ANDROID_API <= 13 // Required on msm8660 on 3.2, not required on 4.1
    else if( !strcmp( comp_name, "OMX.qcom.video.decoder.avc" ))
        *hal_format = 0x108;
#endif

    return OMX_ErrorNone;
}

#endif
}

