/*
This sample supports two modes, installing as a normal user (single user install) AND as admin (all users install)
This sample uses the registry plugin, so you need to download that if you don't already have it
*/

!define APPNAME "UAC_RealWorldFullyLoadedDualMode"
!define ELEVATIONTITLE "${APPNAME}: Elevate" ;displayed during elevation on our custom page
!define SMSUBDIR $StartMenuFolder ;"${APPNAME}"
!define UNINSTALLER_NAME "Uninstall ${APPNAME}.exe"
!define UNINSTALLER_REGSECTION "${APPNAME}"
!define RegPath.MSUninstall "Software\Microsoft\Windows\CurrentVersion\Uninstall"
Name "${APPNAME}"
OutFile "${APPNAME}.exe"
ShowInstDetails show
SetCompressor LZMA
RequestExecutionLevel user /* RequestExecutionLevel REQUIRED! */
!include MUI.nsh
!include UAC.nsh
!include LogicLib.nsh
!include Registry.nsh
!include nsDialogs.nsh	;for our custom page
!include FileFunc.nsh	;we need to parse the command line

!insertmacro GetParameters
!insertmacro GetOptions

!define MUI_CUSTOMFUNCTION_ABORT onAbort
!define MUI_CUSTOMFUNCTION_GUIINIT onGuiInit
!define MUI_COMPONENTSPAGE_NODESC
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\llama-blue.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\llama-blue.ico"
!define MUI_WELCOMEPAGE_TITLE_3LINES

var InstMode 	# 0: Single user, 1:All users, >1:elevated instance, perform page jump
var hKey 		# Reg hive
var hSelModeAdminRadio
var StartMenuFolder

!macro SetMode IsAdmin
!if "${IsAdmin}" > 0
	SetShellVarContext all
	StrCpy $InstMode 1
	StrCpy $hKey HKLM
	!else
	SetShellVarContext current
	StrCpy $InstMode 0
	StrCpy $hKey HKCU
	!endif
!macroend

Function .OnInit
!insertmacro SetMode 0
${GetParameters} $R9
${GetOptions} "$R9" UAC $0 ;look for special /UAC:???? parameter (sort of undocumented)
${Unless} ${Errors}
	UAC::IsAdmin
	${If} $0 < 1 
		SetErrorLevel 666 ;special return value for outer instance so it knows we did not have admin rights
		Quit 
		${EndIf}
	!insertmacro SetMode 1
	StrCpy $InstMode 2
	${EndIf}
FunctionEnd

Function onGuiInit
${If} $InstMode >= 2
	${UAC.GetOuterInstanceHwndParent} $0
	${If} $0 <> 0 
		System::Call /NOUNLOAD "*(i,i,i,i)i.r1"
		System::Call /NOUNLOAD 'user32::GetWindowRect(i $0,i r1)i.r2'
		${If} $2 <> 0
			System::Call /NOUNLOAD "*$1(i.r2,i.r3)"
			System::Call /NOUNLOAD 'user32::SetWindowPos(i $hwndParent,i0,ir2,ir3,i0,i0,i 4|1)'
			${EndIf}
		ShowWindow $hwndParent ${SW_SHOW}
		ShowWindow $0 ${SW_HIDE} ;hide outer instance installer window
		System::Free $1
		${EndIf}
	${EndIf}
FunctionEnd

Function Un.OnInit
!insertmacro SetMode 0
ReadRegDWORD $0 HKLM "${RegPath.MSUninstall}\${UNINSTALLER_REGSECTION}" InstMode ;We saved the "mode" in the installer
${If} $0 U> 0
	; If it was installed for all users, we have to be admin to uninstall it
	${UAC.U.Elevate.AdminOnly} "${UNINSTALLER_NAME}"
	!insertmacro SetMode 1
	${EndIf}
FunctionEnd

Function onAbort
${UAC.Unload}
FunctionEnd

${UAC.AutoCodeUnload} 1 ;Auto-generate .OnInstFailed and .OnInstSuccess functions

!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipPageInElvModePreCB
!insertmacro MUI_PAGE_WELCOME
Page Custom ModeSelectionPageCreate ModeSelectionPageLeave
!define MUI_PAGE_CUSTOMFUNCTION_PRE CmpntsPreCB
!insertmacro MUI_PAGE_COMPONENTS
!define MUI_PAGE_CUSTOMFUNCTION_PRE DirPreCB
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU 1 $StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION FinishRunCB
!insertmacro MUI_PAGE_FINISH
!define MUI_WELCOMEPAGE_TITLE_3LINES
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function CmpntsPreCB
GetDlgItem $0 $hwndparent 3
${IfThen} $InstMode >= 1 ${|} EnableWindow $0 0 ${|} ;prevent user from going back and selecting single user so noobs don't end up installing as the wrong user
FunctionEnd

Function SkipPageInElvModePreCB
${IfThen} $InstMode > 1 ${|} Abort ${|} ;skip this page so we get to the mode selection page
FunctionEnd

Function ModeSelectionPageCreate
${If} $InstMode > 1 
	StrCpy $InstMode 1
	Abort ;skip this page and contine where the "parent" would have gone
	${EndIf}
