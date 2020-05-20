/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#include "compositor_dcomp_uisurface.hpp"
#include "compositor_dcomp_error.hpp"
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>
#include <QQmlError>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

#include <qpa/qplatformnativeinterface.h>

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <comdef.h>

namespace vlc {

using namespace Microsoft::WRL;

static const char *shaderStr = R"(
Texture2D shaderTexture;
SamplerState samplerState;
struct PS_INPUT
{
    float4 position     : SV_POSITION;
    float4 textureCoord : TEXCOORD0;
};

float4 PShader(PS_INPUT In) : SV_TARGET
{
    return shaderTexture.Sample(samplerState, In.textureCoord);
}

struct VS_INPUT
{
    float4 position     : POSITION;
    float4 textureCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 position     : SV_POSITION;
    float4 textureCoord : TEXCOORD0;
};

VS_OUTPUT VShader(VS_INPUT In)
{
    return In;
}
)";

struct SHADER_INPUT {
    struct {
        FLOAT x;
        FLOAT y;
        FLOAT z;
    } position;
    struct {
        FLOAT x;
        FLOAT y;
    } texture;
};


#define BORDER_LEFT    (-1.0f)
#define BORDER_RIGHT   ( 1.0f)
#define BORDER_TOP     ( 1.0f)
#define BORDER_BOTTOM  (-1.0f)

static HINSTANCE Direct3D11LoadShaderLibrary(void)
{
    HINSTANCE instance = NULL;
    /* d3dcompiler_47 is the latest on windows 8.1 */
    for (int i = 47; i > 41; --i) {
        WCHAR filename[19];
        _snwprintf(filename, 19, TEXT("D3DCOMPILER_%d.dll"), i);
        instance = LoadLibrary(filename);
        if (instance) break;
    }
    return instance;
}

class CompositorDCompositionUISurface::OurD3DCompiler
{
public:

    ~OurD3DCompiler()
    {
        if (m_compiler_dll)
        {
            FreeLibrary(m_compiler_dll);
            m_compiler_dll = nullptr;
        }
        compile = nullptr;
    }

    int init(vlc_object_t *obj)
    {
        m_compiler_dll = Direct3D11LoadShaderLibrary();
        if (!m_compiler_dll) {
            msg_Err(obj, "cannot load d3dcompiler.dll, aborting");
            return VLC_EGENERIC;
        }

        compile = (pD3DCompile)GetProcAddress(m_compiler_dll, "D3DCompile");
        if (!compile) {
            msg_Err(obj, "Cannot locate reference to D3DCompile in d3dcompiler DLL");
            FreeLibrary(m_compiler_dll);
            m_compiler_dll = nullptr;
            return VLC_EGENERIC;
        }
        return VLC_SUCCESS;
    }

    pD3DCompile               compile = nullptr;

private:
    HINSTANCE                 m_compiler_dll = nullptr;
};

