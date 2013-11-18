//Copyright (C) 2007 Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.
/*
UAC plugin for NSIS
===================
Compiled with VC6+PlatformSDK (StdCall & MinimizeSize)


Todo:
-----
¤GetCurrentDir in elevated parent and pass along to outer process for Exec* (or somekind of ipc to request it if workingdir param is empty)
X¤Check if secondary logon service is running in SysElevationPresent() on NT5
¤Use IsUserAnAdmin? MAKEINTRESOURCE(680) export on 2k (it even exists on NT4?) //http://forums.winamp.com/showthread.php?s=&threadid=195020
¤AllowSetForegroundWindow
¤Use RPC instead of WM_COPYDATA for IPC
¤Autodetect "best" default admin user in MyRunAs
¤Use ChangeWindowMessageFilter so we can get >WM_USER msg success feedback and possibly fill the log with detailprints
¤Hide IPC window inside inner instance window? (Could this add unload problems?)
¤CREATE_PRESERVE_CODE_AUTHZ_LEVEL? http://msdn2.microsoft.com/en-us/library/ms684863.aspx
¤UpdateProcThreadAttribute?
¤Consent UI on XP ?
¤All langs in single file; [MyRunAsStrings]>LangSections != 0 then load strings from [langid] sections
¤BroadcastSystemMessage to help with SetForeground
¤UAC::StackPop


Notes:
------
Primary integrity levels:
Name					SID				RID 
Low Mandatory Level		S-1-16-4096		0x1000
Medium Mandatory Level	S-1-16-8192		0x2000
High Mandatory Level	S-1-16-12288	0x3000
System Mandatory Level	S-1-16-16384	0x4000

*/

#define UAC_HACK_Clammerz	//ugly messagebox focus hack for .onInit
#define UAC_HACK_FGWND1		//super ugly fullscreen invisible window for focus tricks

#define UAC_INITIMPORTS
#include "uac.h"
#include <objbase.h>//CoInitialize
#include "nsisutil.h"
using namespace NSIS;
NSISUTIL_INIT();


#define ERRAPP_BADIPCSRV (0x20000000|1)
#define SW_INVALID ((WORD)-1)
#define IPCTOUT_DEF (1000*3) //default timeout for IPC messages
#define IPCTOUT_SHORT 1500
#define IPCTOUT_LONG (IPCTOUT_DEF*2)

enum _IPCSRVWNDMSG 
{
	IPC_GETEXITCODE=WM_USER,	//Get exit-code of the process spawned by the last call to ExecWait/ShellExecWait
	IPC_ELEVATEAGAIN,
	IPC_GETSRVPID,				//Get PID of outer process
	IPC_GETSRVHWND,				//Get $HWNDParent of outer process
	IPC_EXECCODESEGMENT,		//wp:pos | lp:hwnd | returns ErrorCode+1
	IPC_GETOUTERPROCESSTOKEN,	
#ifdef UAC_HACK_FGWND1
	IPC_HACKFINDRUNAS,
#endif
};

enum _COPYDATAID 
{
	CDI_SHEXEC=666,	//returns WindowsErrorCode+1
	CDI_SYNCVAR,
	CDI_STACKPUSH,
};

typedef struct 
{
	UINT VarId;
	NSISCH buf[ANYSIZE_ARRAY];
} IPC_SYNCVAR;

typedef struct 
{
	HWND hwnd;
	bool Wait;
	bool UseCreateProcess;
	WORD ShowMode;	
	NSISCH*strExec;
	NSISCH*strParams;
	NSISCH*strWorkDir;
	NSISCH*strVerb;
	NSISCH buf[ANYSIZE_ARRAY];
} IPC_SHEXEC;


typedef struct 
{
	HINSTANCE	hInstance;
	HWND		hSrvIPC;
	BYTE		DllRef;
	bool		UseIPC;
	bool		CheckedIPCParam;
	UINT NSISStrLen;
	//IPC Server stuff:
	HANDLE		hElevatedProcess;
	HANDLE		threadIPC;
	DWORD		LastWaitExitCode;//Exit code of process started from last call to ExecWait/ShellExecWait
	NSIS::extra_parameters*pXParams;
	bool		ElevateAgain;
	//DelayLoadedModules:
	HMODULE		hModAdvAPI;
} GLOBALS;


GLOBALS g = {0};


void StopIPCSrv();
DWORD _GetElevationType(TOKEN_ELEVATION_TYPE* pTokenElevType);
bool GetIPCSrvWndFromParams(HWND&hwndOut);



#if _MSC_VER >= 1400 //MSVC 2005 wants to pull in the CRT, let's try to help it out
void* __cdecl memset(void*mem,int c,size_t len) 
{
	char *p=(char*)mem;
	while (len-- > 0){*p++=c;}
	return mem;
}
#endif



FORCEINLINE NSISCH* GetIPCWndClass() { return _T("NSISUACIPC"); }
FORCEINLINE bool StrCmpI(LPCTSTR s1,LPCTSTR s2) {return 0==lstrcmpi(s1,s2);}
LPTSTR StrNextChar(LPCTSTR Str) { return CharNext(Str); }
bool StrContainsWhiteSpace(LPCTSTR s) { if (s) {while(*s && *s>_T(' '))s=StrNextChar(s);if (*s)return true;}return false; }

DWORD GetSysVer(bool Major=true) 
{
	OSVERSIONINFO ovi = { sizeof(ovi) };
	if ( !GetVersionEx(&ovi) ) return 0;
	return Major ? ovi.dwMajorVersion : ovi.dwMinorVersion;
}
#define GetOSVerMaj() (GetSysVer(true))
#define GetOSVerMin() (GetSysVer(false))

UINT_PTR StrToUInt(LPTSTR s,bool ForceHEX=false,BOOL*pFoundBadChar=0) 
{
	UINT_PTR v=0;
	BYTE base=ForceHEX?16:10;	
	if (pFoundBadChar)*pFoundBadChar=false;
	if ( !ForceHEX && *s=='0' && ((*(s=StrNextChar(s)))&~0x20)=='X' && (s=StrNextChar(s)) )base=16;
	for (TCHAR c=*s; c; c=*(s=StrNextChar(s)) ) 
	{
		if (c >= _T('0') && c <= _T('9')) c-='0';
		else if (base==16 && (c & ~0x20) >= 'A' && (c & ~0x20) <= 'F') c=(c & 7) +9;
		else 
		{
			if (pFoundBadChar /*&& c!=' '*/)*pFoundBadChar=true;
			break;
		}
		v*=base;v+=c;
	}
	return v;
}

LPTSTR FindExePathEnd(LPTSTR p) 
{
	if ( *p=='"' && *(++p) ) 
	{
		while( *p && *p!='"' )p=StrNextChar(p);
		if (*p)
			p=StrNextChar(p);
		else 
			return 0;
	}
	else 
		if ( *p!='/' )while( *p && *p!=' ' )p=StrNextChar(p);
	return p;
}


#ifdef FEAT_MSRUNASDLGMODHACK
HHOOK g_MSRunAsHook;
void MSRunAsDlgMod_Unload(void*hook) 
{
	if (hook) 
	{
		//ASSERT(g_MSRunAsHook==hook);
		UnhookWindowsHookEx((HHOOK)hook);
		//g_MSRunAsHook=0;
	}
}
LRESULT CALLBACK MSRunAsDlgMod_ShellProc(int nCode,WPARAM wp,LPARAM lp) 
{
	CWPRETSTRUCT*pCWPS;
	if (nCode >= 0 && (pCWPS=(CWPRETSTRUCT*)lp) && WM_INITDIALOG==pCWPS->message)
	{
		TCHAR buf[30];
		GetClassName(pCWPS->hwnd,buf,COUNTOF(buf));
		if (!lstrcmpi(buf,_T("#32770"))) 
		{ 
			const UINT IDC_USRSAFER=0x106,IDC_OTHERUSER=0x104,IDC_SYSCRED=0x105;
			GetClassName(GetDlgItem(pCWPS->hwnd,IDC_SYSCRED),buf,COUNTOF(buf));
			if (!lstrcmpi(buf,_T("SysCredential"))) //make sure this is the run as dialog
			{
				MySndDlgItemMsg(pCWPS->hwnd,IDC_USRSAFER,BM_SETCHECK,BST_UNCHECKED);
				MySndDlgItemMsg(pCWPS->hwnd,IDC_OTHERUSER,BM_CLICK);
			}
		}
	}
	return CallNextHookEx(g_MSRunAsHook,nCode,wp,lp);
}
void* MSRunAsDlgMod_Init() 
{
	if(GetOSVerMaj()!=5 || GetOSVerMin()<1)return NULL;//only XP/2003
	return g_MSRunAsHook=SetWindowsHookEx(WH_CALLWNDPROCRET,MSRunAsDlgMod_ShellProc,0,GetCurrentThreadId());
}
#endif

