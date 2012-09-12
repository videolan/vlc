//Copyright (C) 2007 Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.
#pragma once
/** /#define BUILD_DBGRELEASE // Include simple debug output in release version */
/** /#define BUILD_DBGSELECTELVMODE //Test MyRunAs*/

/** /#define UNICODE // Unicode build */
/**/#define FEAT_CUSTOMRUNASDLG // Include custom runas dialog */
/**/#define FEAT_CUSTOMRUNASDLG_TRANSLATE //*/
/**/#define FEAT_MSRUNASDLGMODHACK // Default to other user radio button */


#if !defined(APSTUDIO_INVOKED) && !defined(RC_INVOKED)

#if (defined(_MSC_VER) && !defined(_DEBUG))
	#pragma comment(linker,"/opt:nowin98")
	#pragma comment(linker,"/ignore:4078")
	#pragma comment(linker,"/merge:.rdata=.text")
	
	//#pragma intrinsic(memset) //http://www.codeguru.com/forum/showthread.php?t=371491&page=2&pp=15 | http://www.ddj.com/windows/184416623
#endif

#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#endif
#ifdef _UNICODE
#define TFUNCSUFFIX W
#else
#define TFUNCSUFFIX A
#endif
#define _PCJOIN(a,b) a##b
#define PCJOIN(a,b) _PCJOIN(a,b)


#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include "nsisutil.h"

#ifndef SEE_MASK_NOZONECHECKS
#define SEE_MASK_NOZONECHECKS 0x00800000
#endif

#define COUNTOF(___c) ( sizeof(___c)/sizeof(___c[0]) )
#ifndef ARRAYSIZE
#define ARRAYSIZE COUNTOF
#endif

#if _MSC_VER >= 1400
extern void* __cdecl memset(void*mem,int c,size_t len);
#endif

FORCEINLINE LRESULT MySndDlgItemMsg(HWND hDlg,int id,UINT Msg,WPARAM wp=0,LPARAM lp=0) {return SendMessage(GetDlgItem(hDlg,id),Msg,wp,lp);}
#ifndef UAC_NOCUSTOMIMPLEMENTATIONS
FORCEINLINE HANDLE WINAPI GetCurrentProcess(){return ((HANDLE)(-1));}
FORCEINLINE HANDLE WINAPI GetCurrentThread(){return ((HANDLE)(-2));}

#define MyTStrLen lstrlen

#undef lstrcpy
#define lstrcpy MyTStrCpy
FORCEINLINE LPTSTR MyTStrCpy(LPTSTR s1,LPCTSTR s2) {return PCJOIN(lstr,PCJOIN(cpyn,TFUNCSUFFIX))(s1,s2,0x7FFFFFFF);}

#undef lstrcat
#define lstrcat MyTStrCat
LPTSTR MyTStrCat(LPTSTR s1,LPCTSTR s2) 
#ifdef UAC_INITIMPORTS
{return s1?MyTStrCpy(&s1[MyTStrLen(s1)],s2):NULL;}
#else
;
#endif //UAC_INITIMPORTS

#endif //UAC_NOCUSTOMIMPLEMENTATIONS


