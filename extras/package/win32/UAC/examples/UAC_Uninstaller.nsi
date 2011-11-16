/*
This script was made in response to http://forums.winamp.com/showthread.php?threadid=280330
It is a ugly hack and is mostly here just to have a solution right now.
Hopefully, NSIS will add support for changing the RequestExecutionLevel of the uninstaller
This code inspired the _UAC.GenerateUninstallerTango macro (called by ${UAC.U.Elevate.AdminOnly} unless you define UAC_DISABLEUNINSTALLERTANGO)
*/

RequestExecutionLevel user /* RequestExecutionLevel REQUIRED! */
!define APPNAME "UAC_Uninstaller"
Name "${APPNAME}"
OutFile "${APPNAME}.exe"
ShowInstDetails show
!include LogicLib.nsh

!define UNINSTALLER_UACDATA "uac.ini"
!define UNINSTALLER_NAME "Uninstall FooBarBaz"

Function un.onInit
ReadIniStr $0 "$ExeDir\${UNINSTALLER_UACDATA}" UAC "Un.First"
${IF} $0 != 1
	;SetSilent silent
	InitPluginsDir
	WriteIniStr "$PluginsDir\${UNINSTALLER_UACDATA}" UAC "Un.First" 1
	CopyFiles /SILENT "$EXEPATH" "$PluginsDir\${UNINSTALLER_NAME}.exe"
	StrCpy $0 ""
	${IfThen} ${Silent} ${|} StrCpy $0 "/S " ${|}
	ExecWait '"$PluginsDir\${UNINSTALLER_NAME}.exe" $0/NCRC _?=$INSTDIR' $0
	SetErrorLevel $0
	Quit
	${EndIf}

# UAC code goes here ...
FunctionEnd

Section
WriteUninstaller "$exedir\${UNINSTALLER_NAME}.exe"
SetAutoClose true
DetailPrint "Uninstalling..."
Sleep 1111
Exec '"$exedir\${UNINSTALLER_NAME}.exe"'
SectionEnd

Section uninstall
MessageBox mb_ok "My filename is: $EXEFILE"
Delete "$instdir\${UNINSTALLER_NAME}.exe"
Delete "$instdir\${APPNAME}.exe" ;delete generated installer aswell, this is just a sample script
SectionEnd

page InstFiles