DWORD DllSelfAddRef() 
{
	NSISCH buf[MAX_PATH*5];//Lets hope $pluginsdir is shorter than this, only special builds could break this
	DWORD len=GetModuleFileName(g.hInstance,buf,MAX_PATH*5);
	if ( len && len<MAX_PATH*5 && LoadLibrary(buf) ) 
	{
		if (!g.DllRef)g.DllRef++;
		return NO_ERROR;
	}
	ASSERT(!"DllSelfAddRef failed!");
	return ERROR_BUFFER_OVERFLOW;
}

FORCEINLINE DWORD MaintainDllSelfRef() //Call this from every exported function to prevent NSIS from unloading our plugin
{ 
	if(!g.CheckedIPCParam && !g.DllRef) 
	{
		HWND hSrv;
		g.CheckedIPCParam=true;
		g.UseIPC=GetIPCSrvWndFromParams(hSrv);
		if(g.UseIPC) 
		{
			g.DllRef++;
			g.hSrvIPC=hSrv;
		}
	}
	return (g.DllRef)?DllSelfAddRef():NO_ERROR;
}


DWORD SendIPCMsg(UINT Msg,WPARAM wp,LPARAM lp,DWORD_PTR&MsgRet,DWORD tout=IPCTOUT_DEF,const HWND hIPCSrv=g.hSrvIPC) 
{
	if (tout==INFINITE) //BUGFIX: SendMessageTimeout(...,INFINITE,...) seems to be broken, SendMessageTimeout(...,SMTO_NORMAL,0,..) seems to work but why take the chance
	{
		MsgRet=SendMessage(hIPCSrv,Msg,wp,lp);
		return NO_ERROR;
	}
	if ( SendMessageTimeout(hIPCSrv,Msg,wp,lp,SMTO_NORMAL,tout,&MsgRet) )return NO_ERROR;
	return (tout=GetLastError()) ? tout : ERROR_TIMEOUT; 
}

void _Unload() 
{
	StopIPCSrv();
	if (g.DllRef) 
	{
		g.DllRef=0;
		FreeLibrary(g.hInstance);
		//Why bother?> FreeLibrary(g.hModAdvAPI);
	}
}

DWORD DelayLoadGetProcAddr(void**ppProc,HMODULE hLib,LPCSTR Export) 
{
	ASSERT(ppProc && hLib && Export);
	if (!*ppProc) 
	{
		*ppProc=(void*)GetProcAddress(hLib,Export);
		if (!*ppProc)return GetLastError();
	}
	return NO_ERROR;
}

DWORD DelayLoadDlls() 
{

#ifdef UNICODE
#	define __DLD_FUNCSUFFIX "W"
#	else
#	define __DLD_FUNCSUFFIX "A"
#	endif

	if (!g.hModAdvAPI) //using g.hModAdvAPI to test if this is the first time we have been called
	{
		struct 
		{
			HMODULE*pMod;
			LPCSTR DllName;//NOTE: Always using ANSI strings to save a couple of bytes
		} 
		dld[]=
		{ 
			{&g.hModAdvAPI,"AdvAPI32"},
			{0}
		};
		DWORD ec;
		UINT o;

		for (o=0; dld[o].pMod; ++o)
			if ( !(*dld[o].pMod=LoadLibraryA(dld[o].DllName)) )
				return GetLastError();

		struct 
		{
			HMODULE hMod;
			void**ppProc;
			LPCSTR Export;
			bool Optional;
		} 
		dldprocs[]=
		{
			{GetModuleHandle(_T("USER32")),(void**)&_AllowSetForegroundWindow,"AllowSetForegroundWindow",true},
			{g.hModAdvAPI,(void**)&_OpenProcessToken,			"OpenProcessToken"},
			{g.hModAdvAPI,(void**)&_OpenThreadToken,			"OpenThreadToken"},
			{g.hModAdvAPI,(void**)&_GetTokenInformation,		"GetTokenInformation"},
			{g.hModAdvAPI,(void**)&_AllocateAndInitializeSid,	"AllocateAndInitializeSid"},
			{g.hModAdvAPI,(void**)&_FreeSid,					"FreeSid"},
			{g.hModAdvAPI,(void**)&_EqualSid,					"EqualSid"},
			{g.hModAdvAPI,(void**)&_CheckTokenMembership,		"CheckTokenMembership",true},
			#ifdef FEAT_CUSTOMRUNASDLG
			{g.hModAdvAPI,(void**)&_GetUserName,			"GetUserName" __DLD_FUNCSUFFIX},
			{g.hModAdvAPI,(void**)&_CreateProcessWithLogonW,"CreateProcessWithLogonW",true},
			{LoadLibraryA("SECUR32"),(void**)&_GetUserNameEx,"GetUserNameEx" __DLD_FUNCSUFFIX,true},//We never free this library
			#endif
			{0}
		};
//#undef __DLD_FUNCSUFFIX
		for (o=0; dldprocs[o].hMod; ++o)
			if (ec=DelayLoadGetProcAddr(dldprocs[o].ppProc,dldprocs[o].hMod,dldprocs[o].Export) && !dldprocs[o].Optional) 
			{
				TRACEF("DelayLoadDlls failed to find %s in %X\n",dldprocs[o].Export,dldprocs[o].hMod);
				return ec;
			}
	}
	return NO_ERROR;
}

void AllowOuterInstanceWindowFocus() 
{
	if (g.UseIPC) 
	{
		DWORD_PTR MsgRet;
		if (!SendIPCMsg(IPC_GETSRVPID,0,0,MsgRet,IPCTOUT_SHORT) && MsgRet && _AllowSetForegroundWindow)_AllowSetForegroundWindow(MsgRet);
	}
}

DWORD SyncVars(HWND hwndNSIS) 
{
	DWORD i,ec=NO_ERROR;
	IPC_SYNCVAR*pSV=0;
	if (!g.UseIPC)return NO_ERROR;
	g.NSISStrLen=NSIS::StrSize;
	TRACEF("SyncVars: g.NSISStrLen=%d\n",g.NSISStrLen);ASSERT(g.NSISStrLen>10);
	DWORD cbStruct=FIELD_OFFSET(IPC_SYNCVAR,buf);
	pSV=(IPC_SYNCVAR*)MemAlloc(cbStruct);
	if (!pSV)
		goto die_GLE;
	else 
	{
		COPYDATASTRUCT cds={CDI_SYNCVAR,cbStruct,pSV};
		for (i=0;i<__INST_LAST && !ec;++i) 
		{
			pSV->VarId=i;
			lstrcpyn(pSV->buf,GetVar(i),g.NSISStrLen);
			DWORD MsgRet;//TRACEF("SyncVars: (%d)%s|\n",i,pSV->buf);
			if (!(ec=SendIPCMsg(WM_COPYDATA,(WPARAM)hwndNSIS,(LPARAM)&cds,MsgRet,3000 )))ec=MsgRet-1;
		}	
	}
	return ec;
die_GLE:
	return GetLastError();
}

