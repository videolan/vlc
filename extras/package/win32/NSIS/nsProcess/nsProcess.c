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
#include <tlhelp32.h>
#include "pluginapi.h"

/* Defines */
#define NSIS_MAX_STRLEN 1024

#define SystemProcessInformation     5
#define STATUS_SUCCESS               0x00000000L
#define STATUS_INFO_LENGTH_MISMATCH  0xC0000004L

typedef struct _SYSTEM_THREAD_INFO {
  FILETIME ftCreationTime;
  DWORD dwUnknown1;
  DWORD dwStartAddress;
  DWORD dwOwningPID;
  DWORD dwThreadID;
  DWORD dwCurrentPriority;
  DWORD dwBasePriority;
  DWORD dwContextSwitches;
  DWORD dwThreadState;
  DWORD dwUnknown2;
  DWORD dwUnknown3;
  DWORD dwUnknown4;
  DWORD dwUnknown5;
  DWORD dwUnknown6;
  DWORD dwUnknown7;
} SYSTEM_THREAD_INFO;

typedef struct _SYSTEM_PROCESS_INFO {
  DWORD dwOffset;
  DWORD dwThreadCount;
  DWORD dwUnkown1[6];
  FILETIME ftCreationTime;
  DWORD dwUnkown2;
  DWORD dwUnkown3;
  DWORD dwUnkown4;
  DWORD dwUnkown5;
  DWORD dwUnkown6;
  WCHAR *pszProcessName;
  DWORD dwBasePriority;
  DWORD dwProcessID;
  DWORD dwParentProcessID;
  DWORD dwHandleCount;
  DWORD dwUnkown7;
  DWORD dwUnkown8;
  DWORD dwVirtualBytesPeak;
  DWORD dwVirtualBytes;
  DWORD dwPageFaults;
  DWORD dwWorkingSetPeak;
  DWORD dwWorkingSet;
  DWORD dwUnkown9;
  DWORD dwPagedPool;
  DWORD dwUnkown10;
  DWORD dwNonPagedPool;
  DWORD dwPageFileBytesPeak;
  DWORD dwPageFileBytes;
  DWORD dwPrivateBytes;
  DWORD dwUnkown11;
  DWORD dwUnkown12;
  DWORD dwUnkown13;
  DWORD dwUnkown14;
  SYSTEM_THREAD_INFO ati[ANYSIZE_ARRAY];
} SYSTEM_PROCESS_INFO;


/* Include conversion functions */
//#define xatoi
//#define xitoa

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

BOOL CALLBACK EnumWindowsProc(          HWND hwnd,
    LPARAM lParam
)
{
	HANDLE *data = lParam;
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == data[0])
	{
		PostMessage(data[1], WM_CLOSE, 0, 0);
		data[1] = hwnd;
	}
	return TRUE;
}

