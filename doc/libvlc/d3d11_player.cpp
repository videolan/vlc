/* compile: g++ d3d11_player.cpp -o d3d11_player.exe -L<path/libvlc> -lvlc -ld3d11 -ld3dcompiler_47 -luuid */

/* This is the most extreme use case where libvlc is given its own ID3D11DeviceContext
   and draws in a texture shared with the ID3D11DeviceContext of the app.

   It's possible to share the ID3D11DeviceContext as long as the proper PixelShader
   calls are overridden in the app after each libvlc drawing (see libvlc D3D11 doc).

   It's also possible to use the SwapChain directly with libvlc and let it draw on its
   entire area instead of drawing in a texture.
*/

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <assert.h>

#include <d3d11_1.h>
#include <dxgi1_2.h>

#ifdef DEBUG_D3D11_LEAKS
# include <initguid.h>
# include <dxgidebug.h>
#endif

#include <vlc/vlc.h>

#define SCREEN_WIDTH  1500
#define SCREEN_HEIGHT  900
#define BORDER_LEFT    (-0.95f)
#define BORDER_RIGHT   ( 0.85f)
#define BORDER_TOP     ( 0.95f)
#define BORDER_BOTTOM  (-0.90f)

#define check_leak(x)  assert(x)

struct render_context
{
    HWND hWnd;

    /* resources shared by VLC */
    ID3D11Device            *d3deviceVLC;
    ID3D11DeviceContext     *d3dctxVLC;

    struct {
        ID3D11Texture2D         *textureVLC; // shared between VLC and the app
        ID3D11RenderTargetView  *textureRenderTarget;
        HANDLE                  sharedHandled; // handle of the texture used by VLC and the app

        /* texture VLC renders into */
        ID3D11Texture2D          *texture;
        ID3D11ShaderResourceView *textureShaderInput;
    } resized;

    /* Direct3D11 device/context */
    ID3D11Device        *d3device;
    ID3D11DeviceContext *d3dctx;

    IDXGISwapChain         *swapchain;
    ID3D11RenderTargetView *swapchainRenderTarget;

    /* our vertex/pixel shader */
    ID3D11VertexShader *pVS;
    ID3D11PixelShader  *pPS;
    ID3D11InputLayout  *pShadersInputLayout;

    UINT vertexBufferStride;
    ID3D11Buffer *pVertexBuffer;

    UINT quadIndexCount;
    ID3D11Buffer *pIndexBuffer;

    ID3D11SamplerState *samplerState;

    CRITICAL_SECTION sizeLock; // the ReportSize callback cannot be called during/after the Cleanup_cb is called
    unsigned width, height;
    void (*ReportSize)(void *ReportOpaque, unsigned width, unsigned height);
    void *ReportOpaque;
};

static const char *shaderStr = "\
Texture2D shaderTexture;\n\
SamplerState samplerState;\n\
struct PS_INPUT\n\
{\n\
    float4 position     : SV_POSITION;\n\
    float4 textureCoord : TEXCOORD0;\n\
};\n\
\n\
float4 PShader(PS_INPUT In) : SV_TARGET\n\
{\n\
    return shaderTexture.Sample(samplerState, In.textureCoord);\n\
}\n\
\n\
struct VS_INPUT\n\
{\n\
    float4 position     : POSITION;\n\
    float4 textureCoord : TEXCOORD0;\n\
};\n\
\n\
struct VS_OUTPUT\n\
{\n\
    float4 position     : SV_POSITION;\n\
    float4 textureCoord : TEXCOORD0;\n\
};\n\
\n\
VS_OUTPUT VShader(VS_INPUT In)\n\
{\n\
    return In;\n\
}\n\
";

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