DWORD _Exec(HWND hwnd,NSISCH*Verb,NSISCH*Exec,NSISCH*Params,NSISCH*WorkDir,WORD ShowWnd,bool Wait,bool UseCreateProcess) 
{
	DWORD ec;
	NSISCH*buf=0;
	SHELLEXECUTEINFO sei={sizeof(SHELLEXECUTEINFO)};
	sei.hwnd		=hwnd;
	sei.nShow		=(ShowWnd!=SW_INVALID)?ShowWnd:SW_NORMAL;
	sei.fMask		=SEE_MASK_FLAG_DDEWAIT;
	sei.lpFile		=(Exec&&*Exec)			?Exec:0;
	sei.lpParameters=(Params&&*Params)		?Params:0;
	sei.lpDirectory	=(WorkDir&&*WorkDir)	?WorkDir:0;
	sei.lpVerb		=(Verb&&*Verb)			?Verb:0;
	TRACEF("_Exec:%X|%s|%s|%s|wait=%d useCreateProc=%d ShowWnd=%d useShowWnd=%d\n",hwnd,Exec,Params,WorkDir,Wait,UseCreateProcess,ShowWnd,ShowWnd!=SW_INVALID);
	if (UseCreateProcess) 
	{
		STARTUPINFO si={sizeof(STARTUPINFO)};
		if (ShowWnd != SW_INVALID) 
		{
			si.dwFlags|=STARTF_USESHOWWINDOW;
			si.wShowWindow=sei.nShow;
		}
		PROCESS_INFORMATION pi;
		const NSISCH*Q=( (*Exec!='"') && (*Params) && StrContainsWhiteSpace(Exec)) ? _T("\"") : _T("");//Add extra quotes to program part of command-line?
		const DWORD len= ((*Q)?2:0) + lstrlen(Exec) + 1 + lstrlen(Params) + 1;
		buf=(NSISCH*)NSIS::MemAlloc(len*sizeof(NSISCH));
		if (!buf)return ERROR_OUTOFMEMORY;
		//Build string for CreateProcess, "[Q]<Exec>[Q][Space]<Params>"
		wsprintf(buf,_T("%s%s%s%s%s"),Q,Exec,Q,((*Params)?_T(" "):_T("")),Params);
		TRACEF("_Exec: calling CreateProcess>%s< in >%s< addedQ=%d show=%u\n",buf,sei.lpDirectory,*Q,sei.nShow);
		if ( !CreateProcess(0,buf,0,0,false,0,0,sei.lpDirectory,&si,&pi) ) goto die_GLE;
		CloseHandle(pi.hThread);
		sei.hProcess=pi.hProcess;
	}
	else 
	{
		sei.fMask|=SEE_MASK_NOCLOSEPROCESS;
		TRACEF("_Exec: calling ShellExecuteEx...\n");
		if ( !ShellExecuteEx(&sei) )goto die_GLE;
	}
	if (Wait) 
	{
		WaitForSingleObject(sei.hProcess,INFINITE);
		GetExitCodeProcess(sei.hProcess,&g.LastWaitExitCode);
	}
	else WaitForInputIdle(sei.hProcess,1500);//wait a little bit so the finish page window does not go away too fast and cause focus problems

	CloseHandle(sei.hProcess);
	ec=NO_ERROR;
ret:
	if (buf)NSIS::MemFree(buf);
	return ec;
die_GLE:
	ec=GetLastError();
	TRACEF("_Exec>%s failed with error %u (%s)\n",UseCreateProcess?"CreateProcess":"ShExec",ec,buf);
	goto ret;
}

WORD GetShowWndCmdFromStr(NSISCH*s) 
{
	//NOTE: Little used modes are still supported, just not with strings, you must use the actual number or ${SW_xx} defines from WinMessages.h
	struct {NSISCH*id;WORD cmd;} swcm[] = {
		{_T("SW_HIDE"),				SW_HIDE},
		{_T("SW_SHOW"),				SW_SHOW},
		{_T("SW_RESTORE"),			SW_RESTORE},
		{_T("SW_MAXIMIZE"),			SW_MAXIMIZE},
		{_T("SW_MINIMIZE"),			SW_MINIMIZE},
		//	{_T("SW_MAX"),			SW_MAXIMIZE},
		//	{_T("SW_MIN"),			SW_MINIMIZE},
		{_T("SW_SHOWNORMAL"),		SW_SHOWNORMAL},
		//{_T("SW_NORMAL"),			SW_NORMAL},
		//{_T("SW_SHOWMINIMIZED"),	SW_SHOWMINIMIZED},
		//{_T("SW_SHOWMAXIMIZED"),	SW_SHOWMAXIMIZED},
		//{_T("SW_SHOWNOACTIVATE"),	SW_SHOWNOACTIVATE},
		//{_T("SW_SHOWNA"),			SW_SHOWNA},
		//{_T("SW_SHOWMINNOACTIVE"),	SW_SHOWMINNOACTIVE},
		//{_T("SW_SHOWDEFAULT"),		SW_SHOWDEFAULT},
		//{_T("SW_FORCEMINIMIZE"),	SW_FORCEMINIMIZE},
		{0}
	};
	for (int i=0; swcm[i].id; ++i) if (StrCmpI(s,swcm[i].id)) return swcm[i].cmd;
	return SW_INVALID;
}

#define HasIPCServer() (g.UseIPC!=NULL)

void HandleExecExport(bool CreateProc,bool Wait,HWND&hwndNSIS,int&StrSize,NSISCH*&Vars,stack_t**&StackTop,NSIS::extra_parameters*pXParams) 
{
	DWORD ec=NO_ERROR,ForkExitCode=ERROR_INVALID_FUNCTION;
	UINT cch=0,cbStruct;
	WORD ShowWnd;
	IPC_SHEXEC*pISE=0;
	stack_t* const pSIVerb=CreateProc?0:StackPop();//Only ShellExec supports verb's
	stack_t* const pSIShowWnd	=StackPop();
	stack_t* const pSIExec		=StackPop();
	stack_t* const pSIParams	=StackPop();
	stack_t* const pSIWorkDir	=StackPop();

	if (ec=MaintainDllSelfRef())goto ret;
	if (!pSIExec || !pSIParams || !pSIWorkDir || !pSIShowWnd || (!pSIVerb && !CreateProc)) 
	{
		TRACE("If you see this you probably forgot that all parameters are required!\n");
		ec=ERROR_INVALID_PARAMETER;
		goto ret;
	}
	ShowWnd=GetShowWndCmdFromStr(pSIShowWnd->text);
	if (ShowWnd==SW_INVALID && *pSIShowWnd->text) 
	{
		BOOL BadCh;
		ShowWnd=StrToUInt(pSIShowWnd->text,false,&BadCh);
		if (BadCh)ShowWnd=SW_INVALID;
	}
	TRACEF("HandleExecExport: ipc=%X (%X)\n",g.UseIPC,g.hSrvIPC);
	SyncVars(hwndNSIS);
	if (!g.UseIPC) //No IPC Server, we are not elevated with UAC
	{
		ec=_Exec(hwndNSIS,pSIVerb?pSIVerb->text:0,pSIExec->text,pSIParams->text,pSIWorkDir->text,ShowWnd,Wait,CreateProc);
		if (Wait)ForkExitCode=g.LastWaitExitCode;
		goto ret;
	}	
	cch+=lstrlen(pSIExec->text)+1;
	cch+=lstrlen(pSIParams->text)+1;
	cch+=lstrlen(pSIWorkDir->text)+1;
	if (pSIVerb)cch+=lstrlen(pSIVerb->text)+1;
	cbStruct=FIELD_OFFSET( IPC_SHEXEC, buf );
	pISE=(IPC_SHEXEC*)NSIS::MemAlloc(cbStruct);
	if (!pISE)ec=GetLastError();
	if (!ec) 
	{
		DWORD_PTR MsgRet;
		pISE->hwnd		=hwndNSIS;
		pISE->Wait		=Wait;
		pISE->ShowMode	=ShowWnd;
		pISE->UseCreateProcess=CreateProc;
		//Just offsets at this point
		pISE->strExec	=(NSISCH*)0;
		pISE->strParams	=(NSISCH*)(lstrlen(pSIExec->text)	+pISE->strExec+1);
		pISE->strWorkDir=(NSISCH*)(lstrlen(pSIParams->text)	+pISE->strParams+1);
		pISE->strVerb=	 (NSISCH*)(lstrlen(pSIWorkDir->text)+pISE->strWorkDir+1);
		lstrcpy(pISE->buf,pSIExec->text);
		lstrcpy(&pISE->buf[(DWORD_PTR)pISE->strParams],	pSIParams->text);
		lstrcpy(&pISE->buf[(DWORD_PTR)pISE->strWorkDir],pSIWorkDir->text);
		if (pSIVerb)lstrcpy(&pISE->buf[(DWORD_PTR)pISE->strVerb],	pSIVerb->text);
		COPYDATASTRUCT cds;
		cds.dwData=CDI_SHEXEC;
		cds.cbData=cbStruct;
		cds.lpData=pISE;
		AllowOuterInstanceWindowFocus();
		if (!(ec=SendIPCMsg(WM_COPYDATA,(WPARAM)hwndNSIS,(LPARAM)&cds,MsgRet,Wait?(INFINITE):(IPCTOUT_LONG) )))ec=MsgRet-1;
		TRACEF("HandleExecExport: IPC returned %X, ec=%d\n",MsgRet,ec);
		if (Wait && NO_ERROR==ec) 
		{
			ec=SendIPCMsg(IPC_GETEXITCODE,0,0,ForkExitCode);
			TRACEF("HandleExecExport(Wait): Spawned process exit code=%d",ForkExitCode);
		}
	}
ret:
	NSIS::MemFree(pISE);
	StackFreeItem(pSIShowWnd);
	StackFreeItem(pSIExec);
	StackFreeItem(pSIParams);
	StackFreeItem(pSIWorkDir);
	StackFreeItem(pSIVerb);
	SetVarUINT(INST_0,ec);
	if (ec)SetErrorFlag(pXParams);
	if (Wait)SetVarUINT(INST_1,ForkExitCode);
}


