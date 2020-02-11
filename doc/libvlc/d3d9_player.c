/* compile using: gcc d3d9_player.c -o d3d9_player.exe -I <path/liblc> -llibvlc -ld3d9 */

#include <windows.h>
#include <windowsx.h>
#define COBJMACROS
#include <d3d9.h>

#include <vlc/vlc.h>

#define SCREEN_WIDTH   900
#define SCREEN_HEIGHT  900
#define BORDER_LEFT    ( 20)
#define BORDER_RIGHT   (700 + BORDER_LEFT)
#define BORDER_TOP     ( 10)
#define BORDER_BOTTOM  (700 + BORDER_TOP)

struct render_context
{
    HWND hWnd;

    IDirect3D9Ex *d3d;
    IDirect3DDevice9 *d3ddev;     /* the host app device */
    IDirect3DDevice9 *libvlc_d3d; /* the device where VLC can do its rendering */

    /* surface we'll use to display on our backbuffer */
    IDirect3DTexture9 *renderTexture;
    HANDLE sharedHandled;
    /* surface that VLC will render to, which is a shared version of renderTexture
     * on libvlc_d3d                                                           */
    IDirect3DTexture9 *sharedRenderTexture;
    IDirect3DSurface9 *sharedRenderSurface;

    /* our swapchain backbuffer */
    IDirect3DSurface9 *backBuffer;

    IDirect3DVertexBuffer9 *rectangleFVFVertexBuf;

    CRITICAL_SECTION sizeLock; // the ReportSize callback cannot be called during/after the Cleanup_cb is called
    unsigned width, height;
    void (*ReportSize)(void *ReportOpaque, unsigned width, unsigned height);
    void *ReportOpaque;
};

struct CUSTOMVERTEX {FLOAT X, Y, Z, RHW; DWORD COLOR;
                     FLOAT tu, tv; /* texture relative coordinates */};
#define CUSTOMFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)

/**
 * Callback called when it's time to display the video, in sync with the audio.
 *
 * This is called outside of the UI thread (in the VLC rendering thread).
 */
static void Swap(struct render_context *ctx)
{
    /* finished drawing to our swap surface, now render that surface to the backbuffer */
    IDirect3DDevice9_SetRenderTarget(ctx->d3ddev, 0, ctx->backBuffer);

    /* clear the backbuffer to orange */
    IDirect3DDevice9_Clear(ctx->d3ddev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 120, 0), 1.0f, 0);

    IDirect3DDevice9_BeginScene(ctx->d3ddev);
    IDirect3DDevice9_SetTexture(ctx->d3ddev, 0, (IDirect3DBaseTexture9*)ctx->renderTexture);

    IDirect3DDevice9_SetStreamSource(ctx->d3ddev, 0, ctx->rectangleFVFVertexBuf, 0, sizeof(struct CUSTOMVERTEX));
    IDirect3DDevice9_SetFVF(ctx->d3ddev, CUSTOMFVF);
    IDirect3DDevice9_DrawPrimitive(ctx->d3ddev, D3DPT_TRIANGLEFAN, 0, 2);
    IDirect3DDevice9_EndScene(ctx->d3ddev);

    IDirect3DDevice9_Present(ctx->d3ddev, NULL, NULL, ctx->hWnd, NULL);
}

/**
 * Callback called to tell the size of the surface that should be available
 * to VLC to draw into.
 *
 * This is called outside of the UI thread (not the VLC rendering thread).
 */