static void init_direct3d(struct render_context *ctx)
{
    HRESULT hr;
    DXGI_SWAP_CHAIN_DESC scd = { };

    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = SCREEN_WIDTH;
    scd.BufferDesc.Height = SCREEN_HEIGHT;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = ctx->hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT creationFlags = 0;
#ifndef NDEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D11CreateDeviceAndSwapChain(NULL,
                                  D3D_DRIVER_TYPE_HARDWARE,
                                  NULL,
                                  creationFlags,
                                  NULL,
                                  NULL,
                                  D3D11_SDK_VERSION,
                                  &scd,
                                  &ctx->swapchain,
                                  &ctx->d3device,
                                  NULL,
                                  &ctx->d3dctx);

    /* The ID3D11Device must have multithread protection */
    ID3D10Multithread *pMultithread;
    hr = ctx->d3device->QueryInterface( __uuidof(ID3D10Multithread), (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        pMultithread->SetMultithreadProtected(TRUE);
        pMultithread->Release();
    }

    // RECT currentRect;
    // GetWindowRect(hWnd, &currentRect);
    //currentRect.right - currentRect.left;
    //currentRect.bottom - currentRect.top;

    D3D11_VIEWPORT viewport = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0 };

    ctx->d3dctx->RSSetViewports(1, &viewport);

    D3D11CreateDevice(NULL,
                      D3D_DRIVER_TYPE_HARDWARE,
                      NULL,
                      creationFlags | D3D11_CREATE_DEVICE_VIDEO_SUPPORT, /* needed for hardware decoding */
                      NULL, 0,
                      D3D11_SDK_VERSION,
                      &ctx->d3deviceVLC, NULL, &ctx->d3dctxVLC);

    ID3D11Texture2D *pBackBuffer;
    ctx->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

    ctx->d3device->CreateRenderTargetView(pBackBuffer, NULL, &ctx->swapchainRenderTarget);
    pBackBuffer->Release();

    ctx->d3dctx->OMSetRenderTargets(1, &ctx->swapchainRenderTarget, NULL);

    ID3D10Blob *VS, *PS, *pErrBlob;
    char *err;
    hr = D3DCompile(shaderStr, strlen(shaderStr),
                    NULL, NULL, NULL, "VShader", "vs_4_0", 0, 0, &VS, &pErrBlob);
    err = pErrBlob ? (char*)pErrBlob->GetBufferPointer() : NULL;
    hr = D3DCompile(shaderStr, strlen(shaderStr),
                    NULL, NULL, NULL, "PShader", "ps_4_0", 0, 0, &PS, &pErrBlob);
    err = pErrBlob ? (char*)pErrBlob->GetBufferPointer() : NULL;

    ctx->d3device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &ctx->pVS);
    ctx->d3device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &ctx->pPS);

    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = ctx->d3device->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &ctx->pShadersInputLayout);
    SHADER_INPUT OurVertices[] =
    {
        {{BORDER_LEFT,  BORDER_BOTTOM, 0.0f},  {0.0f, 1.0f}},
        {{BORDER_RIGHT, BORDER_BOTTOM, 0.0f},  {1.0f, 1.0f}},
        {{BORDER_RIGHT, BORDER_TOP,    0.0f},  {1.0f, 0.0f}},
        {{BORDER_LEFT,  BORDER_TOP,    0.0f},  {0.0f, 0.0f}},
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));

    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(OurVertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ctx->d3device->CreateBuffer(&bd, NULL, &ctx->pVertexBuffer);
    ctx->vertexBufferStride = sizeof(OurVertices[0]);

    D3D11_MAPPED_SUBRESOURCE ms;
    ctx->d3dctx->Map(ctx->pVertexBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
    memcpy(ms.pData, OurVertices, sizeof(OurVertices));
    ctx->d3dctx->Unmap(ctx->pVertexBuffer, NULL);

    ctx->quadIndexCount = 6;
    D3D11_BUFFER_DESC quadDesc = { };
    quadDesc.Usage = D3D11_USAGE_DYNAMIC;
    quadDesc.ByteWidth = sizeof(WORD) * ctx->quadIndexCount;
    quadDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    quadDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ctx->d3device->CreateBuffer(&quadDesc, NULL, &ctx->pIndexBuffer);

    ctx->d3dctx->Map(ctx->pIndexBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
    WORD *triangle_pos = static_cast<WORD*>(ms.pData);
    triangle_pos[0] = 3;
    triangle_pos[1] = 1;
    triangle_pos[2] = 0;

    triangle_pos[3] = 2;
    triangle_pos[4] = 1;
    triangle_pos[5] = 3;
    ctx->d3dctx->Unmap(ctx->pIndexBuffer, NULL);

    ctx->d3dctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->d3dctx->IASetInputLayout(ctx->pShadersInputLayout);
    UINT offset = 0;
    ctx->d3dctx->IASetVertexBuffers(0, 1, &ctx->pVertexBuffer, &ctx->vertexBufferStride, &offset);
    ctx->d3dctx->IASetIndexBuffer(ctx->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    ctx->d3dctx->VSSetShader(ctx->pVS, 0, 0);
    ctx->d3dctx->PSSetShader(ctx->pPS, 0, 0);

    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = ctx->d3device->CreateSamplerState(&sampDesc, &ctx->samplerState);
    ctx->d3dctx->PSSetSamplers(0, 1, &ctx->samplerState);

}

static void release_textures(struct render_context *ctx)
{
    ULONG ref;
    if (ctx->resized.sharedHandled)
    {
        CloseHandle(ctx->resized.sharedHandled);
        ctx->resized.sharedHandled = NULL;
    }
    if (ctx->resized.textureVLC)
    {
        ref = ctx->resized.textureVLC->Release();
        check_leak(ref == 0);
        ctx->resized.textureVLC = NULL;
    }
    if (ctx->resized.textureShaderInput)
    {
        ref = ctx->resized.textureShaderInput->Release();
        check_leak(ref == 0);
        ctx->resized.textureShaderInput = NULL;
    }
    if (ctx->resized.textureRenderTarget)
    {
        ref = ctx->resized.textureRenderTarget->Release();
        check_leak(ref == 0);
        ctx->resized.textureRenderTarget = NULL;
    }
    if (ctx->resized.texture)
    {
        ref = ctx->resized.texture->Release();
        check_leak(ref == 0);
        ctx->resized.texture = NULL;
    }
}

static void list_dxgi_leaks(void)
{
#ifdef DEBUG_D3D11_LEAKS
    HMODULE dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
    if (dxgidebug_dll)
    {
        typedef HRESULT (WINAPI * LPDXGIGETDEBUGINTERFACE)(REFIID, void ** );
        LPDXGIGETDEBUGINTERFACE pf_DXGIGetDebugInterface;
        pf_DXGIGetDebugInterface = reinterpret_cast<LPDXGIGETDEBUGINTERFACE>(
            reinterpret_cast<void*>( GetProcAddress( dxgidebug_dll, "DXGIGetDebugInterface" ) ) );
        if (pf_DXGIGetDebugInterface)
        {
            IDXGIDebug *pDXGIDebug;
            if (SUCCEEDED(pf_DXGIGetDebugInterface(__uuidof(IDXGIDebug), (void**)&pDXGIDebug)))
                pDXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            pDXGIDebug->Release();
        }
        FreeLibrary(dxgidebug_dll);
    }
#endif // DEBUG_D3D11_LEAKS
}

static void release_direct3d(struct render_context *ctx)
{
    ULONG ref;

    release_textures(ctx);

    ref = ctx->d3dctxVLC->Release();
    check_leak(ref == 0);
    ref = ctx->d3deviceVLC->Release();
    check_leak(ref == 0);

    ref = ctx->samplerState->Release();
    check_leak(ref == 0);
    ref = ctx->pShadersInputLayout->Release();
    check_leak(ref == 0);
    ref = ctx->pVS->Release();
    check_leak(ref == 0);
    ref = ctx->pPS->Release();
    check_leak(ref == 0);
    ref = ctx->pIndexBuffer->Release();
    check_leak(ref == 0);
    ref = ctx->pVertexBuffer->Release();
    check_leak(ref == 0);
    ref = ctx->swapchain->Release();
    check_leak(ref == 0);
    ref = ctx->swapchainRenderTarget->Release();
    check_leak(ref == 0);
    ref = ctx->d3dctx->Release();
    check_leak(ref == 0);
    ref = ctx->d3device->Release();
    check_leak(ref == 0);

    list_dxgi_leaks();
}

static bool UpdateOutput_cb( void *opaque, const libvlc_video_render_cfg_t *cfg, libvlc_video_output_cfg_t *out )
{
    struct render_context *ctx = static_cast<struct render_context *>( opaque );

    HRESULT hr;

    DXGI_FORMAT renderFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    release_textures(ctx);

    /* interim texture */
    D3D11_TEXTURE2D_DESC texDesc = { };
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = renderFormat;
    texDesc.Height = cfg->height;
    texDesc.Width  = cfg->width;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    hr = ctx->d3device->CreateTexture2D( &texDesc, NULL, &ctx->resized.texture );
    if (FAILED(hr)) return false;

    IDXGIResource1* sharedResource = NULL;
    ctx->resized.texture->QueryInterface(__uuidof(IDXGIResource1), (LPVOID*) &sharedResource);
    hr = sharedResource->CreateSharedHandle(NULL, DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE, NULL, &ctx->resized.sharedHandled);
    sharedResource->Release();

    ID3D11Device1* d3d11VLC1;
    ctx->d3deviceVLC->QueryInterface(__uuidof(ID3D11Device1), (LPVOID*) &d3d11VLC1);
    hr = d3d11VLC1->OpenSharedResource1(ctx->resized.sharedHandled, __uuidof(ID3D11Texture2D), (void**)&ctx->resized.textureVLC);
    d3d11VLC1->Release();

    D3D11_SHADER_RESOURCE_VIEW_DESC resviewDesc;
    ZeroMemory(&resviewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resviewDesc.Texture2D.MipLevels = 1;
    resviewDesc.Format = texDesc.Format;
    hr = ctx->d3device->CreateShaderResourceView(ctx->resized.texture, &resviewDesc, &ctx->resized.textureShaderInput );
    if (FAILED(hr)) return false;

    ctx->d3dctx->PSSetShaderResources(0, 1, &ctx->resized.textureShaderInput);

    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {
        .Format = texDesc.Format,
        .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    };
    hr = ctx->d3deviceVLC->CreateRenderTargetView(ctx->resized.textureVLC, &renderTargetViewDesc, &ctx->resized.textureRenderTarget);
    if (FAILED(hr)) return false;

    ctx->d3dctxVLC->OMSetRenderTargets( 1, &ctx->resized.textureRenderTarget, NULL );

    out->dxgi_format    = renderFormat;
    out->full_range     = true;
    out->colorspace     = libvlc_video_colorspace_BT709;
    out->primaries      = libvlc_video_primaries_BT709;
    out->transfer       = libvlc_video_transfer_func_SRGB;

    return true;
}

static void Swap_cb( void* opaque )
{
    struct render_context *ctx = static_cast<struct render_context *>( opaque );
    ctx->swapchain->Present( 0, 0 );
}

static bool StartRendering_cb( void *opaque, bool enter )
{
    struct render_context *ctx = static_cast<struct render_context *>( opaque );
    if ( enter )
    {
        // DEBUG: draw greenish background to show where libvlc doesn't draw in the texture
        // Normally you should Clear with a black background
        static const FLOAT greenRGBA[4] = {0.5f, 0.5f, 0.0f, 1.0f};
        ctx->d3dctxVLC->ClearRenderTargetView( ctx->resized.textureRenderTarget, greenRGBA);
    }
    else
    {
        static const FLOAT orangeRGBA[4] = {1.0f, 0.5f, 0.0f, 1.0f};
        ctx->d3dctx->ClearRenderTargetView(ctx->swapchainRenderTarget, orangeRGBA);

        // Render into the swapchain
        // We start the drawing of the shared texture in our app as early as possible
        // in hope it's done as soon as Swap_cb is called
        ctx->d3dctx->DrawIndexed(ctx->quadIndexCount, 0, 0);
    }

    return true;
}

static bool SelectPlane_cb( void *opaque, size_t plane )
{
    struct render_context *ctx = static_cast<struct render_context *>( opaque );
    if ( plane != 0 ) // we only support one packed RGBA plane (DXGI_FORMAT_R8G8B8A8_UNORM)
        return false;
    return true;
}

static bool Setup_cb( void **opaque, const libvlc_video_setup_device_cfg_t *cfg, libvlc_video_setup_device_info_t *out )
{
    struct render_context *ctx = static_cast<struct render_context *>(*opaque);

    init_direct3d(ctx);

    out->d3d11.device_context = ctx->d3dctxVLC;
    return true;
}

static void Cleanup_cb( void *opaque )
{
    // here we can release all things Direct3D11 for good (if playing only one file)
    struct render_context *ctx = static_cast<struct render_context *>( opaque );
    release_direct3d(ctx);
}

static void Resize_cb( void *opaque,
                       void (*report_size_change)(void *report_opaque, unsigned width, unsigned height),
                       void *report_opaque )
{
    struct render_context *ctx = static_cast<struct render_context *>( opaque );
    EnterCriticalSection(&ctx->sizeLock);
    ctx->ReportSize = report_size_change;
    ctx->ReportOpaque = report_opaque;

    if (ctx->ReportSize != nullptr)
    {
        /* report our initial size */
        ctx->ReportSize(ctx->ReportOpaque, ctx->width, ctx->height);
    }
    LeaveCriticalSection(&ctx->sizeLock);
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if( message == WM_CREATE )
    {
        /* Store p_mp for future use */
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    LONG_PTR p_user_data = GetWindowLongPtr( hWnd, GWLP_USERDATA );
    if( p_user_data == 0 )
        return DefWindowProc(hWnd, message, wParam, lParam);
    struct render_context *ctx = (struct render_context *)p_user_data;

    switch(message)
    {
        case WM_SIZE:
        {
            /* tell libvlc that our size has changed */
            ctx->width  = LOWORD(lParam) * (BORDER_RIGHT - BORDER_LEFT) / 2.0f; /* remove the orange part ! */
            ctx->height = HIWORD(lParam) * (BORDER_TOP - BORDER_BOTTOM) / 2.0f;
            EnterCriticalSection(&ctx->sizeLock);
            if (ctx->ReportSize != nullptr)
                ctx->ReportSize(ctx->ReportOpaque, ctx->width, ctx->height);
            LeaveCriticalSection(&ctx->sizeLock);
        }
        break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc (hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    WNDCLASSEX wc;
    struct render_context Context = { };
    char *file_path;
    libvlc_instance_t *p_libvlc;
    libvlc_media_t *p_media;
    libvlc_media_player_t *p_mp;
    (void)hPrevInstance;

    /* remove "" around the given path */
    if (lpCmdLine[0] == '"')
    {
        file_path = strdup( lpCmdLine+1 );
        if (file_path[strlen(file_path)-1] == '"')
            file_path[strlen(file_path)-1] = '\0';
    }
    else
        file_path = strdup( lpCmdLine );

    p_libvlc = libvlc_new( 0, NULL );
    p_media = libvlc_media_new_path( p_libvlc, file_path );
    free( file_path );
    p_mp = libvlc_media_player_new_from_media( p_media );

    InitializeCriticalSection(&Context.sizeLock);

    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "WindowClass";

    RegisterClassEx(&wc);

    RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    Context.hWnd = CreateWindowEx(0,
                          "WindowClass",
                          "libvlc Demo app",
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          wr.right - wr.left,
                          wr.bottom - wr.top,
                          NULL,
                          NULL,
                          hInstance,
                          &Context);

    ShowWindow(Context.hWnd, nCmdShow);

    // DON'T use with callbacks libvlc_media_player_set_hwnd(p_mp, hWnd);

    /* Tell VLC to render into our D3D11 environment */
    libvlc_video_set_output_callbacks( p_mp, libvlc_video_engine_d3d11,
                                       Setup_cb, Cleanup_cb, Resize_cb, UpdateOutput_cb, Swap_cb, StartRendering_cb,
                                       nullptr, nullptr, SelectPlane_cb,
                                       &Context );

    libvlc_media_player_play( p_mp );

    MSG msg;

    while(TRUE)
    {
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if(msg.message == WM_QUIT)
                break;
        }
    }

    libvlc_media_player_stop_async( p_mp );

    libvlc_media_player_release( p_mp );
    libvlc_media_release( p_media );
    libvlc_release( p_libvlc );

    DeleteCriticalSection(&Context.sizeLock);

    return msg.wParam;
}
