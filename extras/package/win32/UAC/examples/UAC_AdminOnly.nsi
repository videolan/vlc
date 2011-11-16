RequestExecutionLevel user /* RequestExecutionLevel REQUIRED! */
!define APPNAME "UAC_AdminOnly"
Name "${APPNAME}"
OutFile "${APPNAME}.exe"
ShowInstDetails show

!include UAC.nsh ;<<< New headerfile that does everything for you ;)
!include LogicLib.nsh

!define UACSTR.I.ElvAbortReqAdmin "This fancy app requires admin rights fool" ;custom error string, see _UAC.InitStrings macro in uac.nsh for more

Function .OnInit
${UAC.I.Elevate.AdminOnly}
FunctionEnd

Function .OnInstFailed
${UAC.Unload}
FunctionEnd
Function .OnInstSuccess
${UAC.Unload}
FunctionEnd

Function ExecCodeSegmentTest
${If} "$1" != "666, the # of the beast"
	MessageBox mb_ok "uh oh"
	${EndIf}
FunctionEnd

Section "Info"
!insertmacro _UAC.DbgDetailPrint

StrCpy $1 "666, the # of the beast"
!insertmacro UAC.CallFunctionAsUser ExecCodeSegmentTest
SectionEnd

page InstFiles

/* LEGACY CODE: (now uses magic code from UAC.nsh)
Function .OnInit
UAC_Elevate:
UAC::RunElevated 
StrCmp 1223 $0 UAC_ElevationAborted ; UAC dialog aborted by user?
StrCmp 0 $0 0 UAC_Err ; Error?
StrCmp 1 $1 0 UAC_Success ;Are we the real deal or just the wrapper?
Quit
UAC_Err:
MessageBox mb_iconstop "Unable to elevate , error $0"
Abort
UAC_ElevationAborted:
/*System::Call "user32::CreateWindowEx(i ${WS_EX_TRANSPARENT}|${WS_EX_LAYERED}, t 'Button', t 'blah', i 0, i 10, i 10, i 10, i 10, i 0, i 0, i 0) i .r0"
ShowWindow $0 ${SW_SHOW}
System::Call "user32::SetForegroundWindow(i r0) i."
System::Call "user32::DestroyWindow(i r0) i."
* /
MessageBox mb_iconstop "This installer requires admin access, aborting!"
Abort
UAC_Success:
StrCmp 1 $3 +4 ;Admin?
StrCmp 3 $1 0 UAC_ElevationAborted ;Try again or abort?
MessageBox mb_iconstop "This installer requires admin access, try again" ;Inform user...
goto UAC_Elevate ;... and try again
FunctionEnd*/