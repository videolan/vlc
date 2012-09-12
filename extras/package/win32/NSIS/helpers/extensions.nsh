;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 1. File type associations ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Function that registers one extension for VLC
Function RegisterExtension
  ; back up old value for extension $R0 (eg. ".opt")
  ReadRegStr $1 HKCR "$R0" ""
  StrCmp $1 "" NoBackup
    StrCmp $1 "VLC$R0" "NoBackup"
    WriteRegStr HKCR "$R0" "VLC.backup" $1
NoBackup:
  WriteRegStr HKCR "$R0" "" "VLC$R0"
  ReadRegStr $0 HKCR "VLC$R0" ""
  WriteRegStr HKCR "VLC$R0" "" "VLC media file ($R0)"
  WriteRegStr HKCR "VLC$R0\shell" "" "Open"
  WriteRegStr HKCR "VLC$R0\shell\Open" "" $ShellAssociation_Play
  WriteRegStr HKCR "VLC$R0\shell\Open" "MultiSelectModel" "Player"
  WriteRegStr HKCR "VLC$R0\shell\Open\command" "" '"$INSTDIR\vlc.exe" --started-from-file "%1"'
  WriteRegStr HKCR "VLC$R0\DefaultIcon" "" '"$INSTDIR\vlc.exe",0'

;;; Vista Only part
  ; Vista and above detection
  ReadRegStr $R1 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCpy $R2 $R1 1
  StrCmp $R2 '6' ForVista ToEnd
ForVista:
  WriteRegStr HKLM "Software\Clients\Media\VLC\Capabilities\FileAssociations" "$R0" "VLC$R0"

ToEnd:
FunctionEnd

;; Function that registers one skin extension for VLC
Function RegisterSkinExtension
  ; back up old value for extension $R0 (eg. ".opt")
  ReadRegStr $1 HKCR "$R0" ""
  StrCmp $1 "" NoBackup
    StrCmp $1 "VLC$R0" "NoBackup"
    WriteRegStr HKCR "$R0" "VLC.backup" $1
NoBackup:
  WriteRegStr HKCR "$R0" "" "VLC$R0"
  ReadRegStr $0 HKCR "VLC$R0" ""
  WriteRegStr HKCR "VLC$R0" "" "VLC skin file ($R0)"
  WriteRegStr HKCR "VLC$R0\shell" "" "Open"
  WriteRegStr HKCR "VLC$R0\shell\Open" "" ""
  WriteRegStr HKCR "VLC$R0\shell\Open\command" "" '"$INSTDIR\vlc.exe" -Iskins --skins2-last "%1"'
  WriteRegStr HKCR "VLC$R0\DefaultIcon" "" '"$INSTDIR\vlc.exe",0'

;;; Vista Only part
  ; Vista and above detection
  ReadRegStr $R1 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCpy $R2 $R1 1
  StrCmp $R2 '6' ForVista ToEnd
ForVista:
  WriteRegStr HKLM "Software\Clients\Media\VLC\Capabilities\FileAssociations" "$R0" "VLC$R0"

ToEnd:
FunctionEnd

;; Function that removes one extension that VLC owns.
Function un.RegisterExtension
  ;start of restore script
  ReadRegStr $1 HKCR "$R0" ""
  StrCmp $1 "VLC$R0" 0 NoOwn ; only do this if we own it
    ; Read the old value from Backup
    ReadRegStr $1 HKCR "$R0" "VLC.backup"
    StrCmp $1 "" 0 Restore ; if backup="" then delete the whole key
      DeleteRegKey HKCR "$R0"
    Goto NoOwn
Restore:
      WriteRegStr HKCR "$R0" "" $1
      DeleteRegValue HKCR "$R0" "VLC.backup"
NoOwn:
    DeleteRegKey HKCR "VLC$R0" ;Delete key with association settings
    DeleteRegKey HKLM "Software\Clients\Media\VLC\Capabilities\FileAssociations\VLC$R0" ; for vista