CompositorDCompositionUISurface::CompositorDCompositionUISurface(intf_thread_t* p_intf,
                                                                 QWindow* window,
                                                                 Microsoft::WRL::ComPtr<IDCompositionVisual> dcVisual,
                                                                 QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
    , m_dcUiVisual(dcVisual)
    , m_rootWindow(window)
{
}

bool CompositorDCompositionUISurface::init()
{
    EGLBoolean eglRet;

    QSurfaceFormat format;
    // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
    format.setDepthBufferSize(8);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);

    m_context = new QOpenGLContext(this);
    m_context->setScreen(m_rootWindow->screen());
    m_context->setFormat(format);
    assert(m_context->create());
    assert(m_context->isValid());

    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    m_eglDisplay = static_cast<EGLDisplay>(nativeInterface->nativeResourceForContext("eglDisplay", m_context));
    m_eglCtx = static_cast<EGLContext>(nativeInterface->nativeResourceForContext("eglContext", m_context));
    m_eglConfig = static_cast<EGLConfig>(nativeInterface->nativeResourceForContext("eglConfig", m_context));

    PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT = (PFNEGLQUERYDISPLAYATTRIBEXTPROC)eglGetProcAddress("eglQueryDisplayAttribEXT");
    PFNEGLQUERYDEVICEATTRIBEXTPROC eglQueryDeviceAttribEXT = (PFNEGLQUERYDEVICEATTRIBEXTPROC)eglGetProcAddress("eglQueryDeviceAttribEXT");

    EGLDeviceEXT m_eglDevice  = 0;
    eglRet = eglQueryDisplayAttribEXT(m_eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&m_eglDevice));
    if (!eglRet || m_eglDevice == 0)
    {
        msg_Err(m_intf, "failed to retreive egl device");
        return false;
    }
    eglRet = eglQueryDeviceAttribEXT(m_eglDevice, EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(m_qtd3dDevice.GetAddressOf()));
    if (!eglRet)
    {
        msg_Err(m_intf, "failed to retreive egl device");
        return false;
    }

    m_uiOffscreenSurface = new QOffscreenSurface();
    m_uiOffscreenSurface->setFormat(format);;
    m_uiOffscreenSurface->create();

    m_uiRenderControl = new CompositorDCompositionRenderControl(m_rootWindow);

    m_uiWindow = new QQuickWindow(m_uiRenderControl);
    m_uiWindow->setDefaultAlphaBuffer(true);
    m_uiWindow->setFormat(format);
    m_uiWindow->setClearBeforeRendering(false);

    m_d3dCompiler = std::make_shared<OurD3DCompiler>();
    m_d3dCompiler->init(VLC_OBJECT(m_intf));

    qreal dpr = m_rootWindow->devicePixelRatio();
    initialiseD3DSwapchain(dpr * m_rootWindow->width(), dpr * m_rootWindow->height());

    HR(m_dcUiVisual->SetContent(m_d3dSwapChain.Get()), "fail to create surface");

    m_qmlEngine = new QQmlEngine();
    if (!m_qmlEngine->incubationController())
        m_qmlEngine->setIncubationController(m_uiWindow->incubationController());

    connect(m_uiWindow, &QQuickWindow::sceneGraphInitialized, this, &CompositorDCompositionUISurface::createFbo);
    connect(m_uiWindow, &QQuickWindow::sceneGraphInvalidated, this, &CompositorDCompositionUISurface::destroyFbo);
    connect(m_uiRenderControl, &QQuickRenderControl::renderRequested, this, &CompositorDCompositionUISurface::requestUpdate);
    connect(m_uiRenderControl, &QQuickRenderControl::sceneChanged, this, &CompositorDCompositionUISurface::requestUpdate);

    m_rootWindow->installEventFilter(this);
    return true;
}

CompositorDCompositionUISurface::~CompositorDCompositionUISurface()
{
    if (m_uiWindow)
        delete m_uiWindow;
    if (m_uiRenderControl)
        delete m_uiRenderControl;
    if (m_uiOffscreenSurface)
        delete m_uiOffscreenSurface;
    if (m_context)
        delete m_context;
    if (m_rootItem)
        delete m_rootItem;
    if (m_qmlEngine)
        delete m_qmlEngine;
    releaseSharedTexture();
}