static bool Resize(struct render_context *ctx, unsigned width, unsigned height,
                   IDirect3DDevice9 *vlc_device,
                   libvlc_video_output_cfg_t *out)
{
    HRESULT hr;
    D3DDISPLAYMODE d3ddm;

    hr = IDirect3D9Ex_GetAdapterDisplayMode(ctx->d3d, 0, &d3ddm);

    /* create the output surface VLC will write to */
    if (ctx->renderTexture)
    {
        IDirect3DTexture9_Release(ctx->renderTexture);
        ctx->renderTexture = NULL;
        ctx->sharedHandled = NULL;
    }
    if (ctx->sharedRenderTexture)
    {
        IDirect3DTexture9_Release(ctx->sharedRenderTexture);
        ctx->sharedRenderTexture = NULL;
    }
    if (ctx->sharedRenderSurface)
    {
        IDirect3DSurface9_Release(ctx->sharedRenderSurface);
        ctx->sharedRenderSurface = NULL;
    }
    /* the device to use may have changed */
    if (ctx->libvlc_d3d)
    {
        IDirect3DDevice9_Release(ctx->libvlc_d3d);
    }
    ctx->libvlc_d3d = vlc_device;
    IDirect3DDevice9_AddRef(ctx->libvlc_d3d);

    /* texture we can use on our device */
    hr = IDirect3DDevice9_CreateTexture(ctx->d3ddev, width, height, 1, D3DUSAGE_RENDERTARGET,
                                        d3ddm.Format,
                                        D3DPOOL_DEFAULT,
                                        &ctx->renderTexture,
                                        &ctx->sharedHandled);
    if (FAILED(hr))
        return false;

    /* texture/surface that is set as the render target for libvlc on its device */
    hr = IDirect3DDevice9_CreateTexture(ctx->libvlc_d3d, width, height, 1, D3DUSAGE_RENDERTARGET,
                                        d3ddm.Format,
                                        D3DPOOL_DEFAULT,
                                        &ctx->sharedRenderTexture,
                                        &ctx->sharedHandled);
    if (FAILED(hr))
        return false;

    hr = IDirect3DTexture9_GetSurfaceLevel(ctx->sharedRenderTexture, 0, &ctx->sharedRenderSurface);
    if (FAILED(hr))
        return false;

    hr = IDirect3DDevice9_SetRenderTarget(ctx->libvlc_d3d, 0, ctx->sharedRenderSurface);
    if (FAILED(hr)) return false;

    out->d3d9_format    = d3ddm.Format;
    out->full_range     = true;
    out->colorspace     = libvlc_video_colorspace_BT709;
    out->primaries      = libvlc_video_primaries_BT709;
    out->transfer       = libvlc_video_transfer_func_SRGB;

    return true;
}

static void init_direct3d(struct render_context *ctx, HWND hWnd)
{
    ctx->hWnd = hWnd;
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &ctx->d3d);

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;

    IDirect3D9Ex_CreateDevice(ctx->d3d, D3DADAPTER_DEFAULT,
                            D3DDEVTYPE_HAL,
                            NULL,
                            D3DCREATE_MULTITHREADED| D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
                            &d3dpp,
                            &ctx->d3ddev);

    IDirect3DDevice9_GetRenderTarget(ctx->d3ddev, 0, &ctx->backBuffer);

    struct CUSTOMVERTEX rectangleVertices[] =
    {
        {  BORDER_LEFT,    BORDER_TOP, 0.0f, 1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 0.0f },
        { BORDER_RIGHT,    BORDER_TOP, 0.0f, 1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0.0f },
        { BORDER_RIGHT, BORDER_BOTTOM, 0.0f, 1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 1.0f },
        {  BORDER_LEFT, BORDER_BOTTOM, 0.0f, 1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 1.0f },
    };
    IDirect3DDevice9Ex_CreateVertexBuffer(ctx->d3ddev, sizeof(rectangleVertices),
                                        D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                        CUSTOMFVF,
                                        D3DPOOL_DEFAULT,
                                        &ctx->rectangleFVFVertexBuf,
                                        NULL);

    LPVOID pVoid;
    IDirect3DVertexBuffer9_Lock(ctx->rectangleFVFVertexBuf, 0, 0, (void**)&pVoid, 0);
    memcpy(pVoid, rectangleVertices, sizeof(rectangleVertices));
    IDirect3DVertexBuffer9_Unlock(ctx->rectangleFVFVertexBuf);
}

static void release_direct3d(struct render_context *ctx)
{
    if (ctx->backBuffer)
        IDirect3DSurface9_Release(ctx->backBuffer);
    if (ctx->renderTexture)
        IDirect3DTexture9_Release(ctx->renderTexture);
    if (ctx->sharedRenderSurface)
        IDirect3DSurface9_Release(ctx->sharedRenderSurface);
    if (ctx->sharedRenderTexture)
        IDirect3DTexture9_Release(ctx->sharedRenderTexture);
    if (ctx->rectangleFVFVertexBuf)
        IDirect3DVertexBuffer9_Release(ctx->rectangleFVFVertexBuf);
    if (ctx->libvlc_d3d)
        IDirect3DDevice9_Release(ctx->libvlc_d3d);
    IDirect3DDevice9_Release(ctx->d3ddev);
    IDirect3D9_Release(ctx->d3d);
}

