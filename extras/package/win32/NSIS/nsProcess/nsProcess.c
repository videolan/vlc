/*********************************************************************
 *               nsProcess NSIS plugin v1.5                          *
 *                                                                   *
 * 2006 Shengalts Aleksander aka Instructor (Shengalts@mail.ru)      *
 *                                                                   *
 * Licensed under the zlib/libpng license (the "License");           *
 * you may not use this file except in compliance with the License.  *
 *                                                                   *
 * Licence details can be found in the file COPYING.                 *
 *                                                                   *
 * This software is provided 'as-is', without any express or implied *
 * warranty.                                                         *
 *                                                                   *
 * Source function FIND_PROC_BY_NAME based                           *
 *   upon the Ravi Kochhar (kochhar@physiology.wisc.edu) code        *
 * Thanks iceman_k (FindProcDLL plugin) and                          *
 *   DITMan (KillProcDLL plugin) for point me up                     *
 *********************************************************************/


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include "pluginapi.h"

/* Defines */
#define NSIS_MAX_STRLEN 1024


/* Global variables */
TCHAR szBuf[NSIS_MAX_STRLEN];

/* Funtions prototypes and macros */
int FIND_PROC_BY_NAME(TCHAR *szProcessName, BOOL bTerminate, BOOL bClose);

/* NSIS functions code */
void __declspec(dllexport) _FindProcess(HWND hwndParent, int string_size,
                                      TCHAR *variables, stack_t **stacktop, extra_parameters *extra)
{
  EXDLL_INIT();
  {
    int nError;

    popstringn(szBuf, NSIS_MAX_STRLEN);
    nError=FIND_PROC_BY_NAME(szBuf, FALSE, FALSE);
    pushint(nError);
  }
}

void __declspec(dllexport) _KillProcess(HWND hwndParent, int string_size,
                                      TCHAR *variables, stack_t **stacktop, extra_parameters *extra)
{
  EXDLL_INIT();
  {
    int nError=0;

    popstringn(szBuf, NSIS_MAX_STRLEN);
    nError=FIND_PROC_BY_NAME(szBuf, TRUE, FALSE);
    pushint(nError);
  }
}

void __declspec(dllexport) _CloseProcess(HWND hwndParent, int string_size,
                                      TCHAR *variables, stack_t **stacktop, extra_parameters *extra)
{
  EXDLL_INIT();
  {
    int nError=0;

    popstringn(szBuf, NSIS_MAX_STRLEN);
    nError=FIND_PROC_BY_NAME(szBuf, TRUE, TRUE);
    pushint(nError);
  }
}

void __declspec(dllexport) _Unload(HWND hwndParent, int string_size,
                                      TCHAR *variables, stack_t **stacktop, extra_parameters *extra)
{
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  return TRUE;
}

struct win_id
{
    DWORD proc_id;
    HWND prev_hwnd;
};

BOOL CALLBACK EnumWindowsProc(          HWND hwnd,
    LPARAM lParam
)
{
	struct win_id *data = (struct win_id *)lParam;
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == data->proc_id)
	{
		PostMessage(data->prev_hwnd, WM_CLOSE, 0, 0);
		data->prev_hwnd = hwnd;
	}
	return TRUE;
}

void NiceTerminate(DWORD id, BOOL bClose, BOOL *bSuccess, BOOL *bFailed)
{
  HANDLE hProc;
  DWORD ec;
  BOOL bDone = FALSE;
  if ((hProc=OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, id)) != NULL)
  {
	struct win_id window = { id, NULL };

	if (bClose)
		EnumWindows(EnumWindowsProc, (LPARAM)&window);
	if (window.prev_hwnd != NULL)
	{
	  if (GetExitCodeProcess(hProc,&ec) && ec == STILL_ACTIVE)
		if (WaitForSingleObject(hProc, 3000) == WAIT_OBJECT_0)
		{
		  *bSuccess = bDone = TRUE;
		}
		else;
	  else
	  {
		  *bSuccess = bDone = TRUE;
	  }
	}
	if (!bDone)
	{
            // Open for termination
              if (TerminateProcess(hProc, 0))
                *bSuccess=TRUE;
              else
                *bFailed=TRUE;
	}
    CloseHandle(hProc);
  }
}

