/* compile: g++ d3d11_swapr.cpp -o d3d11_swapr.exe -L<path/libvlc> -lvlc -ld3d11 -ld3dcompiler -luuid */

/* This is a basic sample app using a SwapChain hosted in the app and passed to
   libvlc via the command line parameters. This is the legacy mode used by
   UWP apps.

   The swapchain size changes are signaled to libvlc by updating the swapchain
   variables GUID_SWAPCHAIN_WIDTH and GUID_SWAPCHAIN_HEIGHT.
*/

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <d3d11_1.h>
#include <dxgi1_2.h>

#ifdef DEBUG_D3D11_LEAKS
# include <initguid.h>
# include <dxgidebug.h>
#endif

#include <cassert>
#include <string>
#include <sstream>

#ifdef _MSC_VER
typedef int ssize_t;
#endif

#include <vlc/vlc.h>

#define INITIAL_WIDTH  1500
#define INITIAL_HEIGHT  900

#define check_leak(x)  assert(x)

#include <initguid.h>
// updates to the swapchain width/height need to be set a private data of the shared IDXGISwapChain object
DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

// if the host app is using the ID3D11DeviceContext, it should use a Mutext to access it and share it
// as a private data of the shared ID3D11DeviceContext object
DEFINE_GUID(GUID_CONTEXT_MUTEX, 0x472e8835, 0x3f8e, 0x4f93, 0xa0, 0xcb, 0x25, 0x79, 0x77, 0x6c, 0xed, 0x86);


struct render_context
{
    HWND hWnd = 0;

    libvlc_media_player_t *p_mp = nullptr;

    /* Direct3D11 device/context */
    ID3D11Device        *d3device = nullptr;
    ID3D11DeviceContext *d3dctx = nullptr;
    HANDLE              d3dctx_mutex = INVALID_HANDLE_VALUE;

    IDXGISwapChain      *swapchain = nullptr;

    unsigned width, height;
};