static bool Setup_cb( void **opaque, const libvlc_video_setup_device_cfg_t *cfg, libvlc_video_setup_device_info_t *out )
{
    struct render_context *ctx = *opaque;
    out->d3d9.device = ctx->d3d;
    out->d3d9.adapter = D3DADAPTER_DEFAULT;
    return true;
}

static void Cleanup_cb( void *opaque )
{
    /* here we can release all things Direct3D9 for good  (if playing only one file) */
    struct render_context *ctx = opaque;
    if (ctx->libvlc_d3d)
    {
        IDirect3DDevice9_Release(ctx->libvlc_d3d);
        ctx->libvlc_d3d = NULL;
    }
}

static void Resize_cb( void *opaque,
                       void (*report_size_change)(void *report_opaque, unsigned width, unsigned height),
                       void *report_opaque )
{
    struct render_context *ctx = opaque;
    EnterCriticalSection(&ctx->sizeLock);
    ctx->ReportSize = report_size_change;
    ctx->ReportOpaque = report_opaque;

    if (ctx->ReportSize != NULL)
    {
        /* report our initial size */
        ctx->ReportSize(ctx->ReportOpaque, ctx->width, ctx->height);
    }
    LeaveCriticalSection(&ctx->sizeLock);
}

static bool UpdateOutput_cb( void *opaque, const libvlc_video_render_cfg_t *cfg, libvlc_video_output_cfg_t *out )
{
    struct render_context *ctx = opaque;
    return Resize(ctx, cfg->width, cfg->height, (IDirect3DDevice9*)cfg->device, out);
}

static void Swap_cb( void* opaque )
{
    struct render_context *ctx = opaque;
    Swap( ctx );
}

/**
 * Callback called just before VLC starts/finishes drawing the video.
 *
 * Set the surface VLC will render to (could be the backbuffer if nothing else
 * needs to be displayed). And then call BeginScene().
 *
 * This is called outside of the UI thread (in the VLC rendering thread).
 */
static bool StartRendering_cb( void *opaque, bool enter )
{
    struct render_context *ctx = opaque;
    if ( enter )
    {
        /* we already set the RenderTarget on the IDirect3DDevice9 */
        return true;
    }

    /* VLC has finished preparing drawning on our surface, we need do the drawing now
       so the surface is finished rendering when Swap() is called to do our own
       rendering */
    IDirect3DDevice9_Present(ctx->libvlc_d3d, NULL, NULL, NULL, NULL);
    return true;
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
            ctx->width  = (BORDER_RIGHT - BORDER_LEFT) * LOWORD(lParam) / SCREEN_WIDTH; /* remove the orange part ! */
            ctx->height = (BORDER_BOTTOM - BORDER_TOP) * HIWORD(lParam) / SCREEN_HEIGHT;
            EnterCriticalSection(&ctx->sizeLock);
            if (ctx->ReportSize != NULL)
                ctx->ReportSize(ctx->ReportOpaque, ctx->width, ctx->height);
            LeaveCriticalSection(&ctx->sizeLock);
        }
        break;

        case WM_DESTROY:
            {
                PostQuitMessage(0);
                return 0;
            } break;
    }

    return DefWindowProc (hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    HWND hWnd;
    WNDCLASSEX wc;
    struct render_context Context = { 0 };
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
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = "WindowClass";

    RegisterClassEx(&wc);

    RECT wr = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    hWnd = CreateWindowEx(0,
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

    ShowWindow(hWnd, nCmdShow);

    init_direct3d(&Context, hWnd);

    // DON'T use with callbacks libvlc_media_player_set_hwnd(p_mp, hWnd);

    /* Tell VLC to render into our D3D9 environment */
    libvlc_video_set_output_callbacks( p_mp, libvlc_video_engine_d3d9,
                                       Setup_cb, Cleanup_cb, Resize_cb, UpdateOutput_cb, Swap_cb, StartRendering_cb,
                                       NULL, NULL, NULL,
                                       &Context );

    libvlc_media_player_play( p_mp );

    MSG msg;

    while(TRUE)
    {
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if(msg.message == WM_QUIT)
            break;
    }

    libvlc_media_player_stop_async( p_mp );

    libvlc_media_player_release( p_mp );
    libvlc_media_release( p_media );
    libvlc_release( p_libvlc );

    DeleteCriticalSection(&Context.sizeLock);
    release_direct3d(&Context);

    return msg.wParam;
}