int FIND_PROC_BY_NAME(TCHAR *szProcessName, BOOL bTerminate, BOOL bClose)
// Find the process "szProcessName" if it is currently running.
// This works for Win95/98/ME and also WinNT/2000/XP.
// The process name is case-insensitive, i.e. "notepad.exe" and "NOTEPAD.EXE"
// will both work. If bTerminate is TRUE, then process will be terminated.
//
// Return codes are as follows:
//   0   = Success
//   601 = No permission to terminate process
//   602 = Not all processes terminated successfully
//   603 = Process was not currently running
//   604 = Unable to identify system type
//   605 = Unsupported OS
//   606 = Unable to load NTDLL.DLL
//   607 = Unable to get procedure address from NTDLL.DLL
//   608 = NtQuerySystemInformation failed
//   609 = Unable to load KERNEL32.DLL
//   610 = Unable to get procedure address from KERNEL32.DLL
//   611 = CreateToolhelp32Snapshot failed
//
// Change history:
//   created  06/23/2000  - Ravi Kochhar (kochhar@physiology.wisc.edu)
//                            http://www.neurophys.wisc.edu/ravi/software/
//   modified 03/08/2002  - Ravi Kochhar (kochhar@physiology.wisc.edu)
//                          - Borland-C compatible if BORLANDC is defined as
//                            suggested by Bob Christensen
//   modified 03/10/2002  - Ravi Kochhar (kochhar@physiology.wisc.edu)
//                          - Removed memory leaks as suggested by
//                            Jonathan Richard-Brochu (handles to Proc and Snapshot
//                            were not getting closed properly in some cases)
//   modified 14/11/2005  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Combine functions FIND_PROC_BY_NAME and KILL_PROC_BY_NAME
//                          - Code has been optimized
//                          - Now kill all processes with specified name (not only one)
//                          - Cosmetic improvements
//                          - Removed error 632 (Invalid process name)
//                          - Changed error 602 (Unable to terminate process for some other reason)
//                          - BORLANDC define not needed
//   modified 04/01/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed CRT dependency
//   modified 21/04/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed memory leak as suggested by {_trueparuex^}
//                            (handle to hSnapShot was not getting closed properly in some cases)
//   modified 21/04/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed memory leak as suggested by {_trueparuex^}
//                            (handle to hSnapShot was not getting closed properly in some cases)
//   modified 19/07/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Code for WinNT/2000/XP has been rewritten
//                          - Changed error codes
//   modified 31/08/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed memory leak as suggested by Daniel Vanesse
{
  TCHAR szName[MAX_PATH];
  OSVERSIONINFO osvi;
  HANDLE hProc;
  ULONG uError;
  BOOL bFound=FALSE;
  BOOL bSuccess=FALSE;
  BOOL bFailed=FALSE;

  // First check what version of Windows we're in
  osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
  if (!GetVersionEx(&osvi)) return 604;

  if (osvi.dwPlatformId != VER_PLATFORM_WIN32_NT &&
      osvi.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS)
    return 605;

  size_t process_count = 512;
  DWORD *processIDs = NULL;
  for (;;)
  {
    processIDs = realloc(processIDs, process_count * sizeof(DWORD));
    if (processIDs == NULL)
        break;
    DWORD readSize;
    if (!EnumProcesses(processIDs, process_count*sizeof(DWORD), &readSize))
    {
        free(processIDs);
        processIDs = NULL;
        break;
    }
    if (readSize < process_count * sizeof(DWORD))
    {
        process_count = readSize / sizeof(DWORD);
        break;
    }

    // there might be more processes
    process_count *= 2;
  }
  if (processIDs != NULL)
  {
    const size_t cmpsize = lstrlen(szProcessName);
    for (size_t i=0; i<process_count; i++)
    {
      hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processIDs[i]);
      if (hProc != NULL)
      {
        DWORD proclen = sizeof(szName);
        BOOL got = QueryFullProcessImageName(hProc, PROCESS_NAME_NATIVE, szName, &proclen);
        CloseHandle(hProc);
        if (got != FALSE)
        {

          if (proclen < cmpsize + 1)
          {
            continue;
          }
          if (szName[proclen - cmpsize - 1] == TEXT('\\') &&
              !lstrcmpi(szProcessName, &szName[proclen - cmpsize]))
          {
            // Process found
            bFound=TRUE;

            if (bTerminate == TRUE)
            {
              NiceTerminate(processIDs[i], bClose, &bSuccess, &bFailed);
            }
            else break;
          }
        }
      }
    }
    free(processIDs);
  }

  if (bFound == FALSE) return 603;
  if (bTerminate == TRUE)
  {
    if (bSuccess == FALSE) return 601;
    if (bFailed == TRUE) return 602;
  }
  return 0;
}
