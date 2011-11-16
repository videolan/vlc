/*
This sample will try to elevate, but it will also allow non admin users to continue if they click cancel in the elevation dialog
*/

RequestExecutionLevel user /* RequestExecutionLevel REQUIRED! */
!define APPNAME "UAC_AllowLUA"
Name "${APPNAME}"
OutFile "${APPNAME}.exe"
ShowInstDetails show
!include UAC.nsh


Function .OnInstFailed
UAC::Unload ;Must call unload!
FunctionEnd
Function .OnInstSuccess
UAC::Unload ;Must call unload!
FunctionEnd

Function .OnInit
UAC::RunElevated 
;MessageBox mb_iconinformation "Debug: UAC::RunElevated: $\n0(Error)=$0 $\n1(UACMode)=$1 $\n2=$2 $\nadmin=$3$\n$\n$CmdLine"
StrCmp 1223 $0 UAC_ElevationAborted ; UAC dialog aborted by user?
StrCmp 0 $0 0 UAC_Err ; Error?
StrCmp 1 $1 0 UAC_Success ;Are we the real deal or just the wrapper?
Quit
UAC_Err:
MessageBox mb_iconstop "Unable to elevate , error $0"
Abort
UAC_ElevationAborted:
# elevation was aborted, we still run as normal
UAC_Success:
FunctionEnd



Section "Info"
!insertmacro _UAC.DbgDetailPrint
SectionEnd





Page InstFiles