bool _SupportsUAC(bool VersionTestOnly=false) 
{
	TOKEN_ELEVATION_TYPE tet;
	OSVERSIONINFO ovi={sizeof(ovi)};
	if (!GetVersionEx(&ovi)) 
	{
		ASSERT(!"_SupportsUAC>GetVersionEx");
		return false;
	}
	if (VersionTestOnly)return ovi.dwMajorVersion>=6;
	if (ovi.dwMajorVersion>=6 && _GetElevationType(&tet)==NO_ERROR) 
	{
		const bool ret=tet!=TokenElevationTypeDefault && tet!=NULL;
		TRACEF("_SupportsUAC tet=%d, returning %d\n",tet,ret);
		return ret;
	}
	DBGONLY(TRACEF("_SupportsUAC returning false! ver=%d _GetElevationType.ret=%u\n",ovi.dwMajorVersion,_GetElevationType(&tet)));
	return false;
}

DWORD _GetElevationType(TOKEN_ELEVATION_TYPE*pTokenElevType) 
{
	DWORD ec=ERROR_ACCESS_DENIED;
	HANDLE hToken=0;
	DWORD RetLen;
	if (!pTokenElevType)return ERROR_INVALID_PARAMETER;
	if (ec=DelayLoadDlls())return ec;
	*pTokenElevType=(TOKEN_ELEVATION_TYPE)NULL;
	if (!_SupportsUAC(true))return NO_ERROR;
	if (!_OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))goto dieLastErr;
	if (!_GetTokenInformation(hToken,(_TOKEN_INFORMATION_CLASS)TokenElevationType,pTokenElevType,sizeof(TOKEN_ELEVATION_TYPE),&RetLen))goto dieLastErr;
	SetLastError(NO_ERROR);
dieLastErr:
	ec=GetLastError();
	CloseHandle(hToken);
	TRACEF("_GetElevationType ec=%u type=%d\n",ec,*pTokenElevType);
	return ec;
}


bool _IsUACEnabled() 
{
	HKEY hKey;
	bool ret=false;
	if (GetSysVer()>=6 && NO_ERROR==RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"),0,KEY_READ,&hKey)) 
	{
		//Check must be !=0, see http://codereview.chromium.org/3110 & http://src.chromium.org/viewvc/chrome?view=rev&revision=2330
		//Apparently, Vista treats !=0 as UAC on, and some machines have EnableLUA=2 !!
		DWORD val,type,size=sizeof(DWORD);
		if (NO_ERROR==RegQueryValueEx(hKey,_T("EnableLUA"),0,&type,(LPBYTE)&val,&size) && type==REG_DWORD && val!=0) ret=true;
		RegCloseKey(hKey);
	}
	return ret;
}

bool SysQuery_IsServiceRunning(LPCTSTR servicename) 
{
	bool retval=false;
	SC_HANDLE scdbh=NULL,hsvc;
	scdbh=OpenSCManager(NULL,NULL,GENERIC_READ);
	if (scdbh) 
	{
		if (hsvc=OpenService(scdbh,servicename,SERVICE_QUERY_STATUS)) 
		{
			SERVICE_STATUS ss;
			if (QueryServiceStatus(hsvc,&ss))retval=(ss.dwCurrentState==SERVICE_RUNNING);
			CloseServiceHandle(hsvc);
		}
	}
	CloseServiceHandle(scdbh);
	return retval;
}

inline bool SysNT5IsSecondaryLogonSvcRunning() 
{
	return SysQuery_IsServiceRunning(_T("seclogon"));
}

bool SysElevationPresent() //Will return false on Vista if UAC is off
{ 
	const DWORD vmaj=GetSysVer();
	ASSERT(vmaj<=6 && vmaj>=4);
	if (vmaj==5) return true; //TODO:Check if secondary logon service is running?
	if (vmaj>=6) return _IsUACEnabled();
	return false;
}

FORCEINLINE bool SysSupportsRunAs() 
{ 
	return GetSysVer()>=5;
}





bool _IsAdmin() 
{
	
#ifdef BUILD_XPTEST
	static int _dbgOld=-1;
	unsigned _dbg=(unsigned)FindExePathEnd(GetCommandLine());
	if (_dbgOld==-1){_dbg=(_dbg && *((TCHAR*)_dbg))?MessageBoxA(0,"Debug: Pretend to be admin?",GetCommandLine(),MB_YESNOCANCEL):IDCANCEL;} else _dbg=_dbgOld;_dbgOld=_dbg;TRACEF("_IsAdmin=%d|%d\n",_dbg,_dbg==IDYES);
	if (_dbg!=IDCANCEL){SetLastError(0);return _dbg==IDYES;}
#endif

	BOOL isAdmin=false;
	DWORD ec;
	OSVERSIONINFO ovi={sizeof(ovi)};	
	if (!GetVersionEx(&ovi))return false;
	if (VER_PLATFORM_WIN32_NT != ovi.dwPlatformId) //Not NT
	{
		SetLastError(NO_ERROR);
		return true;
	}
	if (ec=DelayLoadDlls()) 
	{
		TRACEF("DelayLoadDlls failed in _IsAdmin() with err x%X\n",ec);
		SetLastError(ec);
		return false;
	}

	ASSERT(_OpenThreadToken && _OpenProcessToken && _AllocateAndInitializeSid && _EqualSid && _FreeSid);
	HANDLE hToken;
	if (_OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,FALSE,&hToken) || _OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken)) 
	{
		SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;
		PSID psid=0;
		if (_AllocateAndInitializeSid(&SystemSidAuthority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&psid)) 
		{
			if (_CheckTokenMembership) 
			{
				if (!_CheckTokenMembership(0,psid,&isAdmin))isAdmin=false;
			}
			else 
			{
				DWORD cbTokenGrps;
				if (!_GetTokenInformation(hToken,TokenGroups,0,0,&cbTokenGrps)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER) 
				{
					TOKEN_GROUPS*ptg=0;
					if (ptg=(TOKEN_GROUPS*)NSIS::MemAlloc(cbTokenGrps)) 
					{
						if (_GetTokenInformation(hToken,TokenGroups,ptg,cbTokenGrps,&cbTokenGrps)) 
						{
							for (UINT i=0; i<ptg->GroupCount;i++) 
							{								
								if (_EqualSid(ptg->Groups[i].Sid,psid))isAdmin=true;
							}
						}
						NSIS::MemFree(ptg);
					}
				}
			}			
			_FreeSid(psid);
		}
		CloseHandle(hToken);
	}
	if (isAdmin) //UAC Admin with split token check
	{
		if (_SupportsUAC()) 
		{
			TOKEN_ELEVATION_TYPE tet;
			if (_GetElevationType(&tet) || tet==TokenElevationTypeLimited)isAdmin=false;
		}
		else SetLastError(NO_ERROR);
	}
	return FALSE != isAdmin;
}


