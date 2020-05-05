/* compile: cc win_player.c -o win_player.exe -L<path/libvlc> -lvlc */

#include <windows.h>
#include <assert.h>

#include <vlc/vlc.h>

#define SCREEN_WIDTH  1500
#define SCREEN_HEIGHT  900

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
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
    char *file_path;
    libvlc_instance_t *p_libvlc;
    libvlc_media_t *p_media;
    libvlc_media_player_t *p_mp;
    (void)hPrevInstance;
    HWND hWnd;

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
                          NULL);

    libvlc_media_player_set_hwnd(p_mp, hWnd);

    ShowWindow(hWnd, nCmdShow);

    libvlc_media_player_play( p_mp );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if(msg.message == WM_QUIT)
            break;
    }

    libvlc_media_player_stop( p_mp );

    libvlc_media_player_release( p_mp );
    libvlc_media_release( p_media );
    libvlc_release( p_libvlc );

    return msg.wParam;
}