static void init_direct3d(struct render_context *ctx)
{
    HRESULT hr;
    DXGI_SWAP_CHAIN_DESC scd = { };

    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = ctx->width;
    scd.BufferDesc.Height = ctx->height;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = ctx->hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT creationFlags = 0;
#ifndef NDEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ctx->d3dctx_mutex = CreateMutexEx( NULL, NULL, 0, SYNCHRONIZE );

    D3D11CreateDeviceAndSwapChain(NULL,
                                  D3D_DRIVER_TYPE_HARDWARE,
                                  NULL,
                                  creationFlags,
                                  NULL,
                                  0,
                                  D3D11_SDK_VERSION,
                                  &scd,
                                  &ctx->swapchain,
                                  &ctx->d3device,
                                  NULL,
                                  &ctx->d3dctx);

    ctx->d3dctx->SetPrivateData(GUID_CONTEXT_MUTEX,  sizeof(ctx->d3dctx_mutex), &ctx->d3dctx_mutex);

    uint32_t i_width = scd.BufferDesc.Width;
    uint32_t i_height = scd.BufferDesc.Height;
    ctx->swapchain->SetPrivateData(GUID_SWAPCHAIN_WIDTH,  sizeof(i_width),  &i_width);
    ctx->swapchain->SetPrivateData(GUID_SWAPCHAIN_HEIGHT, sizeof(i_height), &i_height);

    /* The ID3D11Device must have multithread protection */
    ID3D10Multithread *pMultithread;
    hr = ctx->d3device->QueryInterface( __uuidof(ID3D10Multithread), (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        pMultithread->SetMultithreadProtected(TRUE);
        pMultithread->Release();
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

    ref = ctx->swapchain->Release();
    check_leak(ref == 0);
    ref = ctx->d3dctx->Release();
    check_leak(ref == 0);
    ref = ctx->d3device->Release();
    check_leak(ref == 0);

    CloseHandle(ctx->d3dctx_mutex);

    list_dxgi_leaks();
}

static const char *AspectRatio = NULL;

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
            ctx->width  = LOWORD(lParam);
            ctx->height = HIWORD(lParam);

            // update the swapchain to match our window client area
            if (ctx->swapchain != nullptr)
            {
                uint32_t i_width  = ctx->width;
                uint32_t i_height = ctx->height;
                ctx->swapchain->SetPrivateData(GUID_SWAPCHAIN_WIDTH,  sizeof(i_width),  &i_width);
                ctx->swapchain->SetPrivateData(GUID_SWAPCHAIN_HEIGHT, sizeof(i_height), &i_height);

                D3D11_VIEWPORT viewport = { 0, 0, (FLOAT)ctx->width, (FLOAT)ctx->height, 0, 0 };
                WaitForSingleObjectEx( ctx->d3dctx_mutex, INFINITE, FALSE );
                // This call is not necessary but show how to use the shared mutex
                // when accessing the shared ID3D11DeviceContext object
                ctx->d3dctx->RSSetViewports(1, &viewport);
                ReleaseMutex( ctx->d3dctx_mutex );
            }
        }
        break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            {
                int key = tolower( MapVirtualKey( (UINT)wParam, 2 ) );
                if (key == 'a')
                {
                    if (AspectRatio == NULL)
                        AspectRatio = "16:10";
                    else if (strcmp(AspectRatio,"16:10")==0)
                        AspectRatio = "16:9";
                    else if (strcmp(AspectRatio,"16:9")==0)
                        AspectRatio = "4:3";
                    else if (strcmp(AspectRatio,"4:3")==0)
                        AspectRatio = "185:100";
                    else if (strcmp(AspectRatio,"185:100")==0)
                        AspectRatio = "221:100";
                    else if (strcmp(AspectRatio,"221:100")==0)
                        AspectRatio = "235:100";
                    else if (strcmp(AspectRatio,"235:100")==0)
                        AspectRatio = "239:100";
                    else if (strcmp(AspectRatio,"239:100")==0)
                        AspectRatio = "5:3";
                    else if (strcmp(AspectRatio,"5:3")==0)
                        AspectRatio = "5:4";
                    else if (strcmp(AspectRatio,"5:4")==0)
                        AspectRatio = "1:1";
                    else if (strcmp(AspectRatio,"1:1")==0)
                        AspectRatio = NULL;
                    libvlc_video_set_aspect_ratio( ctx->p_mp, AspectRatio );
                }
                break;
            }
        default: break;
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
    (void)hPrevInstance;

    /* remove "" around the given path */
    if (lpCmdLine[0] == '"')
    {
        file_path = _strdup( lpCmdLine+1 );
        if (file_path[strlen(file_path)-1] == '"')
            file_path[strlen(file_path)-1] = '\0';
    }
    else
        file_path = _strdup( lpCmdLine );


    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "WindowClass";

    RegisterClassEx(&wc);

    RECT wr = {0, 0, INITIAL_WIDTH, INITIAL_HEIGHT};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    Context.width  = wr.right - wr.left;
    Context.height = wr.bottom - wr.top;

    Context.hWnd = CreateWindowEx(0,
                          "WindowClass",
                          "libvlc external swapchain demo",
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          Context.width,
                          Context.height,
                          NULL,
                          NULL,
                          hInstance,
                          &Context);

    ShowWindow(Context.hWnd, nCmdShow);

    init_direct3d(&Context);

    std::stringstream winrt_ctx_ss;
    winrt_ctx_ss << "--winrt-d3dcontext=0x";
    winrt_ctx_ss << std::hex << (intptr_t)Context.d3dctx;
    std::string winrt_ctx  = winrt_ctx_ss.str();
    std::stringstream winrt_swap_ss;
    winrt_swap_ss << "--winrt-swapchain=0x";
    winrt_swap_ss << std::hex << (intptr_t)Context.swapchain;
    std::string winrt_swap = winrt_swap_ss.str();
    const char *params [] = {
        winrt_ctx.c_str(),
        winrt_swap.c_str(),
    };

    p_libvlc = libvlc_new( sizeof(params)/sizeof(params[0]), params );
    p_media = libvlc_media_new_path( p_libvlc, file_path );
    free( file_path );
    Context.p_mp = libvlc_media_player_new_from_media( p_media );


    // DON'T use with callbacks libvlc_media_player_set_hwnd(p_mp, hWnd);

    libvlc_media_player_play( Context.p_mp );

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

    libvlc_media_player_stop( Context.p_mp );

    libvlc_media_player_release( Context.p_mp );
    libvlc_media_release( p_media );

    release_direct3d(&Context);

    libvlc_release( p_libvlc );

    return (int)msg.wParam;
}
