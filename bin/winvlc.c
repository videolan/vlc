/*****************************************************************************
 * winvlc.c: the Windows VLC media player
 *****************************************************************************
 * Copyright (C) 1998-2011 the VideoLAN team
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
#include <windows.h>

#if !defined(UNDER_CE)
# ifndef _WIN32_IE
#   define  _WIN32_IE 0x501
# endif
# include <fcntl.h>
# include <io.h>
# include <shlobj.h>
# include <wininet.h>
# define PSAPI_VERSION 1
# include <psapi.h>
# define HeapEnableTerminationOnCorruption (HEAP_INFORMATION_CLASS)1
static void check_crashdump(void);
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo);
static const wchar_t *crashdump_path;
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
    int argc;

#ifndef UNDER_CE
#ifdef TOP_BUILDDIR
    putenv("VLC_PLUGIN_PATH=Z:"TOP_BUILDDIR"/modules");
#endif

    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    /* SetProcessDEPPolicy */
    HINSTANCE h_Kernel32 = LoadLibraryW(L"kernel32.dll");
    if(h_Kernel32)
    {
        BOOL (WINAPI * mySetProcessDEPPolicy)( DWORD dwFlags);
        BOOL (WINAPI * mySetDllDirectoryA)(const char* lpPathName);
# define PROCESS_DEP_ENABLE 1

        mySetProcessDEPPolicy = (BOOL WINAPI (*)(DWORD))
                            GetProcAddress(h_Kernel32, "SetProcessDEPPolicy");
        if(mySetProcessDEPPolicy)
            mySetProcessDEPPolicy(PROCESS_DEP_ENABLE);

        /* Do NOT load any library from cwd. */
        mySetDllDirectoryA = (BOOL WINAPI (*)(const char*)) GetProcAddress(h_Kernel32, "SetDllDirectoryA");
        if(mySetDllDirectoryA)
            mySetDllDirectoryA("");

        FreeLibrary(h_Kernel32);
    }

    /* Args */
    wchar_t **wargv = CommandLineToArgvW (GetCommandLine (), &argc);
    if (wargv == NULL)
        return 1;

    char *argv[argc + 3];
    BOOL crash_handling = TRUE;
    int j = 0;

    argv[j++] = FromWide( L"--media-library" );
    argv[j++] = FromWide( L"--no-ignore-config" );
#ifdef TOP_SRCDIR
    argv[j++] = FromWide (L"--data-path=Z:"TOP_SRCDIR"/share");
#endif
    for (int i = 1; i < argc; i++)
    {
        if(!wcscmp(wargv[i], L"--no-crashdump"))
        {
            crash_handling = FALSE;
            continue; /* don't give argument to libvlc */
        }

        argv[j++] = FromWide (wargv[i]);
    }

    argc = j;
    argv[argc] = NULL;
    LocalFree (wargv);

    if(crash_handling)
    {
        static wchar_t path[MAX_PATH];
        if( S_OK != SHGetFolderPathW( NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                    NULL, SHGFP_TYPE_CURRENT, path ) )
            fprintf( stderr, "Can't open the vlc conf PATH\n" );
        swprintf( path+wcslen( path ), L"%s", L"\\vlc\\crashdump" );
        crashdump_path = &path[0];

        check_crashdump();
        SetUnhandledExceptionFilter(vlc_exception_filter);
    }

    _setmode( _fileno( stdin ), _O_BINARY ); /* Needed for pipes */

#else /* UNDER_CE */
    char **argv, psz_cmdline[wcslen(lpCmdLine) * 4];

    WideCharToMultiByte( CP_UTF8, 0, lpCmdLine, -1,
                         psz_cmdline, sizeof (psz_cmdline), NULL, NULL );

    argc = parse_cmdline (psz_cmdline, &argv);
#endif

    /* Initialize libvlc */
    libvlc_instance_t *vlc;
    vlc = libvlc_new (argc, (const char **)argv);
    if (vlc != NULL)
    {
        libvlc_add_intf (vlc, "globalhotkeys,none");
        libvlc_add_intf (vlc, NULL);
        libvlc_playlist_play (vlc, -1, 0, NULL);
        libvlc_wait (vlc);
        libvlc_release (vlc);
    }

    for (int i = 0; i < argc; i++)
        free (argv[i]);

    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return 0;
}

#if !defined( UNDER_CE )
/* Crashdumps handling */
static void check_crashdump(void)
{
    FILE * fd = _wfopen ( crashdump_path, L"r, ccs=UTF-8" );
    if( !fd )
        return;
    fclose( fd );

    int answer = MessageBox( NULL, L"VLC media player just crashed." \
    " Do you want to send a bug report to the developers team?",
    L"VLC crash reporting", MB_YESNO);

    if(answer == IDYES)
    {
        HINTERNET Hint = InternetOpen(L"VLC Crash Reporter", INTERNET_OPEN_TYPE_PRECONFIG, NULL,NULL,0);
        if(Hint)
        {
            HINTERNET ftp = InternetConnect(Hint, L"crash.videolan.org", INTERNET_DEFAULT_FTP_PORT,
                                            NULL, NULL, INTERNET_SERVICE_FTP, 0, 0);
            if(ftp)
            {
                SYSTEMTIME now;
                GetSystemTime(&now);
                wchar_t remote_file[MAX_PATH];
                swprintf( remote_file, L"/crashes-win32/%04d%02d%02d%02d%02d%02d",
                          now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond );

                if( FtpPutFile( ftp, crashdump_path, remote_file, FTP_TRANSFER_TYPE_BINARY, 0) )
                    MessageBox( NULL, L"Report sent correctly. Thanks a lot for the help.",
                                L"Report sent", MB_OK);
                else
                    MessageBox( NULL, L"There was an error while transferring to the FTP server. "\
                                "Thanks a lot for the help anyway.",
                                L"Report sending failed", MB_OK);
                InternetCloseHandle(ftp);
            }
            else
            {
                MessageBox( NULL, L"There was an error while connecting to the FTP server. "\
                                "Thanks a lot for the help anyway.",
                                L"Report sending failed", MB_OK);
                fprintf(stderr,"Can't connect to FTP server 0x%08lu\n",
                        (unsigned long)GetLastError());
            }
            InternetCloseHandle(Hint);
        }
        else
        {
              MessageBox( NULL, L"There was an error while connecting to Internet. "\
                                "Thanks a lot for the help anyway.",
                                L"Report sending failed", MB_OK);
        }
    }

    _wremove(crashdump_path);
}

