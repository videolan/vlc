;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Utils                                      ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Delete prefs Macro                ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

!macro delprefs
  StrCpy $0 0
  !define Index 'Line${__LINE__}'
  "${Index}-Loop:"
  ; FIXME
  ; this will loop through all the logged users and "virtual" windows users
  ; (it looks like users are only present in HKEY_USERS when they are logged in)
    ClearErrors
    EnumRegKey $1 HKU "" $0
    StrCmp $1 "" "${Index}-End"
    IntOp $0 $0 + 1
    ReadRegStr $2 HKU "$1\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" AppData
    StrCmp $2 "" "${Index}-Loop"
    RMDir /r "$2\vlc"
    Goto "${Index}-Loop"
  "${Index}-End:"
  !undef Index
!macroend

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Start VLC with dropped privileges ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function AppExecAs
  ${If} ${AtLeastWinVista}
    Exec '"$WINDIR\explorer.exe" "$INSTDIR\vlc.exe"'
  ${Else}
    Exec '$INSTDIR\vlc.exe'
  ${Endif}
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Parse the command line            ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function ParseCommandline
  ${GetParameters} $R0

  ${GetOptions} $R0 "/update" $R1

  ${If} ${Errors}
    StrCpy $PerformUpdate 0
  ${Else}
    StrCpy $PerformUpdate 1
  ${EndIf}
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Read previous version from        ;
; the registry                      ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function ReadPreviousVersion
  ReadRegStr $PreviousInstallDir HKLM "${PRODUCT_DIR_REGKEY}" "InstallDir"

  Call CheckPrevInstallDirExists

  ${If} $PreviousInstallDir != ""
    ;Detect version
    ReadRegStr $PreviousVersion HKLM "${PRODUCT_DIR_REGKEY}" "Version"
  ${EndIf}
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Check if the previous install     ;
; directory still exists            ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function CheckPrevInstallDirExists
  ${If} $PreviousInstallDir != ""

    ; Make sure directory is valid
    Push $R0
    Push $R1
    StrCpy $R0 "$PreviousInstallDir" "" -1
    ${If} $R0 == '\'
    ${OrIf} $R0 == '/'
      StrCpy $R0 $PreviousInstallDir*.*
    ${Else}
      StrCpy $R0 $PreviousInstallDir\*.*
    ${EndIf}
    ${IfNot} ${FileExists} $R0
      StrCpy $PreviousInstallDir ""
    ${EndIf}
    Pop $R1
    Pop $R0

  ${EndIf}
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Compare two versions and return   ;
; whether the new one is "newer",   ;
; the "same" or "older"             ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function VersionCompare
  Exch $1
  Exch
  Exch $0

  Push $2
  Push $3
  Push $4

versioncomparebegin:
  ${If} $0 == ""
  ${AndIf} $1 == ""
    StrCpy $PreviousVersionState "same"
    goto versioncomparedone
  ${EndIf}

  StrCpy $2 0
  StrCpy $3 0

  ; Parse rc / beta suffixes for segments
  StrCpy $4 $0 2
  ${If} $4 == "rc"
    StrCpy $2 100
    StrCpy $0 $0 "" 2
  ${Else}
    StrCpy $4 $0 4
    ${If} $4 == "beta"
      StrCpy $0 $0 "" 4
    ${Else}
      StrCpy $2 10000
    ${EndIf}
  ${EndIf}

  StrCpy $4 $1 2
  ${If} $4 == "rc"
    StrCpy $3 100
    StrCpy $1 $1 "" 2
  ${Else}
    StrCpy $4 $1 4
    ${If} $4 == "beta"
      StrCpy $1 $1 "" 4
    ${Else}
      StrCpy $3 10000
    ${EndIf}
  ${EndIf}

split1loop:
  StrCmp $0 "" split1loopdone
  StrCpy $4 $0 1
  StrCpy $0 $0 "" 1
  StrCmp $4 "." split1loopdone
  StrCmp $4 "-" split1loopdone
  StrCpy $2 $2$4
  goto split1loop
split1loopdone:

split2loop:
  StrCmp $1 "" split2loopdone
  StrCpy $4 $1 1
  StrCpy $1 $1 "" 1
  StrCmp $4 "." split2loopdone
  StrCmp $4 "-" split2loopdone
  StrCpy $3 $3$4
  goto split2loop
split2loopdone:

  ${If} $2 > $3
    StrCpy $PreviousVersionState "newer"
  ${ElseIf} $3 > $2
    StrCpy $PreviousVersionState "older"
  ${Else}
    goto versioncomparebegin
  ${EndIf}

versioncomparedone:

  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; TrimNewlines (copied from NSIS documentation)       ;
; input: top of stack  (e.g. whatever$\r$\n)          ;
; output: top of stack (replaces, with e.g. whatever) ;
; modifies no other variables.                        ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function un.TrimNewlines
 Exch $R0
 Push $R1
 Push $R2
 StrCpy $R1 0

 loop:
   IntOp $R1 $R1 - 1
   StrCpy $R2 $R0 1 $R1
   StrCmp $R2 "$\r" loop
   StrCmp $R2 "$\n" loop
   IntOp $R1 $R1 + 1
   IntCmp $R1 0 no_trim_needed
   StrCpy $R0 $R0 $R1

 no_trim_needed:
   Pop $R2
   Pop $R1
   Exch $R0
FunctionEnd

Function TrimNewlines
 Exch $R0
 Push $R1
 Push $R2
 StrCpy $R1 0

 loop:
   IntOp $R1 $R1 - 1
   StrCpy $R2 $R0 1 $R1
   StrCmp $R2 "$\r" loop
   StrCmp $R2 "$\n" loop
   IntOp $R1 $R1 + 1
   IntCmp $R1 0 no_trim_needed
   StrCpy $R0 $R0 $R1

 no_trim_needed:
   Pop $R2
   Pop $R1
   Exch $R0
FunctionEnd

Function un.RemoveEmptyDirs
  Pop $9
  !define Index 'Line${__LINE__}'
  FindFirst $0 $1 "$INSTDIR$9*"
  StrCmp $0 "" "${Index}-End"
  "${Index}-Loop:"
    StrCmp $1 "" "${Index}-End"
    StrCmp $1 "." "${Index}-Next"
    StrCmp $1 ".." "${Index}-Next"
    Push $0
    Push $1
    Push $9
    Push "$9$1\"
    Call un.RemoveEmptyDirs
    Pop $9
    Pop $1
    Pop $0
    RMDir "$INSTDIR$9$1"
    "${Index}-Next:"
    FindNext $0 $1
    Goto "${Index}-Loop"
  "${Index}-End:"
  FindClose $0
  !undef Index
FunctionEnd

Function RemoveEmptyDirs
  Pop $9
  !define Index 'Line${__LINE__}'
  FindFirst $0 $1 "$INSTDIR$9*"
  StrCmp $0 "" "${Index}-End"
  "${Index}-Loop:"
    StrCmp $1 "" "${Index}-End"
    StrCmp $1 "." "${Index}-Next"
    StrCmp $1 ".." "${Index}-Next"
    Push $0
    Push $1
    Push $9
    Push "$9$1\"
    Call RemoveEmptyDirs
    Pop $9
    Pop $1
    Pop $0
    RMDir "$INSTDIR$9$1"
    "${Index}-Next:"
    FindNext $0 $1
    Goto "${Index}-Loop"
  "${Index}-End:"
  FindClose $0
  !undef Index
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Check if VLC is running and kill it if necessary ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function CheckRunningProcesses
    ${nsProcess::FindProcess} "vlc.exe" $R0
    StrCmp $R0 0 0 end
    IfSilent +3
    BringToFront
    MessageBox MB_OKCANCEL|MB_ICONQUESTION "$(MessageBox_VLCRunning)" IDCANCEL stop

    ${nsProcess::CloseProcess} "vlc.exe" $R0
    IfSilent end
    StrCmp $R0 0 end 0      ; Success
    StrCmp $R0 603 end 0    ; Not running
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(MessageBox_VLCUnableToClose)"
    goto end

    stop:
    ${nsProcess::Unload}
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(MessageBox_InstallAborted)"
    Quit

    end:
    ${nsProcess::Unload}
FunctionEnd