LRESULT CALLBACK IPCSrvWndProc(HWND hwnd,UINT uMsg,WPARAM wp,LPARAM lp) 
{
	switch(uMsg) 
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		return DestroyWindow(hwnd);
	case WM_COPYDATA:
		if (lp) 
		{
			const COPYDATASTRUCT*pCDS=(COPYDATASTRUCT*)lp;
			if (pCDS->dwData==CDI_SHEXEC && pCDS->lpData) 
			{
				if ( pCDS->cbData < sizeof(IPC_SHEXEC) )break;
				g.LastWaitExitCode=ERROR_INVALID_FUNCTION;
				IPC_SHEXEC& ise=*((IPC_SHEXEC*)pCDS->lpData);
				SetForegroundWindow(ise.hwnd);				
				DWORD ec=_Exec(
					ise.hwnd,
					&ise.buf[(DWORD_PTR)ise.strVerb],
					&ise.buf[(DWORD_PTR)ise.strExec],
					&ise.buf[(DWORD_PTR)ise.strParams],
					&ise.buf[(DWORD_PTR)ise.strWorkDir],
					ise.ShowMode,ise.Wait,ise.UseCreateProcess
					);
				TRACEF("IPCSrvWndProc>IPC_SHEXEC>_ShExec=%d\n",ec);
				return ec+1;
			}
			else if (pCDS->dwData==CDI_SYNCVAR && pCDS->lpData && pCDS->cbData>1) 
			{
				IPC_SYNCVAR*pSV=(IPC_SYNCVAR*)pCDS->lpData;
				if (pSV->VarId>=__INST_LAST)return 1+ERROR_INVALID_PARAMETER;
				TRACEF("WM_COPYDATA: CDI_SYNCVAR:%d=%s|\n",pSV->VarId,&pSV->buf[0]);
				lstrcpy(GetVar(pSV->VarId),&pSV->buf[0]);
				return NO_ERROR+1;
			}
			else if (pCDS->dwData==CDI_STACKPUSH && pCDS->lpData && pCDS->cbData>=1) 
			{
				TRACEF("WM_COPYDATA: CDI_STACKPUSH:%s|\n",pCDS->lpData);
				return StackPush((NSISCH*)pCDS->lpData)+1;
			}
		}
		break;
	case IPC_GETEXITCODE:
		return g.LastWaitExitCode;
	case IPC_ELEVATEAGAIN:
		TRACE("IPCSrvWndProc>IPC_ELEVATEAGAIN\n");
		return (g.ElevateAgain=true);
	case IPC_GETSRVPID:
		return GetCurrentProcessId();
	case IPC_GETSRVHWND:
		return GetWindowLongPtr(hwnd,GWLP_USERDATA);
	case IPC_EXECCODESEGMENT:
		return 1+(g.pXParams ? ExecuteCodeSegment(g.pXParams,wp,(HWND)lp) : ERROR_INVALID_FUNCTION);
	case IPC_GETOUTERPROCESSTOKEN:
		if (_OpenProcessToken)
		{
			HANDLE hToken,hOutToken;
			if (_OpenProcessToken(GetCurrentProcess(),TOKEN_ALL_ACCESS,&hToken)) 
			{
				TRACEF("IPC_GETOUTERPROCESSTOKEN: hToken=%X targetProcess=%X\n",hToken,g.hElevatedProcess);
				if (DuplicateHandle(GetCurrentProcess(),hToken,g.hElevatedProcess,&hOutToken,lp,false,DUPLICATE_CLOSE_SOURCE))
				{
					TRACEF("IPC_GETOUTERPROCESSTOKEN: %X(%X) > %X(%X)\n",hToken,-1,hOutToken,g.hElevatedProcess);
					return (LRESULT)hOutToken;
				}
			}
		}
		return NULL;

#ifdef UAC_HACK_FGWND1
	case IPC_HACKFINDRUNAS: //super ugly hack to get title of run as dialog
		if (wp<200) 
		{
			HWND hRA=GetLastActivePopup((HWND)lp);
			TRACEF("IPC_HACKFINDRUNAS:%d %X %X\n",wp,lp,hRA);
			if (hRA && hRA !=(HWND)lp ) return PostMessage((HWND)lp,WM_APP,0,(LONG_PTR)hRA);
			Sleep(10);PostMessage(hwnd,uMsg,wp+1,lp);
		}
		break;
#endif
	}
	return DefWindowProc(hwnd,uMsg,wp,lp);
}


DWORD WINAPI IPCSrvThread(LPVOID lpParameter) 
{
	CoInitialize(0);
	const DWORD WStyle=WS_VISIBLE DBGONLY(|(WS_CAPTION));
	const int PosOffset=32700;
	MSG msg;
	WNDCLASS wc={0};
	wc.lpszClassName=GetIPCWndClass();
	wc.lpfnWndProc=IPCSrvWndProc;
	wc.hInstance=g.hInstance;
	if (!RegisterClass(&wc))goto dieLastErr;	
	if (!(g.hSrvIPC=CreateWindowEx(WS_EX_TOOLWINDOW DBGONLY(&~WS_EX_TOOLWINDOW),
		GetIPCWndClass(),
		DBGONLY(_T("Debug: NSIS.UAC")+)0,
		WStyle,
		-PosOffset DBGONLY(+PosOffset),-PosOffset DBGONLY(+PosOffset),DBGONLY(150+)1,DBGONLY(10+)1,
		0,0,wc.hInstance,0
		)))goto dieLastErr;		
	SetWindowLongPtr(g.hSrvIPC,GWLP_USERDATA,(LONG_PTR)lpParameter);
	TRACEF("IPCSrv=%X server created...\n",g.hSrvIPC);
	while (GetMessage(&msg,0,0,0)>0)DispatchMessage(&msg);
	SetLastError(NO_ERROR);
dieLastErr:
	CoUninitialize();
	return g.LastWaitExitCode=GetLastError();
}

DWORD InitIPC(HWND hwndNSIS,NSIS::extra_parameters*pXParams,UINT NSISStrLen) 
{
	if (g.threadIPC)return NO_ERROR;
	TRACEF("InitIPC StrSize=%u vs %u\n",NSIS::StrSize,NSISStrLen);
	DWORD tid;
	ASSERT(!g.pXParams && pXParams);
	ASSERT(NSISStrLen>0 && NSISStrLen==NSIS::StrSize);
	g.NSISStrLen=NSISStrLen;
	g.pXParams=pXParams;
	g.threadIPC=CreateThread(0,0,IPCSrvThread,hwndNSIS,0,&tid);
	if (g.threadIPC) 
	{
		while(!g.hSrvIPC && !g.LastWaitExitCode)Sleep(20);
		return g.hSrvIPC ? NO_ERROR : g.LastWaitExitCode;
	}
	return GetLastError();
}

void StopIPCSrv() 
{
	if (g.threadIPC) 
	{
		TRACEF("StopIPCSrv h=%X \n",g.hSrvIPC);
#ifdef UAC_HACK_Clammerz
		if ( GetSysVer()>=5 )
		{
			//WINBUGFIX: This ugly thing supposedly solves the problem of messagebox'es in .OnInit appearing behind other windows in Vista
			HWND hack=CreateWindowEx(WS_EX_TRANSPARENT|WS_EX_LAYERED,_T("Button"),NULL,NULL,0,0,0,0,NULL,NULL,NULL,0);
			ShowWindow(hack,SW_SHOW);
			SetForegroundWindow(hack);
			DestroyWindow(hack);		
		}
#endif
		PostMessage(g.hSrvIPC,WM_CLOSE,0,0);
		WaitForSingleObject(g.threadIPC,INFINITE);
		CloseHandle(g.threadIPC);
		UnregisterClass(GetIPCWndClass(),g.hInstance);//DLL can be loaded more than once, so make sure RegisterClass doesn't fail
		g.hSrvIPC=0;
		g.threadIPC=0;
	}
}

#ifdef UAC_HACK_FGWND1 
LRESULT CALLBACK HackWndSubProc(HWND hwnd,UINT Msg,WPARAM wp,LPARAM lp) 
{
	switch(Msg) 
	{
	case WM_APP:
		GetWindowText((HWND)lp,GetVar(0),NSIS::StrSize);
		if (*GetVar(0))SendMessage(hwnd,WM_SETTEXT,0,(LONG_PTR)GetVar(0));
		break;
	}
	return DefWindowProc(hwnd,Msg,wp,lp);
}
#endif

inline bool MustUseInternalRunAs() 
{ 
#ifdef BUILD_DBGSELECTELVMODE
	TCHAR dbgb[MAX_PATH*4];wsprintf(dbgb,_T("%s.ini"),GetVar(VIDX_EXEPATH));
	static int dbg_answer=GetPrivateProfileInt(_T("UACDBG"),_T("MustUseInternalRunAs"),2,dbgb);
	if (dbg_answer<2)return !!dbg_answer;WritePrivateProfileString(_T("UACDBG"),_T("MustUseInternalRunAs"),"",dbgb);
	if (MessageBox(GetActiveWindow(),"MustUseInternalRunAs?",dbgb,MB_YESNO)==IDYES)return true;
#endif
	return GetSysVer()>=6 && !SysElevationPresent(); 
}