/*****************************************************************************
 * vlc_exception_filter: handles unhandled exceptions, like segfaults
 *****************************************************************************/
LONG WINAPI vlc_exception_filter(struct _EXCEPTION_POINTERS *lpExceptionInfo)
{
    if(IsDebuggerPresent())
    {
        //If a debugger is present, pass the exception to the debugger with EXCEPTION_CONTINUE_SEARCH
        return EXCEPTION_CONTINUE_SEARCH;
    }
    else
    {
        fprintf( stderr, "unhandled vlc exception\n" );

        FILE * fd = _wfopen ( crashdump_path, L"w, ccs=UTF-8" );

        if( !fd )
        {
            fprintf( stderr, "\nerror while opening file" );
            exit( 1 );
        }

        OSVERSIONINFO osvi;
        ZeroMemory( &osvi, sizeof(OSVERSIONINFO) );
        osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
        GetVersionEx( &osvi );

        fwprintf( fd, L"[version]\nOS=%d.%d.%d.%d.%s\nVLC=" VERSION_MESSAGE, osvi.dwMajorVersion,
                                                               osvi.dwMinorVersion,
                                                               osvi.dwBuildNumber,
                                                               osvi.dwPlatformId,
                                                               osvi.szCSDVersion);

        const CONTEXT *const pContext = (const CONTEXT *)lpExceptionInfo->ContextRecord;
        const EXCEPTION_RECORD *const pException = (const EXCEPTION_RECORD *)lpExceptionInfo->ExceptionRecord;
        /*No nested exceptions for now*/
        fwprintf( fd, L"\n\n[exceptions]\n%08x at %px",pException->ExceptionCode,
                                                pException->ExceptionAddress );

        for( unsigned int i = 0; i < pException->NumberParameters; i++ )
            fwprintf( fd, L" | %p", pException->ExceptionInformation[i] );

#ifdef WIN64
        fwprintf( fd, L"\n\n[context]\nRDI:%px\nRSI:%px\n" \
                    "RBX:%px\nRDX:%px\nRCX:%px\nRAX:%px\n" \
                    "RBP:%px\nRIP:%px\nRSP:%px\nR8:%px\n" \
                    "R9:%px\nR10:%px\nR11:%px\nR12:%px\n" \
                    "R13:%px\nR14:%px\nR15:%px\n",
                        pContext->Rdi,pContext->Rsi,pContext->Rbx,
                        pContext->Rdx,pContext->Rcx,pContext->Rax,
                        pContext->Rbp,pContext->Rip,pContext->Rsp,
                        pContext->R8,pContext->R9,pContext->R10,
                        pContext->R11,pContext->R12,pContext->R13,
                        pContext->R14,pContext->R15 );
#else
        fwprintf( fd, L"\n\n[context]\nEDI:%px\nESI:%px\n" \
                    "EBX:%px\nEDX:%px\nECX:%px\nEAX:%px\n" \
                    "EBP:%px\nEIP:%px\nESP:%px\n",
                        pContext->Edi,pContext->Esi,pContext->Ebx,
                        pContext->Edx,pContext->Ecx,pContext->Eax,
                        pContext->Ebp,pContext->Eip,pContext->Esp );
#endif

        HANDLE hpid = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                        FALSE, GetCurrentProcessId());
        if (hpid) {
            HMODULE mods[1024];
            DWORD size;
            if (EnumProcessModules(hpid, mods, sizeof(mods), &size)) {
                fwprintf( fd, L"\n\n[modules]\n" );
                for (unsigned int i = 0; i < size / sizeof(HMODULE); i++) {
                    wchar_t module[ 256 ];
                    GetModuleFileName(mods[i], module, 256);
                    fwprintf( fd, L"%p|%s\n", mods[i], module);
                }
            }
            CloseHandle(hpid);
        }


        fwprintf( fd, L"\n[stacktrace]\n#EIP|base|module\n" );

#ifdef WIN64
        LPCVOID caller = (LPCVOID)pContext->Rip;
        LPVOID *pBase  = (LPVOID*)pContext->Rbp;
#else
        LPVOID *pBase  = (LPVOID*)pContext->Ebp;
        LPCVOID caller = (LPCVOID)pContext->Eip;
#endif
        for( unsigned frame = 0; frame <= 100; frame++ )
        {
            MEMORY_BASIC_INFORMATION mbi;
            wchar_t module[ 256 ];
            VirtualQuery( caller, &mbi, sizeof( mbi ) ) ;
            GetModuleFileName( mbi.AllocationBase, module, 256 );
            fwprintf( fd, L"%p|%s\n", caller, module );

            /*The last BP points to NULL!*/
            caller = *(pBase + 1);
            if( !caller )
                break;
            pBase = *pBase;
        }

        fclose( fd );
        fflush( stderr );
        exit( 1 );
    }
}
#endif