!insertmacro MUI_HEADER_TEXT_PAGE "Select install type" "Blah blah blah blah"
nsDialogs::Create /NOUNLOAD 1018
Pop $0
${NSD_CreateLabel} 0 20u 75% 20u "Blah blah blah blah select install type..."
Pop $0
System::Call "advapi32::GetUserName(t.r0, *i ${NSIS_MAX_STRLEN}r1) i.r2"
${NSD_CreateRadioButton} 0 40u 75% 15u "Single User ($0)"
Pop $0
${IfThen} $InstMode U< 1 ${|} SendMessage $0 ${BM_SETCHECK} 1 0 ${|}
${NSD_CreateRadioButton} 0 60u 75% 15u "All users"
Pop $hSelModeAdminRadio
${IfThen} $InstMode U> 0 ${|} SendMessage $hSelModeAdminRadio ${BM_SETCHECK} 1 0 ${|}
nsDialogs::Show
FunctionEnd

!macro EnableCtrl dlg id state
push $language
GetDlgItem $language ${dlg} ${id}
EnableWindow $language ${state}
pop $language
!macroend

Function ModeSelectionPageLeave
SendMessage $hSelModeAdminRadio ${BM_GETCHECK} 0 0 $9
UAC::IsAdmin
${If} $9 U> 0
	${If} $0 <> 0
		!insertmacro SetMode 1
	${Else}
		System::Call /NoUnload 'user32::GetWindowText(i $HwndParent,t.R1,i ${NSIS_MAX_STRLEN})' ;get original window title
		System::Call /NoUnload 'user32::SetWindowText(i $HwndParent,t "${ELEVATIONTITLE}")' ;set out special title
		StrCpy $2 "" ;reset special return, only gets set when sub process is executed, not when user cancels
		!insertmacro EnableCtrl $HWNDParent 1 0 ;Disable next button, just because it looks good ;)
		${UAC.RunElevatedAndProcessMessages}
		!insertmacro EnableCtrl $HWNDParent 1 1
		System::Call 'user32::SetWindowText(i $HwndParent,t "$R1")' ;restore title
		${If} $2 = 666 ;our special return, the new process was not admin after all 
			MessageBox mb_iconExclamation "You need to login with an account that is a member of the admin group to continue..."
			Abort 
		${ElseIf} $0 = 1223 ;cancel
			Abort
		${Else}
			${If} $0 <> 0
				${If} $0 = 1062
					MessageBox mb_iconstop "Unable to elevate, Secondary Logon service not running!"
				${Else}
					MessageBox mb_iconstop "Unable to elevate, error $0 ($1,$2,$3)"
				${EndIf} 
				Abort
			${EndIf}
		${EndIf} 
		Quit ;We now have a new process, the install will continue there, we have nothing left to do here
	${EndIf}
${EndIf}
FunctionEnd

Function DirPreCB
${If} $InstDir == ""
	${If} $InstMode U> 0
		StrCpy $InstDir "$ProgramFiles\${APPNAME}"
		${Else}
		StrCpy $InstDir "$APPDATA\${APPNAME}"
		${EndIf}
	${EndIf}
FunctionEnd

Function FinishRunCB
UAC::Exec "" "Notepad.exe" "$Windir\Win.INI" "$InstDir"
FunctionEnd

Function CreateSMShortcuts
StrCpy ${SMSUBDIR} $9 ;stupid sync
CreateDirectory "$SMPrograms\${SMSUBDIR}"
CreateShortcut "$SMPrograms\${SMSUBDIR}\${APPNAME}.lnk" "$Windir\Notepad.exe"
CreateShortcut "$SMPrograms\${SMSUBDIR}\Uninstall ${APPNAME}.lnk" "$InstDir\${UNINSTALLER_NAME}"
FunctionEnd
Function CreateDeskShortcuts
CreateShortcut "$Desktop\${APPNAME}.lnk" "$Windir\Notepad.exe"
FunctionEnd

Section "!Required files"
SectionIn RO
SetOutPath -
!insertmacro _UAC.DbgDetailPrint ;some debug info, useful during testing
;Install files here...
WriteUninstaller "$InstDir\${UNINSTALLER_NAME}"
${registry::Write} "$hKey\${RegPath.MSUninstall}\${UNINSTALLER_REGSECTION}" DisplayName "${APPNAME}" REG_SZ $0
${registry::Write} "$hKey\${RegPath.MSUninstall}\${UNINSTALLER_REGSECTION}" UninstallString "$InstDir\${UNINSTALLER_NAME}" REG_SZ $0
${registry::Write} "$hKey\${RegPath.MSUninstall}\${UNINSTALLER_REGSECTION}" InstMode $InstMode REG_DWORD $0
${registry::Unload}
SectionEnd

Section "Startmenu Shortcuts"
StrCpy $9 ${SMSUBDIR} ;this is stupid as hell, we need correct ${SMSUBDIR} in the outer process, this is the only way (plugins cannot enum "custom" var's AFAIK)
${UAC.CallFunctionAsUser} CreateSMShortcuts
SectionEnd
Section "Desktop Shortcut"
${UAC.CallFunctionAsUser} CreateDeskShortcuts
SectionEnd

Section Uninstall
Delete "$InstDir\${UNINSTALLER_NAME}"
Delete "$SMPrograms\${SMSUBDIR}\${APPNAME}.lnk"
Delete "$SMPrograms\${SMSUBDIR}\Uninstall ${APPNAME}.lnk"
RMDir "$SMPrograms\${SMSUBDIR}"
Delete "$Desktop\${APPNAME}.lnk"

RMDir "$InstDir"
${registry::DeleteKey} "$hKey\${RegPath.MSUninstall}\${UNINSTALLER_REGSECTION}" $0
${registry::Unload}
SectionEnd