DWORD ForkSelf(HWND hParent,DWORD&ForkExitCode,NSIS::extra_parameters*pXParams,UINT NSISStrLen) 
{
	DWORD ec=ERROR_ACCESS_DENIED;
	SHELLEXECUTEINFO sei={sizeof(sei)};
	//STARTUPINFO startInfo={sizeof(STARTUPINFO)};
	LPTSTR pszExePathBuf=0;
	LPTSTR pszParamBuf=0;
	LPTSTR p,pCL=GetCommandLine();
	UINT len;
	const DWORD OSVerMaj=GetOSVerMaj();
#ifdef UAC_HACK_FGWND1
	HWND hHack=0;
#endif
	ASSERT(pXParams);
	
	//GetStartupInfo(&startInfo);
	if (ec=InitIPC(hParent,pXParams,NSISStrLen))goto ret;
	ASSERT(IsWindow(g.hSrvIPC));
	sei.hwnd=hParent;
	sei.nShow=/*(startInfo.dwFlags&STARTF_USESHOWWINDOW) ? startInfo.wShowWindow :*/ SW_SHOWNORMAL;
	sei.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOZONECHECKS;
	sei.lpVerb=_T("runas");
	p=FindExePathEnd(pCL);
	len=p-pCL;
	if (!p || !len) 
	{
		ec=ERROR_FILE_NOT_FOUND;
		goto ret;
	}
	for (;;) 
	{
		NSIS::MemFree(pszExePathBuf);
		if (!(pszExePathBuf=(LPTSTR)NSIS::MemAlloc((++len)*sizeof(TCHAR))))goto dieOOM;
		if ( GetModuleFileName(0,pszExePathBuf,len) < len )break; //FUCKO: what if GetModuleFileName returns 0?
		len+=MAX_PATH;
	}
	sei.lpFile=pszExePathBuf;	
	len=lstrlen(p);
	len+=20;//space for "/UAC:xxxxxx /NCRC\0"
	if (!(pszParamBuf=(LPTSTR)NSIS::MemAlloc(len*sizeof(TCHAR))))goto dieOOM;
	wsprintf(pszParamBuf,_T("/UAC:%X /NCRC%s"),g.hSrvIPC,p);//NOTE: The argument parser depends on our special flag appearing first
	sei.lpParameters=pszParamBuf;


	if (OSVerMaj==5) 
	{
		bool hasseclogon=SysNT5IsSecondaryLogonSvcRunning();
		TRACEF("SysNT5IsSecondaryLogonSvcRunning=%d\n",hasseclogon);
		if (!hasseclogon) 
		{
			ec=ERROR_SERVICE_NOT_ACTIVE;
			goto ret;
		}
	}
	



#ifdef UAC_HACK_FGWND1
	if ( OSVerMaj>=5 && !sei.hwnd ) 
	{
		//sei.nShow=SW_SHOW;//forced, do we HAVE to do this?
		hHack=CreateWindowEx(WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_APPWINDOW,_T("Button"),GetVar(VIDX_EXEFILENAME),0|WS_MAXIMIZE,0,0,0,0,NULL,NULL,NULL,0);
		
		SetWindowLongPtr(hHack,GWLP_WNDPROC,(LONG_PTR)HackWndSubProc);
		if (GetSysVer()<6 || MustUseInternalRunAs())
			PostMessage(g.hSrvIPC,IPC_HACKFINDRUNAS,0,(LONG_PTR)hHack);
		else
			SetWindowLongPtr(hHack,GWL_EXSTYLE,GetWindowLongPtr(hHack,GWL_EXSTYLE)&~WS_EX_APPWINDOW);//kill taskbar btn on vista
		HICON hIcon=(HICON)LoadImage(GetModuleHandle(0),MAKEINTRESOURCE(103),IMAGE_ICON,0,0,LR_DEFAULTSIZE|LR_SHARED);	
		SendMessage(hHack,WM_SETICON,ICON_BIG,(LONG_PTR)hIcon);
		ShowWindow(hHack,SW_SHOW);
		SetForegroundWindow(hHack);
		sei.hwnd=hHack;
	}
#endif

	if (hParent)SetForegroundWindow(hParent);//try to force taskbar button active (for RunElevatedAndProcessMessages, really important on Vista)

#ifdef FEAT_CUSTOMRUNASDLG
	if (MustUseInternalRunAs()) 
	{ 
		ec=MyRunAs(g.hInstance,sei);
	}
	else 
#endif
	{
		if (GetSysVer()>=6) 
		{
			////////sei.nShow=SW_SHOW;
			//if ( _SupportsUAC() )sei.hwnd=0; //Vista does not like it when we provide a HWND
			//if (_AllowSetForegroundWindow) _AllowSetForegroundWindow(ASFW_ANY);//TODO: is GrantClientWindowInput() enough?
		}
#ifdef FEAT_MSRUNASDLGMODHACK
		void* hook=MSRunAsDlgMod_Init();
#endif
		TRACEF("ForkSelf:calling ShExec:app=%s|params=%s|vrb=%s|hwnd=%X\n",sei.lpFile,sei.lpParameters,sei.lpVerb,sei.hwnd);
		ec=(ShellExecuteEx(&sei)?NO_ERROR:GetLastError());
		TRACEF("ForkSelf: ShExec->Runas returned %d hInstApp=%d\n",ec,sei.hInstApp);
#ifdef FEAT_MSRUNASDLGMODHACK
		MSRunAsDlgMod_Unload(hook);
#endif
	}
#ifdef UAC_HACK_FGWND1
	DestroyWindow(hHack);
#endif
	if (ec)goto ret;
	TRACEF("ForkSelf: waiting for process %X (%s|%s|%s)sei.hwnd=%X\n",(sei.hProcess),sei.lpFile,sei.lpParameters,sei.lpVerb,sei.hwnd);
	ASSERT(sei.hProcess);
	ASSERT(NO_ERROR==ec);
	ShowWindow(g.hSrvIPC,SW_HIDE);

	g.hElevatedProcess=sei.hProcess;

	if (!IsWindow(sei.hwnd))
	{
		DWORD w=WaitForSingleObject(sei.hProcess,INFINITE);
		if (w==WAIT_OBJECT_0)
			VERIFY(GetExitCodeProcess(sei.hProcess,&ForkExitCode));
		else 
		{
			ec=GetLastError();
			TRACEF("ForkSelf:WaitForSingleObject failed ec=%d w=%d\n",ec,w);ASSERT(!"ForkSelf:WaitForSingleObject");
		}
	}
	else 
	{
		bool abortWait=false;
		const DWORD waitCount=1;
		const HANDLE handles[waitCount]={sei.hProcess};
		do 
		{
			DWORD w=MsgWaitForMultipleObjects(waitCount,handles,false,INFINITE,QS_ALLEVENTS|QS_ALLINPUT);
			switch(w)
			{
			case WAIT_OBJECT_0:
				VERIFY(GetExitCodeProcess(sei.hProcess,&ForkExitCode));
				abortWait=true;
				break;
			case WAIT_OBJECT_0+waitCount:
				{
					const HWND hwnd=sei.hwnd;
					MSG msg;
					while( !ec && PeekMessage(&msg,hwnd,0,0,PM_REMOVE) ) 
					{
						if (msg.message==WM_QUIT) 
						{
							ASSERT(0);
							ec=ERROR_CANCELLED;
							break;
						}
						if (!IsDialogMessage(hwnd,&msg)) 
						{
							TranslateMessage(&msg);
							DispatchMessage(&msg);
						}
					}
				}
				break;
			default:
				abortWait=true;
				ec=GetLastError();
				TRACEF("ForkSelf:MsgWaitForMultipleObjects failed, ec=%u w=%u\n",ec,w);
			}
		} while( NO_ERROR==ec && !abortWait );
	} 
	
	TRACEF("ForkSelf: wait complete, ec=%d forkexitcode=%u\n",ec,ForkExitCode);
	goto ret;
dieOOM:
	ec=ERROR_OUTOFMEMORY;
ret:
	StopIPCSrv();
	CloseHandle(sei.hProcess);
	NSIS::MemFree(pszExePathBuf);
	NSIS::MemFree(pszParamBuf);
	return ec;
}

bool GetIPCSrvWndFromParams(HWND&hwndOut) 
{
	LPTSTR p=FindExePathEnd(GetCommandLine());
	while(p && *p==' ')p=CharNext(p);TRACEF("GetIPCSrvWndFromParams:%s|\n",p);
	if (p && *p++=='/'&&*p++=='U'&&*p++=='A'&&*p++=='C'&&*p++==':') 
	{
		hwndOut=(HWND)StrToUInt(p,true);
		return !!IsWindow(hwndOut);
	}
	return false;
}