FunctionEnd

!macro RegisterExtensionSection EXT
  Section ${EXT}
    SectionIn 1 3
    Push $R0
    StrCpy $R0 ${EXT}
    Call RegisterExtension
    Pop $R0
  SectionEnd
!macroend

!macro RegisterSkinExtensionSection EXT
  Section /o ${EXT}
    SectionIn 1 3
    Push $R0
    StrCpy $R0 ${EXT}
    Call RegisterSkinExtension
    Pop $R0
  SectionEnd
!macroend

!macro UnRegisterExtensionSection EXT
  Push $R0
  StrCpy $R0 ${EXT}
  Call un.RegisterExtension
  Pop $R0
!macroend

!macro WriteRegStrSupportedTypes EXT
  WriteRegStr HKCR Applications\vlc.exe\SupportedTypes ${EXT} ""
!macroend

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Extension lists  Macros                    ;
; Those macros calls the previous functions  ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

!macro MacroAudioExtensions _action
  !insertmacro ${_action} ".3ga"
  !insertmacro ${_action} ".669"
  !insertmacro ${_action} ".a52"
  !insertmacro ${_action} ".aac"
  !insertmacro ${_action} ".ac3"
  !insertmacro ${_action} ".adt"
  !insertmacro ${_action} ".adts"
  !insertmacro ${_action} ".aif"
  !insertmacro ${_action} ".aifc"
  !insertmacro ${_action} ".aiff"
  !insertmacro ${_action} ".au"
  !insertmacro ${_action} ".amr"
  !insertmacro ${_action} ".aob"
  !insertmacro ${_action} ".ape"
  !insertmacro ${_action} ".caf"
  !insertmacro ${_action} ".cda"
  !insertmacro ${_action} ".dts"
  !insertmacro ${_action} ".flac"
  !insertmacro ${_action} ".it"
  !insertmacro ${_action} ".m4a"
  !insertmacro ${_action} ".m4p"
  !insertmacro ${_action} ".mid"
  !insertmacro ${_action} ".mka"
  !insertmacro ${_action} ".mlp"
  !insertmacro ${_action} ".mod"
  !insertmacro ${_action} ".mp1"
  !insertmacro ${_action} ".mp2"
  !insertmacro ${_action} ".mp3"
  !insertmacro ${_action} ".mpc"
  !insertmacro ${_action} ".mpga"
  !insertmacro ${_action} ".oga"
  !insertmacro ${_action} ".oma"
  !insertmacro ${_action} ".opus"
  !insertmacro ${_action} ".qcp"
  !insertmacro ${_action} ".ra"
  !insertmacro ${_action} ".rmi"
  !insertmacro ${_action} ".snd"
  !insertmacro ${_action} ".s3m"
  !insertmacro ${_action} ".spx"
  !insertmacro ${_action} ".tta"
  !insertmacro ${_action} ".voc"
  !insertmacro ${_action} ".vqf"
  !insertmacro ${_action} ".w64"
  !insertmacro ${_action} ".wav"
  !insertmacro ${_action} ".wma"
  !insertmacro ${_action} ".wv"
  !insertmacro ${_action} ".xa"
  !insertmacro ${_action} ".xm"
!macroend