void NiceTerminate(DWORD id, BOOL bClose, BOOL *bSuccess, BOOL *bFailed)
{
  HANDLE hProc;
  HANDLE data[2];
  DWORD ec;
  BOOL bDone = FALSE;
  if (hProc=OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, id))
  {
	data[0] = id;
	data[1] = NULL;

	if (bClose)
		EnumWindows(EnumWindowsProc, data);
	if (data[1] != NULL)
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
  HMODULE hLib;
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

  if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
  {
    // WinNT/2000/XP

    SYSTEM_PROCESS_INFO *spi;
    SYSTEM_PROCESS_INFO *spiCount;
    DWORD dwSize=0x4000;
    DWORD dwData;
    ULONG (WINAPI *NtQuerySystemInformationPtr)(ULONG, PVOID, LONG, PULONG);

    if (hLib=LoadLibraryW(L"NTDLL.DLL"))
    {
      NtQuerySystemInformationPtr=(ULONG(WINAPI *)(ULONG, PVOID, LONG, PULONG))GetProcAddress(hLib, "NtQuerySystemInformation");

      if (NtQuerySystemInformationPtr)
      {
        while (1)
        {
          if (spi=LocalAlloc(LMEM_FIXED, dwSize))
          {
            uError=(*NtQuerySystemInformationPtr)(SystemProcessInformation, spi, dwSize, &dwData);

            if (uError == STATUS_SUCCESS) break;

            LocalFree(spi);

            if (uError != STATUS_INFO_LENGTH_MISMATCH)
            {
              uError=608;
              break;
            }
          }
          else
          {
            uError=608;
            break;
          }
          dwSize*=2;
        }
      }
      else uError=607;

      FreeLibrary(hLib);
    }
    else uError=606;

    if (uError != STATUS_SUCCESS) return uError;

    spiCount=spi;

    while (1)
    {
      if (spiCount->pszProcessName)
      {

#ifdef UNICODE
	    lstrcpyn(szName, spiCount->pszProcessName, MAX_PATH);
#else
	    WideCharToMultiByte(CP_ACP, 0, spiCount->pszProcessName, -1, szName, MAX_PATH, NULL, NULL);
#endif		

        if (!lstrcmpi(szName, szProcessName))
        {
          // Process found
          bFound=TRUE;

          if (bTerminate == TRUE)
          {
			  NiceTerminate(spiCount->dwProcessID, bClose, &bSuccess, &bFailed);
          }
          else break;
        }
      }
      if (spiCount->dwOffset == 0) break;
      spiCount=(SYSTEM_PROCESS_INFO *)((char *)spiCount + spiCount->dwOffset);
    }
    LocalFree(spi);
  }
  else
  {
    // Win95/98/ME

    PROCESSENTRY32 pe;
    char *pName;
    HANDLE hSnapShot;
    BOOL bResult;
    HANDLE (WINAPI *CreateToolhelp32SnapshotPtr)(DWORD, DWORD);
    BOOL (WINAPI *Process32FirstPtr)(HANDLE, LPPROCESSENTRY32);
    BOOL (WINAPI *Process32NextPtr)(HANDLE, LPPROCESSENTRY32);

    if (hLib=LoadLibraryA("KERNEL32.DLL"))
    {
      CreateToolhelp32SnapshotPtr=(HANDLE(WINAPI *)(DWORD, DWORD)) GetProcAddress(hLib, "CreateToolhelp32Snapshot");
      Process32FirstPtr=(BOOL(WINAPI *)(HANDLE, LPPROCESSENTRY32)) GetProcAddress(hLib, "Process32First");
      Process32NextPtr=(BOOL(WINAPI *)(HANDLE, LPPROCESSENTRY32)) GetProcAddress(hLib, "Process32Next");

      if (CreateToolhelp32SnapshotPtr && Process32NextPtr && Process32FirstPtr)
      {
        // Get a handle to a Toolhelp snapshot of all the systems processes.
        if ((hSnapShot=(*CreateToolhelp32SnapshotPtr)(TH32CS_SNAPPROCESS, 0)) != INVALID_HANDLE_VALUE)
        {
          // Get the first process' information.
          pe.dwSize=sizeof(PROCESSENTRY32);
          bResult=(*Process32FirstPtr)(hSnapShot, &pe);

          // While there are processes, keep looping and checking.
          while (bResult)
          {
            //Get file name
            for (pName=pe.szExeFile + lstrlen(pe.szExeFile) - 1; *pName != '\\' && *pName != '\0'; --pName);

			++pName;

#ifdef UNICODE
			MultiByteToWideChar(CP_ACP, 0, pName, lstrlenA(pName)+1, szName, MAX_PATH);
#else
			lstrcpyn(szName, pName, MAX_PATH);
#endif		

            if (!lstrcmpi(szName, szProcessName))
            {
              // Process found
              bFound=TRUE;

              if (bTerminate == TRUE)
              {
                // Open for termination
				  NiceTerminate(pe.th32ProcessID, bClose, &bSuccess, &bFailed);
              }
              else break;
            }
            //Keep looking
            bResult=(*Process32NextPtr)(hSnapShot, &pe);
          }
          CloseHandle(hSnapShot);
        }
        else uError=611;
      }
      else uError=610;

      FreeLibrary(hLib);
    }
    else uError=609;
  }

  if (bFound == FALSE) return 603;
  if (bTerminate == TRUE)
  {
    if (bSuccess == FALSE) return 601;
    if (bFailed == TRUE) return 602;
  }
  return 0;
}
