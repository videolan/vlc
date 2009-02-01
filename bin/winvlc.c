/*****************************************************************************
 * winvlc.c: the Windows VLC player
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Lots of other people, see the libvlc AUTHORS file
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define UNICODE
#include <vlc/vlc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#if !defined(UNDER_CE) && defined ( NDEBUG )
#   define  _WIN32_IE 0x500
#   include <shlobj.h>
#   include <tlhelp32.h>
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo);
#endif

#ifndef UNDER_CE
static char *FromWide (const wchar_t *wide)
{
    size_t len;
    len = WideCharToMultiByte (CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);

    char *out = (char *)malloc (len);
    if (out)
        WideCharToMultiByte (CP_UTF8, 0, wide, -1, out, len, NULL, NULL);
    return out;
}
#else
static int parse_cmdline (char *line, char ***argvp)
{
    char **argv = malloc (sizeof (char *));
    int argc = 0;

    while (*line != '\0')
    {
        char quote = 0;

        /* Skips white spaces */
        while (strchr ("\t ", *line))
            line++;
        if (!*line)
            break;

        /* Starts a new parameter */
        argv = realloc (argv, (argc + 2) * sizeof (char *));
        if (*line == '"')
        {
            quote = '"';
            line++;
        }
        argv[argc++] = line;

    more:
            while (*line && !strchr ("\t ", *line))
            line++;

    if (line > argv[argc - 1] && line[-1] == quote)
        /* End of quoted parameter */
        line[-1] = 0;
    else
        if (*line && quote)
    {
        /* Space within a quote */
        line++;
        goto more;
    }
    else
        /* End of unquoted parameter */
        if (*line)
            *line++ = 0;
    }
    argv[argc] = NULL;
    *argvp = argv;
    return argc;
}
#endif

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
#ifndef UNDER_CE
                    LPSTR lpCmdLine,
#else
                    LPWSTR lpCmdLine,
#endif
                    int nCmdShow )
{
    int argc, ret;
#ifndef UNDER_CE
    wchar_t **wargv = CommandLineToArgvW (GetCommandLine (), &argc);
    if (wargv == NULL)
        return 1;

    char *argv[argc + 1];
    for (int i = 0; i < argc; i++)
        argv[i] = FromWide (wargv[i]);
    argv[argc] = NULL;
    LocalFree (wargv);
#else
    char **argv, psz_cmdline[wcslen(lpCmdLine) * 4];

    WideCharToMultiByte( CP_UTF8, 0, lpCmdLine, -1,
                         psz_cmdline, sizeof (psz_cmdline), NULL, NULL );

    argc = parse_cmdline (psz_cmdline, &argv);
#endif

    libvlc_exception_t ex, dummy;
    libvlc_exception_init (&ex);
    libvlc_exception_init (&dummy);

#if !defined( UNDER_CE ) && defined ( NDEBUG )
    SetUnhandledExceptionFilter(vlc_exception_filter);
#endif

    /* Initialize libvlc */
    libvlc_instance_t *vlc;
    vlc = libvlc_new (argc - 1, (const char **)argv + 1, &ex);
    if (vlc != NULL)
    {
        libvlc_add_intf (vlc, NULL, &ex);
        libvlc_playlist_play (vlc, -1, 0, NULL, &dummy);
        libvlc_wait (vlc);
        libvlc_release (vlc);
    }

    ret = libvlc_exception_raised (&ex);
    libvlc_exception_clear (&ex);
    libvlc_exception_clear (&dummy);

    for (int i = 0; i < argc; i++)
        free (argv[i]);

    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return ret;
}

#if !defined( UNDER_CE ) && defined ( NDEBUG )
/*****************************************************************************
 * vlc_exception_filter: handles unhandled exceptions, like segfaults
 *****************************************************************************/
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo)
{
    wchar_t wdir[MAX_PATH];

    fprintf( stderr, "unhandled vlc exception\n" );

    if( S_OK != SHGetFolderPathW( NULL,
                         CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                  NULL, SHGFP_TYPE_CURRENT, wdir ) )
            fprintf( stderr, "Can't open the vlc conf PATH\n" );

    swprintf( wdir+wcslen( wdir ), L"%s", L"\\vlc\\crashdump" );

    FILE * fd = _wfopen ( wdir, L"w, ccs=UTF-8" );

    if( !fd )
        fprintf( stderr, "\nerror while opening file" );

    OSVERSIONINFO osvi;
    ZeroMemory( &osvi, sizeof(OSVERSIONINFO) );
    osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
    GetVersionEx( &osvi );

    fwprintf( fd, L"[Version]\n0S=%d.%d.%d.%d.%s\nVLC=%s", osvi.dwMajorVersion,
                                                           osvi.dwMinorVersion,
                                                           osvi.dwBuildNumber,
                                                           osvi.dwPlatformId,
                                                           osvi.szCSDVersion,
                                                           VERSION_MESSAGE);

    const CONTEXT *const pContext = (const CONTEXT *)lpExceptionInfo->ContextRecord;
    const EXCEPTION_RECORD *const pException = (const EXCEPTION_RECORD *)lpExceptionInfo->ExceptionRecord;
    /*No nested exceptions for now*/
    fwprintf( fd, L"\n\n[Exceptions]\n%08x at %08x",pException->ExceptionCode,
                                            pException->ExceptionAddress );
    if( pException->NumberParameters > 0 )
    {
        unsigned int i;
        for( i = 0; i < pException->NumberParameters; i++ )
            fprintf( fd, " | %08x", pException->ExceptionInformation[i] );
    }

    fwprintf( fd, L"\n\n[CONTEXT]\nEDI:%08x\nESI:%08x\n" \
                "EBX:%08x\nEDX:%08xn\nECX:%08x\nEAX:%08x\n" \
                "EBP:%08x\nEIP:%08x\nESP:%08x\n",
                    pContext->Edi,pContext->Esi,pContext->Ebx,
                    pContext->Edx,pContext->Ecx,pContext->Eax,
                    pContext->Ebp,pContext->Eip,pContext->Esp );

    fwprintf( fd, L"\n\n[STACKTRACE]\n#EIP|base|module\n" );

    DWORD pEbp = pContext->Ebp;
    DWORD caller = *((DWORD*)pEbp + 1) ;

    wchar_t module[ 256 ];

    do
    {
        MEMORY_BASIC_INFORMATION mbi ;
        VirtualQuery( (DWORD *)caller, &mbi, sizeof( mbi ) ) ;
        HINSTANCE hInstance = mbi.AllocationBase;
        GetModuleFileName( hInstance, module, 256 ) ;
        fwprintf( fd, L"%08x|%08x|%s\n", caller, hInstance, module );
        pEbp = *(DWORD*)pEbp ;
        caller = *((DWORD*)pEbp + 1) ;
        /*The last EBP points to NULL!*/
    }while(caller);

    fclose( fd );
    fflush( stderr );
    exit( 1 );
}
#endif