//DelayLoaded functions:
typedef BOOL	(WINAPI*ALLOWSETFOREGROUNDWINDOW)(DWORD dwProcessId);
typedef BOOL	(WINAPI*OPENPROCESSTOKEN)(HANDLE ProcessHandle,DWORD DesiredAccess,PHANDLE TokenHandle);
typedef BOOL	(WINAPI*OPENTHREADTOKEN)(HANDLE ThreadHandle,DWORD DesiredAccess,BOOL OpenAsSelf,PHANDLE TokenHandle);
typedef BOOL	(WINAPI*GETTOKENINFORMATION)(HANDLE hToken,TOKEN_INFORMATION_CLASS TokInfoClass,LPVOID TokInfo,DWORD TokInfoLenh,PDWORD RetLen);
typedef BOOL	(WINAPI*ALLOCATEANDINITIALIZESID)(PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,BYTE nSubAuthorityCount,DWORD sa0,DWORD sa1,DWORD sa2,DWORD sa3,DWORD sa4,DWORD sa5,DWORD sa6,DWORD sa7,PSID*pSid);
typedef PVOID	(WINAPI*FREESID)(PSID pSid);
typedef BOOL	(WINAPI*EQUALSID)(PSID pSid1,PSID pSid2);
typedef BOOL	(WINAPI*CHECKTOKENMEMBERSHIP)(HANDLE TokenHandle,PSID SidToCheck,PBOOL IsMember);
#ifdef FEAT_CUSTOMRUNASDLG
typedef BOOL	(WINAPI*GETUSERNAME)(LPTSTR lpBuffer,LPDWORD nSize);
typedef BOOL	(WINAPI*CREATEPROCESSWITHLOGONW)(LPCWSTR lpUsername,LPCWSTR lpDomain,LPCWSTR lpPassword,DWORD dwLogonFlags,LPCWSTR lpApplicationName,LPWSTR lpCommandLine,DWORD dwCreationFlags,LPVOID pEnv,LPCWSTR WorkDir,LPSTARTUPINFOW pSI,LPPROCESS_INFORMATION pPI);
#define SECURITY_WIN32
#include <security.h>//namesamcompatible
typedef BOOLEAN	(WINAPI*GETUSERNAMEEX)(EXTENDED_NAME_FORMAT NameFormat,LPTSTR lpNameBuffer,PULONG nSize);
#endif
#ifdef UAC_INITIMPORTS
ALLOWSETFOREGROUNDWINDOW _AllowSetForegroundWindow=0;
OPENPROCESSTOKEN		_OpenProcessToken=0;
OPENTHREADTOKEN			_OpenThreadToken=0;
GETTOKENINFORMATION		_GetTokenInformation=0;
ALLOCATEANDINITIALIZESID _AllocateAndInitializeSid=0;
FREESID					_FreeSid=0;
EQUALSID				_EqualSid=0;
CHECKTOKENMEMBERSHIP	_CheckTokenMembership=0;
#ifdef FEAT_CUSTOMRUNASDLG
GETUSERNAME				_GetUserName=0;
GETUSERNAMEEX			_GetUserNameEx=0;
CREATEPROCESSWITHLOGONW	_CreateProcessWithLogonW=0;
#endif
#else
#ifdef FEAT_CUSTOMRUNASDLG
extern GETUSERNAME _GetUserName;
extern GETUSERNAMEEX _GetUserNameEx;
extern CREATEPROCESSWITHLOGONW _CreateProcessWithLogonW;
#endif
#endif /* UAC_INITIMPORTS */ 


extern DWORD DelayLoadDlls();
#ifdef FEAT_CUSTOMRUNASDLG
extern DWORD MyRunAs(HINSTANCE hInstDll,SHELLEXECUTEINFO&sei);
#endif

#if !defined(NTDDI_VISTA) || defined(BUILD_OLDSDK)
//#if !defined(NTDDI_VERSION) || (NTDDI_VERSION < 0x06000000) || !defined(NTDDI_VISTA) 
//#if !defined(TOKEN_ELEVATION_TYPE) || defined(BUILD_OLDSDK)
enum TOKEN_ELEVATION_TYPE { 
	TokenElevationTypeDefault = 1, 
	TokenElevationTypeFull, 
	TokenElevationTypeLimited 
};
enum _TOKEN_INFORMATION_CLASS___VISTA {
	TokenElevationType = (TokenOrigin+1),
	TokenLinkedToken,
	TokenElevation,
	TokenHasRestrictions,
	TokenAccessInformation,
	TokenVirtualizationAllowed,
	TokenVirtualizationEnabled,
	TokenIntegrityLevel,
	TokenUIAccess,
	TokenMandatoryPolicy,
	TokenLogonSid,
};
#endif


#if defined(_DEBUG) || defined(BUILD_DBGRELEASE)
//Simple debug helpers:
#define  BUILD_DBG
/** /#define BUILD_XPTEST //Pretend UAC exists and "elevate" with NT runas */ 
#define DBG_RESETDBGVIEW() do{HWND hDbgView=FindWindowA("dbgviewClass",0);PostMessage(hDbgView,WM_COMMAND,40020,0);if(0)SetForegroundWindow(hDbgView);}while(0)
#define _pp_MakeStr_(x)	#x
#define pp_MakeStr(x)	_pp_MakeStr_(x)
#define TRACE OutputDebugStringA
#define DBGONLY(_x) _x
#ifndef ASSERT
#define ASSERT(_x) do{if(!(_x)){MessageBoxA(GetActiveWindow(),#_x##"\n\n"##__FILE__##":"## pp_MakeStr(__LINE__),"SimpleAssert",0);/*TRACE(#_x##"\n"##__FILE__##":" pp_MakeStr(__LINE__)"\n");*/}}while(0)
#endif
#define VERIFY(_x) ASSERT(_x)
static void TRACEF(const char*fmt,...) {va_list a;va_start(a,fmt);static TCHAR b[1024*4];if (sizeof(TCHAR)!=sizeof(char)){static TCHAR fmtBuf[COUNTOF(b)];VERIFY(wsprintf(fmtBuf,_T("%hs"),fmt)<COUNTOF(fmtBuf));fmt=(LPCSTR)fmtBuf;}wvsprintf(b,(TCHAR*)fmt,a);OutputDebugString(b);}
#else
#define TRACE /*(void)0*/
#define DBGONLY(_x)
#define ASSERT(_x)
#define VERIFY(_x) ((void)(_x))
#define TRACEF TRACE
#endif /* DEBUG */

#endif /* APSTUDIO_INVOKED */