void CompositorDCompositionUISurface::initialiseD3DSwapchain(int width, int height)
{
    HRESULT hr;
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT
            ;

    HR(D3D11CreateDevice(
        nullptr,    // Adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,    // Module
        creationFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        nullptr,
        &m_d3dContext), "create D3D11 Context");

    ComPtr<ID3D10Multithread> pMultithread;
    hr = m_d3dDevice.As(&pMultithread);
    if (SUCCEEDED(hr)) {
        pMultithread->SetMultithreadProtected(TRUE);
        pMultithread.Reset();
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    HR(m_d3dDevice.As(&dxgiDevice));

    ComPtr<IDXGIAdapter> dxgiAdapter;
    HR(dxgiDevice->GetAdapter(&dxgiAdapter));

    ComPtr<IDXGIFactory2> dxgiFactory;
    HR(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

    //Create swapchain
    DXGI_SWAP_CHAIN_DESC1 scd = { };
    scd.Width = width;
    scd.Height = height;
    scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.BufferCount = 2;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.Scaling = DXGI_SCALING_STRETCH;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scd.Flags = 0;

    HR(dxgiFactory->CreateSwapChainForComposition(m_d3dDevice.Get(), &scd, nullptr, &m_d3dSwapChain));
    ComPtr<ID3D11Texture2D> backTexture;
    HR(m_d3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&backTexture)), "Get swapchain buffer");
    HR(m_d3dDevice->CreateRenderTargetView(backTexture.Get(), nullptr, &m_d3dRenderTarget));

    D3D11_VIEWPORT viewport = { 0.f, 0.f, (float)width, (float)height, 0.f, 0.f};
    m_d3dContext->RSSetViewports(1, &viewport);
    m_d3dContext->OMSetRenderTargets(1, m_d3dRenderTarget.GetAddressOf(), nullptr);

    //Create shaders
    ComPtr<ID3D10Blob> VS, PS, pErrBlob;
    assert(m_d3dCompiler->compile);
    hr = m_d3dCompiler->compile(shaderStr, strlen(shaderStr), nullptr, nullptr, nullptr, "VShader", "vs_4_0", 0, 0, &VS, &pErrBlob);
    if (FAILED(hr))
    {
        char* err = pErrBlob ? (char*)pErrBlob->GetBufferPointer() : nullptr;
        msg_Err(m_intf, "fail to compile vertex shader (0x%lX) : %s", hr, err);
        return;
    }

    hr = m_d3dCompiler->compile(shaderStr, strlen(shaderStr), nullptr, nullptr, nullptr, "PShader", "ps_4_0", 0, 0, &PS, &pErrBlob);
    if (FAILED(hr))
    {
        char* err = pErrBlob ? (char*)pErrBlob->GetBufferPointer() : nullptr;
        msg_Err(m_intf, "fail to compile pixel shader (0x%lX) : %s", hr, err);
        return;
    }

    HR(m_d3dDevice->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &m_VS), "CreateVertexShader");
    HR(m_d3dDevice->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &m_PS), "CreatePixelShader");

    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    HR(m_d3dDevice->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &m_ShadersInputLayout));
    //the texture is rendered upside down
    SHADER_INPUT OurVertices[] =
    {
        {{BORDER_LEFT,  BORDER_BOTTOM, 0.0f},  {0.0f, 0.0f}},
        {{BORDER_RIGHT, BORDER_BOTTOM, 0.0f},  {1.0f, 0.0f}},
        {{BORDER_RIGHT, BORDER_TOP,    0.0f},  {1.0f, 1.0f}},
        {{BORDER_LEFT,  BORDER_TOP,    0.0f},  {0.0f, 1.0f}},
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(OurVertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HR(m_d3dDevice->CreateBuffer(&bd, nullptr, &m_VertexBuffer), "create vertex buffer");
    m_vertexBufferStride = sizeof(OurVertices[0]);

    D3D11_MAPPED_SUBRESOURCE ms;
    HR(m_d3dContext->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms));
    memcpy(ms.pData, OurVertices, sizeof(OurVertices));
    m_d3dContext->Unmap(m_VertexBuffer.Get(), 0);

    m_quadIndexCount = 6;
    D3D11_BUFFER_DESC quadDesc = { };
    quadDesc.Usage = D3D11_USAGE_DYNAMIC;
    quadDesc.ByteWidth = sizeof(WORD) * m_quadIndexCount;
    quadDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    quadDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR(m_d3dDevice->CreateBuffer(&quadDesc, nullptr, &m_IndexBuffer), "create triangle list buffer");

    HR(m_d3dContext->Map(m_IndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms));
    WORD *triangle_pos = static_cast<WORD*>(ms.pData);
    triangle_pos[0] = 3;
    triangle_pos[1] = 1;
    triangle_pos[2] = 0;
    triangle_pos[3] = 2;
    triangle_pos[4] = 1;
    triangle_pos[5] = 3;
    m_d3dContext->Unmap(m_IndexBuffer.Get(), 0);

    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_d3dContext->IASetInputLayout(m_ShadersInputLayout.Get());
    UINT offset = 0;
    m_d3dContext->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &m_vertexBufferStride, &offset);
    m_d3dContext->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    m_d3dContext->VSSetShader(m_VS.Get(), 0, 0);
    m_d3dContext->PSSetShader(m_PS.Get(), 0, 0);

    D3D11_SAMPLER_DESC sampDesc {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HR(m_d3dDevice->CreateSamplerState(&sampDesc, &m_samplerState));
    m_d3dContext->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    updateSharedTexture(width, height);
}