/*** RunElevated
Return:	r0:	Windows error code (0 on success, 1223 if user aborted elevation dialog, anything else should be treated as a fatal error)	
		r1: If r0==0, r1 is:
				0 if UAC is not supported by the OS, 
				1 if UAC was used to elevate and the current process should act like a wrapper (Call Quit in .OnInit without any further processing), 
				2 if the process is (already?) running @ HighIL (Member of admin group on other systems),
				3 if you should call RunElevated again (This can happen if a user without admin priv. is used in the runas dialog),
		r2: If r0==0 && r1==1: r2 is the ExitCode of the elevated fork process (The NSIS errlvl is also set to the ExitCode)
		r3: If r0==0: r3 is 1 if the user is a member of the admin group or 0 otherwise
*/
EXPORTNSISFUNC RunElevated(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);

	BYTE UACMode=0;
	bool UserIsAdmin=false;
	DWORD ec=ERROR_ACCESS_DENIED,ForkExitCode;
	TOKEN_ELEVATION_TYPE tet;
	
	UserIsAdmin=_IsAdmin();
	TRACEF("RunElevated:Init: IsAdmin=%d\n",UserIsAdmin);
#ifdef BUILD_XPTEST
	if (!UserIsAdmin)goto DbgXPAsUAC;//Skip UAC detection for special debug builds and force a call to runas on <NT6 systems
#endif
	if (!_SupportsUAC() && !SysSupportsRunAs())goto noUAC;
	if ((ec=DelayLoadDlls()))goto ret;
		
	if (GetIPCSrvWndFromParams(g.hSrvIPC)) 
	{
		if (ec=DllSelfAddRef())goto ret;
		if (!IsWindow(g.hSrvIPC))ec=ERRAPP_BADIPCSRV;
		UACMode=2;
		g.UseIPC=true;
		if (!UserIsAdmin) //Elevation used, but we are not Admin, let the wrapper know so it can try again...
		{ 		
			UACMode=0xFF;//Special invalid mode
			DWORD_PTR MsgRet;
			if (SendIPCMsg(IPC_ELEVATEAGAIN,0,0,MsgRet) || !MsgRet)ec=ERRAPP_BADIPCSRV;//if we could not notify the need for re-elevation, the IPCSrv must be bad
		}
		goto ret;
	}
	
	if ( (ec=DllSelfAddRef()) || (ec=_GetElevationType(&tet)) )goto ret;
	if ( tet==TokenElevationTypeFull || UserIsAdmin ) 
	{
		UserIsAdmin=true;
		UACMode=2;
		goto ret;
	}

	DBGONLY(DBG_RESETDBGVIEW());
	
#ifdef BUILD_XPTEST
DbgXPAsUAC:VERIFY(!DllSelfAddRef());
#endif
	//OS supports UAC and we need to elevate...
	ASSERT(!UserIsAdmin);
	UACMode=1;
	
	ec=ForkSelf(hwndNSIS,ForkExitCode,XParams,StrSize);
	if (!ec && !g.ElevateAgain) 
	{
		SetVarUINT(INST_2,ForkExitCode);
		SetErrLvl(XParams,ForkExitCode);
	}
	goto ret;
noUAC:
	ec=NO_ERROR;ASSERT(UACMode==0);
ret:
	if (ec==ERROR_CANCELLED) 
	{
		if (UACMode!=1)ec=ERROR_INVALID_FUNCTION;
		if (UACMode<2)g.UseIPC=false;
	}
	if (UACMode==0xFF && !ec) //We called IPC_ELEVATEAGAIN, so we need to quit so the wrapper gains control
	{
		ASSERT(g.UseIPC);
		UACMode=1;//We pretend to be the wrapper so Quit gets called in .OnInit
		SetErrLvl(XParams,0);
		_Unload();
	}
	if (g.ElevateAgain) 
	{
		ASSERT(!g.UseIPC);
		UACMode=3;//Fork called IPC_ELEVATEAGAIN, we need to change our UACMode so the wrapper(our instance) can try to elevate again if it wants to
	}
	if (!g.UseIPC)
		_Unload();//The wrapper can call quit in .OnInit without calling UAC::Unload, so we do it here
	
	SetVarUINT(INST_0,ec);
	SetVarUINT(INST_1,UACMode);
	SetVarUINT(INST_3,UserIsAdmin);
	TRACEF("RunElevated returning ec=%X UACMode=%d g.UseIPC=%d g.ElevateAgain=%d IsAdmin=%d\n",ec,UACMode,g.UseIPC,g.ElevateAgain,UserIsAdmin);

	NSISFUNCEND();
}



/*** Exec
Notes:
		¤ ErrorFlag is also set on error
STACK:	<ShowWindow> <File> <Parameters> <WorkingDir>
Return:	windows error code in r0, 0 on success
*/
EXPORTNSISFUNC Exec(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	HandleExecExport(true,false,hwndNSIS,StrSize,Vars,StackTop,XParams);
	NSISFUNCEND();
}

/*** ExecWait
Notes:
		¤ ErrorFlag is also set on error
STACK:	<ShowWindow> <File> <Parameters> <WorkingDir>
Return:	
		r0: windows error code, 0 on success
		r1: exitcode of new process 
*/
EXPORTNSISFUNC ExecWait(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	HandleExecExport(true,true,hwndNSIS,StrSize,Vars,StackTop,XParams);
	NSISFUNCEND();
}

/*** ShellExec
Notes:
		¤ ErrorFlag is also set on error
STACK:	<Verb> <ShowWindow> <File> <Parameters> <WorkingDir>
Return:	windows error code in r0, 0 on success
*/
EXPORTNSISFUNC ShellExec(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	HandleExecExport(false,false,hwndNSIS,StrSize,Vars,StackTop,XParams);
	NSISFUNCEND();
}

/*** ShellExecWait
Notes:
		¤ ErrorFlag is also set on error
STACK:	<Verb> <ShowWindow> <File> <Parameters> <WorkingDir>
Return:	
		r0: windows error code, 0 on success
		r1: exitcode of new process 
*/
EXPORTNSISFUNC ShellExecWait(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	HandleExecExport(false,true,hwndNSIS,StrSize,Vars,StackTop,XParams);
	NSISFUNCEND();
}


/*** GetElevationType
Notes:
		TokenElevationTypeDefault=1	:User is not using a split token (UAC disabled)
		TokenElevationTypeFull=2	:UAC enabled, the process is elevated
		TokenElevationTypeLimited=3	:UAC enabled, the process is not elevated
Return:	r0:	(TOKEN_ELEVATION_TYPE)TokenType, The error flag is set if the function fails and r0==0
*/
EXPORTNSISFUNC GetElevationType(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	TOKEN_ELEVATION_TYPE tet=(TOKEN_ELEVATION_TYPE)NULL; //Default to invalid value
	if (MaintainDllSelfRef() || /*!_SupportsUAC() ||*/ _GetElevationType(&tet)) SetErrorFlag(XParams);
	SetVarUINT(INST_0,tet);
	NSISFUNCEND();
}



/*** SupportsUAC
Notes:	Check if the OS supports UAC (And the user has UAC turned on)
Return:	r0:	(BOOL)Result 
*/
EXPORTNSISFUNC SupportsUAC(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	BOOL present=false;
	MaintainDllSelfRef();
	if (_SupportsUAC())present=true;	
	SetVarUINT(INST_0,present);
	NSISFUNCEND();
}


/*** IsAdmin
Return:	r0:	(BOOL)Result 
*/
EXPORTNSISFUNC IsAdmin(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	bool Admin=false;
	DWORD ec;
	if ( !(ec=MaintainDllSelfRef()) ) 
	{
		Admin=_IsAdmin();
		if ( ec=GetLastError() ) 
		{
			TRACEF("IsAdmin failed with %d\n",ec);
			SetErrorFlag(XParams);
			Admin=false;
		}
	}
	SetVarUINT(INST_0,Admin);
	NSISFUNCEND();
}



/*** ExecCodeSegment
Notes:	Sets error flag on error
		There is currently no way to transfer state to/from the executed code segment!
		If you use instructions that alter the UI or the stack/variables in the code segment (StrCpy,Push/Pop/Exch,DetailPrint,HideWindow etc.) they will affect the hidden wrapper installer and not "your" installer instance!
*/
EXPORTNSISFUNC ExecCodeSegment(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);

	DWORD ec;
	if (!(ec=DllSelfAddRef())) //force AddRef since our plugin could be called inside the executed code segment!
	{
		ec=ERROR_INVALID_PARAMETER;
		stack_t* pSI=StackPop();
		if (pSI) 
		{
			BOOL badCh;
			UINT pos=StrToUInt(pSI->text,false,&badCh);
			TRACEF("ExecCodeSegment %d (%s) badinput=%d\n",pos-1,pSI->text,badCh);
			if (!badCh && pos--) 
			{
				if (!g.UseIPC)
					ec=NSIS::ExecuteCodeSegment(XParams,pos);
				else 
				{
					SyncVars(hwndNSIS);
					DWORD_PTR MsgRet;
					AllowOuterInstanceWindowFocus();
					if (!(ec=SendIPCMsg(IPC_EXECCODESEGMENT,pos,0,MsgRet,INFINITE)))ec=MsgRet-1;
				}
			}
			StackFreeItem(pSI);
		}
	}
	if (ec)SetErrorFlag(XParams);

	NSISFUNCEND();
}



