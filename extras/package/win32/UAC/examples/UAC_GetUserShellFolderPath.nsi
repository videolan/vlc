RequestExecutionLevel user /* RequestExecutionLevel REQUIRED! */
!define APPNAME "UAC_GetUserShellFolderPath"
Name "${APPNAME}"
OutFile "${APPNAME}.exe"
ShowInstDetails show

!include UAC.nsh
!include LogicLib.nsh

page instfiles

Function .onInit
${UAC.I.Elevate.AdminOnly}
FunctionEnd

!ifndef CSIDL_PERSONAL
	!define CSIDL_PERSONAL 0x0005 ;My Documents
!endif
Section

/*
You can specify a fallback value in the 2nd parameter, it is used if the installer is not elevated 
or running on NT4/Win9x or on errors.
If you just want to check for success, use "" as the 2nd parameter and compare $0 with "" 
*/
UAC::GetShellFolderPath ${CSIDL_PERSONAL} $Documents
DetailPrint MyDocs=$0


SectionEnd