void CompositorDCompositionUISurface::resizeSwapchain(int width, int height)
{
    try
    {
        m_d3dContext->OMSetRenderTargets(0, 0, 0);
        m_d3dRenderTarget.Reset();

        HR(m_d3dSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "resize buffer");

        ComPtr<ID3D11Texture2D> backTexture;
        HR(m_d3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&backTexture)), "get back buffer");
        HR(m_d3dDevice->CreateRenderTargetView(backTexture.Get(), nullptr, &m_d3dRenderTarget));
    }
    catch( const DXError& err )
    {
        msg_Warn(m_intf, "failed to resize: %s, code 0x%lX", err.what(), err.code());
    }
}

void CompositorDCompositionUISurface::releaseSharedTexture()
{
    if (m_eglInterimTextureQt)
    {
        eglDestroySurface(m_eglDisplay, m_eglInterimTextureQt);
        m_eglInterimTextureQt = 0;
    }
    if (m_sharedTextureHandled) {
        CloseHandle(m_sharedTextureHandled);
        m_sharedTextureHandled = nullptr;
    }
    m_d3dInterimTexture.Reset();
    m_textureShaderInput.Reset();
    m_d3dInterimTextureQt.Reset();
}

void CompositorDCompositionUISurface::updateSharedTexture(int width, int height)
{
    try
    {
        releaseSharedTexture();
        /* interim texture */
        D3D11_TEXTURE2D_DESC texDesc = { };
        texDesc.MipLevels = 1;
        texDesc.SampleDesc.Count = 1;
        texDesc.MiscFlags = 0;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = 0;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.Height = height;
        texDesc.Width  = width;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

        HR(m_d3dDevice->CreateTexture2D( &texDesc, NULL, &m_d3dInterimTexture ), "create texture");

        //share texture between our swapchain and Qt
        ComPtr<IDXGIResource1> sharedResource;
        HR(m_d3dInterimTexture.As(&sharedResource));
        HR(sharedResource->CreateSharedHandle(NULL, DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE, NULL, &m_sharedTextureHandled), "create shared texture (d3d)");

        D3D11_SHADER_RESOURCE_VIEW_DESC resviewDesc = {};
        resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        resviewDesc.Texture2D.MipLevels = 1;
        resviewDesc.Format = texDesc.Format;
        HR(m_d3dDevice->CreateShaderResourceView(m_d3dInterimTexture.Get(), &resviewDesc, &m_textureShaderInput ), "create share resource view");

        m_d3dContext->PSSetShaderResources(0, 1, m_textureShaderInput.GetAddressOf());

        //bind shared texture on Qt side
        ComPtr<ID3D11Device1> m_qtd3dDevice1;
        HR(m_qtd3dDevice.As(&m_qtd3dDevice1));
        HR(m_qtd3dDevice1->OpenSharedResource1(m_sharedTextureHandled, IID_PPV_ARGS(&m_d3dInterimTextureQt)), "open shared texture (Qt)");

        EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(m_d3dInterimTextureQt.Get());
        EGLint pBufferAttributes[] =
        {
            EGL_WIDTH, width,
            EGL_HEIGHT, height,
            EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE, EGL_TRUE,
            EGL_NONE
        };

        m_eglInterimTextureQt = eglCreatePbufferFromClientBuffer(m_eglDisplay, EGL_D3D_TEXTURE_ANGLE, buffer, m_eglConfig, pBufferAttributes);
    }
    catch (const DXError& err)
    {
        msg_Warn(m_intf, "failed to update shared texture: %s, code 0x%lX", err.what(), err.code());
    }
}


