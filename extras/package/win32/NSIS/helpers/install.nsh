;;;;;;;;;;;;;;;
; 4. Logging  ;
;;;;;;;;;;;;;;;
!macro OpenUninstallLog
  FileOpen $UninstallLog "$INSTDIR\uninstall.log" a
  FileSeek $UninstallLog 0 END
!macroend

!macro CloseUninstallLog
  FileClose $UninstallLog
  SetFileAttributes "$INSTDIR\uninstall.log" HIDDEN
!macroend

;;;;;;;;;;;;;;;;;;;;
; 5. Installations ;
;;;;;;;;;;;;;;;;;;;;
!macro InstallFile FILEREGEX
  File "${FILEREGEX}"
  !define Index 'Line${__LINE__}'
  FindFirst $0 $1 "$INSTDIR\${FILEREGEX}"
  StrCmp $0 "" "${Index}-End"
  "${Index}-Loop:"
    StrCmp $1 "" "${Index}-End"
    FileWrite $UninstallLog "$1$\r$\n"
    FindNext $0 $1
    Goto "${Index}-Loop"
  "${Index}-End:"
  FindClose $0
  !undef Index
!macroend

!macro InstallFolder FOLDER
  SetOutPath "$INSTDIR\${FOLDER}"
  File /r "${FOLDER}\*.*"
  SetOutPath "$INSTDIR"
  Push "${FOLDER}"
  Call InstallFolderInternal
!macroend

!macro InstallFolderOptional FOLDER
  SetOutPath "$INSTDIR\${FOLDER}"
  File /nonfatal /r "${FOLDER}\*.*"
  SetOutPath "$INSTDIR"
  Push "${FOLDER}"
  Call InstallFolderInternal
!macroend

Function InstallFolderInternal
  Pop $9
  !define Index 'Line${__LINE__}'
  FindFirst $0 $1 "$INSTDIR\$9\*"
  StrCmp $0 "" "${Index}-End"
  "${Index}-Loop:"
    StrCmp $1 "" "${Index}-End"
    StrCmp $1 "." "${Index}-Next"
    StrCmp $1 ".." "${Index}-Next"
    IfFileExists "$9\$1\*" 0 "${Index}-Write"
      Push $0
      Push $9
      Push "$9\$1"
      Call InstallFolderInternal
      Pop $9
      Pop $0
      Goto "${Index}-Next"
    "${Index}-Write:"
    FileWrite $UninstallLog "$9\$1$\r$\n"
    "${Index}-Next:"
    FindNext $0 $1
    Goto "${Index}-Loop"
  "${Index}-End:"
  FindClose $0
  !undef Index
FunctionEnd
;;; End of Macros