/*** StackPush   */
EXPORTNSISFUNC StackPush(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);

	DWORD ec;
	if (!(ec=DllSelfAddRef()) && g.UseIPC) 
	{
		stack_t* pSI=StackPop();
		ec=ERROR_INVALID_PARAMETER;
		if (pSI) 
		{
			DWORD_PTR MsgRet;
			COPYDATASTRUCT cds={CDI_STACKPUSH,(lstrlen(pSI->text)+1)*sizeof(NSISCH),pSI->text};
			if (!(ec=SendIPCMsg(WM_COPYDATA,(WPARAM)hwndNSIS,(LPARAM)&cds,MsgRet,5000 )))ec=MsgRet-1;
			StackFreeItem(pSI);
		}
	}
	if (ec)SetErrorFlag(XParams);

	NSISFUNCEND();
}



/*** GetOuterHwnd   */
EXPORTNSISFUNC GetOuterHwnd(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	MaintainDllSelfRef();
	DWORD_PTR MsgRet;HWND hSrvIPC;
	if (!GetIPCSrvWndFromParams(hSrvIPC)||!IsWindow(hSrvIPC)||SendIPCMsg(IPC_GETSRVHWND,0,0,MsgRet,IPCTOUT_DEF,hSrvIPC))MsgRet=0;
	SetVarUINT(INST_0,MsgRet);
	NSISFUNCEND();
}


DWORD SetPrivilege(LPCSTR PrivName,bool Enable)
{
	DWORD r=NO_ERROR;
	HANDLE hToken=NULL;
	TOKEN_PRIVILEGES TokenPrivs;
	if (!LookupPrivilegeValueA(NULL,PrivName,&TokenPrivs.Privileges[0].Luid))goto dieGLE;
	if (!_OpenProcessToken(GetCurrentProcess (),TOKEN_ADJUST_PRIVILEGES,&hToken))goto dieGLE;
	TokenPrivs.Privileges[0].Attributes = Enable ? SE_PRIVILEGE_ENABLED : 0 ;
	TokenPrivs.PrivilegeCount=1;
	if (AdjustTokenPrivileges(hToken,FALSE,&TokenPrivs,0,NULL,NULL))goto ret;
dieGLE:
	r=GetLastError();
ret:
	CloseHandle(hToken);
	return r;
}


#include <shlobj.h>
/*** GetShellFolderPath   */
EXPORTNSISFUNC GetShellFolderPath(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	/*
	WINBUG: 
	For some reason, even with debug priv. enabled, a call to OpenProcessToken(TOKEN_READ|TOKEN_IMPERSONATE)
	will fail, even if we have a PROCESS_ALL_ACCESS handle on XP when running as a member of the Power Users Group.
	So we have to ask the outer process to give us the handle.
	*/

	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	MaintainDllSelfRef();

	const unsigned TokenAccessRights=TOKEN_READ|TOKEN_IMPERSONATE;
	unsigned clsidFolder;
	HWND hSrvIPC;
	HANDLE hToken=NULL,hOuterProcess=NULL;
	DWORD SrvPID;
	HRESULT hr;
	FARPROC pfnSHGetFolderPath;
	LPTSTR buf=GetVar(INST_0);
	stack_t*pssCLSID=StackPop();
	stack_t*pssFallback=NULL;
	BOOL ConvBadCh;
	if (!pssCLSID || !(pssFallback=StackPop())) goto fail;
	clsidFolder=StrToUInt(pssCLSID->text,false,&ConvBadCh);
	if (ConvBadCh)goto fail;
	TRACEF("GetShellFolderPath HasIPCServer=%X param=%s>%X fallback=%s|\n",HasIPCServer(),pssCLSID->text,clsidFolder,pssFallback->text);

	pfnSHGetFolderPath=GetProcAddress(GetModuleHandle(_T("SHELL32")),"SHGetFolderPath"__DLD_FUNCSUFFIX);
	if (!pfnSHGetFolderPath)goto fail;

	if (GetIPCSrvWndFromParams(hSrvIPC) && 0 != GetWindowThreadProcessId(hSrvIPC,&SrvPID)) 
	{
		hOuterProcess=OpenProcess(PROCESS_QUERY_INFORMATION,false,SrvPID);
		if (hOuterProcess)
		{
			BOOL bOk=OpenProcessToken(hOuterProcess,TokenAccessRights,&hToken);
			CloseHandle(hOuterProcess);
			if (bOk)goto gotToken;

		}
/*		SetPrivilege(SE_DEBUG_NAME,true);
		hOuterProcess=OpenProcess(PROCESS_DUP_HANDLE,false,SrvPID);
		SetPrivilege(SE_DEBUG_NAME,false);
		if (!hOuterProcess)goto fail;
		TRACEF("hOuterProcess=%X\n",hOuterProcess);
*/		SendIPCMsg(IPC_GETOUTERPROCESSTOKEN,0,TokenAccessRights,(DWORD_PTR&)hToken,IPCTOUT_DEF,hSrvIPC);
		TRACEF("IPC_GETOUTERPROCESSTOKEN=%X\n",hToken);//*/
	}
gotToken:
	if (HasIPCServer() && !hToken)goto fail;

	hr=((HRESULT(WINAPI*)(HWND,int,HANDLE,DWORD,LPTSTR))pfnSHGetFolderPath)(hwndNSIS,clsidFolder,hToken,SHGFP_TYPE_CURRENT,buf);
	TRACEF("SHGetFolderPath hr=%X with token=%X, clsidFolder=%X|%s\n",hr,hToken,clsidFolder,buf);
	if (FAILED(hr)) goto fail; else goto ret;
fail:
	TRACEF("GetShellFolderPath GLE=%X\n",GetLastError());
	lstrcpy(buf,pssFallback->text);
	TRACEF("%s|%s\n",buf,pssFallback->text);
ret:
//	CloseHandle(hOuterProcess);
	CloseHandle(hToken);
	StackFreeItem(pssFallback);
	StackFreeItem(pssCLSID);
	NSISFUNCEND();
}



/*** GetOuterPID   * /
EXPORTNSISFUNC GetOuterPID(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop,NSIS::extra_parameters* XParams) 
{
	NSISFUNCSTART(hwndNSIS,StrSize,Vars,StackTop,XParams);
	MaintainDllSelfRef();
	DWORD_PTR Ret=0;
	HWND hSrvIPC;
	if (GetIPCSrvWndFromParams(hSrvIPC))
		if (0==GetWindowThreadProcessId(hSrvIPC,&Ret))Ret=0;
	SetVarUINT(INST_0,Ret);
	NSISFUNCEND();
}//*/



/*** Unload
Notes:	Call in .OnInstFailed AND .OnInstSuccess !
*/
EXPORTNSISFUNC Unload(HWND hwndNSIS,int StrSize,NSISCH*Vars,NSIS::stack_t **StackTop) 
{
	NSISFUNCSTART4(hwndNSIS,StrSize,Vars,StackTop);
	if (!MaintainDllSelfRef())_Unload(); else ASSERT(!"MaintainDllSelfRef failed in Unload!");
	NSISFUNCEND();
}



#ifdef _DEBUG
BOOL WINAPI DllMain(HINSTANCE hInst,DWORD Event,LPVOID lpReserved) 
#else
extern "C" BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInst,ULONG Event,LPVOID lpReserved) 
#endif
{
	if (Event==DLL_PROCESS_ATTACH) 
	{
		TRACEF("************************************ DllMain %u\n",GetCurrentProcessId());
		ASSERT(!_OpenProcessToken && !_EqualSid);
		g.hInstance=hInst;
	}
//	DBGONLY( if (Event==DLL_PROCESS_DETACH){ASSERT(g.DllRef==0);}TRACE("DLL_PROCESS_DETACH\n"); );//Make sure we unloaded so we don't lock $pluginsdir
	return TRUE;
}