void CompositorDCompositionUISurface::setContent(QQmlComponent*,  QQuickItem* rootItem)
{
    m_rootItem = rootItem;

    QQuickItem* contentItem  = m_uiWindow->contentItem();

    m_rootItem->setParentItem(contentItem);

    updateSizes();

    m_context->makeCurrent(m_uiOffscreenSurface);
    m_uiRenderControl->initialize(m_context);
    m_context->doneCurrent();

    requestUpdate();
}

void CompositorDCompositionUISurface::render()
{
    EGLBoolean eglRet;

    QSize realSize = m_rootWindow->size() * m_rootWindow->devicePixelRatio();
    if (realSize != m_surfaceSize)
    {
        m_surfaceSize = realSize;
    }

    //Draw on Qt side
    m_context->makeCurrent(m_uiOffscreenSurface);
    eglRet = eglMakeCurrent(m_eglDisplay, m_eglInterimTextureQt, m_eglInterimTextureQt, m_eglCtx);
    if (!eglRet)
    {
        msg_Warn(m_intf, "failed to make current egl context");
        return;
    }
    m_context->functions()->glViewport(0, 0, realSize.width(), realSize.height());
    m_context->functions()->glScissor( 0, 0, realSize.width(), realSize.height());
    m_context->functions()->glEnable(GL_SCISSOR_TEST);
    m_context->functions()->glClearColor(0.,0.,0.,0.);
    m_context->functions()->glClear(GL_COLOR_BUFFER_BIT);

    m_uiRenderControl->polishItems();
    m_uiRenderControl->sync();
    m_uiRenderControl->render();

    //glFinish will present, glFlush isn't enough
    m_context->functions()->glFinish();
    m_context->doneCurrent();

    //Draw on D3D side side
    m_d3dContext->OMSetRenderTargets(1, m_d3dRenderTarget.GetAddressOf(), nullptr);
    D3D11_VIEWPORT viewport = { 0.f, 0.f, (float)m_surfaceSize.width(), (float)m_surfaceSize.height(), 0.f, 0.f};
    m_d3dContext->RSSetViewports(1, &viewport);
    m_d3dContext->DrawIndexed(m_quadIndexCount, 0, 0);

    HRESULT hr = m_d3dSwapChain->Present(0, 0);
    if ( hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET )
    {
        msg_Err( m_intf, "SwapChain Present failed. code 0x%lX)", hr );
    }
}

void CompositorDCompositionUISurface::timerEvent(QTimerEvent *event)
{
    if (!event)
        return;
    if (event->timerId() == m_renderTimer.timerId())
    {
        m_renderPending = false;
        m_renderTimer.stop();
        render();
    }
}

static void remapInputMethodQueryEvent(QObject *object, QInputMethodQueryEvent *e)
{
    auto item = qobject_cast<QQuickItem *>(object);
    if (!item)
        return;
    // Remap all QRectF values.
    for (auto query : {Qt::ImCursorRectangle, Qt::ImAnchorRectangle, Qt::ImInputItemClipRectangle})
    {
        if (e->queries() & query)
        {
            auto value = e->value(query);
            if (value.canConvert<QRectF>())
                e->setValue(query, item->mapRectToScene(value.toRectF()));
        }
    }
    // Remap all QPointF values.
    if (e->queries() & Qt::ImCursorPosition)
    {
        auto value = e->value(Qt::ImCursorPosition);
        if (value.canConvert<QPointF>())
            e->setValue(Qt::ImCursorPosition, item->mapToScene(value.toPointF()));
    }
}