!macro MacroVideoExtensions _action
  !insertmacro ${_action} ".3g2"
  !insertmacro ${_action} ".3gp"
  !insertmacro ${_action} ".3gp2"
  !insertmacro ${_action} ".3gpp"
  !insertmacro ${_action} ".amv"
  !insertmacro ${_action} ".asf"
  !insertmacro ${_action} ".avi"
  !insertmacro ${_action} ".divx"
  !insertmacro ${_action} ".drc"
  !insertmacro ${_action} ".dv"
  !insertmacro ${_action} ".f4v"
  !insertmacro ${_action} ".flv"
  !insertmacro ${_action} ".gvi"
  !insertmacro ${_action} ".gxf"
  !insertmacro ${_action} ".m1v"
  !insertmacro ${_action} ".m2t"
  !insertmacro ${_action} ".m2v"
  !insertmacro ${_action} ".m2ts"
  !insertmacro ${_action} ".m4v"
  !insertmacro ${_action} ".mkv"
  !insertmacro ${_action} ".mov"
  !insertmacro ${_action} ".mp2"
  !insertmacro ${_action} ".mp2v"
  !insertmacro ${_action} ".mp4"
  !insertmacro ${_action} ".mp4v"
  !insertmacro ${_action} ".mpa"
  !insertmacro ${_action} ".mpe"
  !insertmacro ${_action} ".mpeg"
  !insertmacro ${_action} ".mpeg1"
  !insertmacro ${_action} ".mpeg2"
  !insertmacro ${_action} ".mpeg4"
  !insertmacro ${_action} ".mpg"
  !insertmacro ${_action} ".mpv2"
  !insertmacro ${_action} ".mts"
  !insertmacro ${_action} ".mtv"
  !insertmacro ${_action} ".mxf"
  !insertmacro ${_action} ".nsv"
  !insertmacro ${_action} ".nuv"
  !insertmacro ${_action} ".ogg"
  !insertmacro ${_action} ".ogm"
  !insertmacro ${_action} ".ogx"
  !insertmacro ${_action} ".ogv"
  !insertmacro ${_action} ".rec"
  !insertmacro ${_action} ".rm"
  !insertmacro ${_action} ".rmvb"
  !insertmacro ${_action} ".tod"
  !insertmacro ${_action} ".ts"
  !insertmacro ${_action} ".tts"
  !insertmacro ${_action} ".vob"
  !insertmacro ${_action} ".vro"
  !insertmacro ${_action} ".webm"
  !insertmacro ${_action} ".wmv"
  !insertmacro ${_action} ".xesc"
!macroend

!macro MacroOtherExtensions _action
  !insertmacro ${_action} ".asx"
  !insertmacro ${_action} ".b4s"
  !insertmacro ${_action} ".bin"
  !insertmacro ${_action} ".cue"
  !insertmacro ${_action} ".ifo"
  !insertmacro ${_action} ".m3u"
  !insertmacro ${_action} ".m3u8"
  !insertmacro ${_action} ".pls"
  !insertmacro ${_action} ".ram"
  !insertmacro ${_action} ".sdp"
  !insertmacro ${_action} ".vlc"
  !insertmacro ${_action} ".wvx"
  !insertmacro ${_action} ".xspf"
!macroend

!macro MacroSkinExtensions _action
  !insertmacro ${_action} ".vlt"
  !insertmacro ${_action} ".wsz"
!macroend

; One macro to rule them all
!macro MacroAllExtensions _action
  !insertmacro MacroAudioExtensions ${_action}
  !insertmacro MacroVideoExtensions ${_action}
  !insertmacro MacroOtherExtensions ${_action}
!macroend

; Generic function for adding the context menu for one ext.
!macro AddContextMenuExt EXT
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC "" $ContextMenuEntry_PlayWith
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC\command "" '"$INSTDIR\vlc.exe" --started-from-file --no-playlist-enqueue "%1"'

  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC "" $ContextMenuEntry_AddToPlaylist
  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC\command "" '"$INSTDIR\vlc.exe" --started-from-file --playlist-enqueue "%1"'
!macroend

!macro AddContextMenu EXT
  Push $R0
  ReadRegStr $R0 HKCR ${EXT} ""
  !insertmacro AddContextMenuExt $R0
  Pop $R0
!macroend

!macro DeleteContextMenuExt EXT
  DeleteRegKey HKCR ${EXT}\shell\PlayWithVLC
  DeleteRegKey HKCR ${EXT}\shell\AddToPlaylistVLC
!macroend

!macro DeleteContextMenu EXT
  Push $R0
  ReadRegStr $R0 HKCR ${EXT} ""
  !insertmacro DeleteContextMenuExt $R0
  Pop $R0
!macroend