bool CompositorDCompositionUISurface::eventFilter(QObject* object, QEvent* event)
{
    if (object != m_rootWindow)
        return false;

    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Show:
        updatePosition();
        break;

    case QEvent::Resize:
        updateSizes();
        break;

    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::Leave:
        return QCoreApplication::sendEvent(m_uiWindow, event);
    case QEvent::Enter:
    {
        QEnterEvent *enterEvent = static_cast<QEnterEvent *>(event);
        QEnterEvent mappedEvent(enterEvent->localPos(), enterEvent->windowPos(),
                                enterEvent->screenPos());
        bool ret = QCoreApplication::sendEvent(m_uiWindow, &mappedEvent);
        event->setAccepted(mappedEvent.isAccepted());
        return ret;
    }

    case QEvent::InputMethod:
        return QCoreApplication::sendEvent(m_uiWindow->focusObject(), event);
    case QEvent::InputMethodQuery:
    {
        bool eventResult = QCoreApplication::sendEvent(m_uiWindow->focusObject(), event);
        // The result in focusObject are based on offscreenWindow. But
        // the inputMethodTransform won't get updated because the focus
        // is on QQuickWidget. We need to remap the value based on the
        // widget.
        remapInputMethodQueryEvent(m_uiWindow->focusObject(), static_cast<QInputMethodQueryEvent *>(event));
        return eventResult;
    }

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QMouseEvent mappedEvent(mouseEvent->type(), mouseEvent->localPos(),
                                mouseEvent->localPos(), mouseEvent->screenPos(),
                                mouseEvent->button(), mouseEvent->buttons(),
                                mouseEvent->modifiers(), mouseEvent->source());
        QCoreApplication::sendEvent(m_uiWindow, &mappedEvent);
        return true;
    }
    case QEvent::Wheel:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:

    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::DragLeave:
    case QEvent::DragResponse:
    case QEvent::Drop:
        return QCoreApplication::sendEvent(m_uiWindow, event);

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        return QCoreApplication::sendEvent(m_uiWindow, event);

    case QEvent::ScreenChangeInternal:
        m_uiWindow->setScreen(m_rootWindow->screen());
        break;
    default:
        break;
    }
    return false;
}

void CompositorDCompositionUISurface::createFbo()
{
    //write to the immediate context
    m_uiWindow->setRenderTarget(0, m_rootWindow->size());
}

void CompositorDCompositionUISurface::destroyFbo()
{
}

void CompositorDCompositionUISurface::updateSizes()
{
    qreal dpr = m_rootWindow->devicePixelRatio();
    QSize windowSize = m_rootWindow->size();

    resizeSwapchain(windowSize.width() * dpr, windowSize.height() * dpr);
    updateSharedTexture(windowSize.width() * dpr, windowSize.height() * dpr);

    // Behave like SizeRootObjectToView.
    m_rootItem->setSize(windowSize);
    m_uiWindow->resize(windowSize);
}

void CompositorDCompositionUISurface::updatePosition()
{
    QPoint windowPosition = m_rootWindow->mapToGlobal(QPoint(0,0));
    if (m_uiWindow->position() != windowPosition)
        m_uiWindow->setPosition(windowPosition);
}

void CompositorDCompositionUISurface::requestUpdate()
{
    //Don't flood the redering with requests
    if (!m_renderPending) {
        m_renderPending = true;
        m_renderTimer.start(5, Qt::PreciseTimer, this);
    }
}

void CompositorDCompositionUISurface::handleScreenChange()
{
    msg_Info(m_intf, "handle screen change");
    m_uiWindow->setGeometry(0, 0, m_rootWindow->width(), m_rootWindow->height());;
    requestUpdate();
}

}
