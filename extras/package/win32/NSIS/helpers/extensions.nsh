!include "StrFunc.nsh"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 1. File type associations ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; "Initialize" string functions
${StrRep}
${StrCase}

;; Function that associates one extension with VLC
Function AssociateExtension
  ; back up old value for extension $R0 (eg. ".opt")
  ReadRegStr $1 HKCR "$R0" ""
  StrCmp $1 "" NoBackup
    StrCmp $1 "VLC$R0" "NoBackup"
    WriteRegStr HKCR "$R0" "VLC.backup" $1
NoBackup:
  WriteRegStr HKCR "$R0" "" "VLC$R0"
FunctionEnd

;; Function that registers one extension for VLC
Function RegisterExtension
  ; R0 contains the extension, R1 contains the type (Audio/Video)
  ; Remove the leading dot from the filetype string
  ${StrRep} $R2 $R0 "." ""
  ; And capitalize the extension
  ${StrCase} $R2 $R2 "U"
  ; for instance: MKV Video File (VLC)
  WriteRegStr HKCR "VLC$R0" "" "$R2 $R1 File (VLC)"
  WriteRegStr HKCR "VLC$R0\shell" "" "Open"
  WriteRegStr HKCR "VLC$R0\shell\Open" "" "$(ShellAssociation_Play)"
  WriteRegStr HKCR "VLC$R0\shell\Open" "MultiSelectModel" "Player"
  WriteRegStr HKCR "VLC$R0\shell\Open\command" "" '"$INSTDIR\vlc.exe" --started-from-file "%1"'
  WriteRegStr HKCR "VLC$R0\DefaultIcon" "" '"$INSTDIR\vlc.exe",0'
  WriteRegStr HKCR "Applications\vlc.exe\SupportedTypes" $0 ""

  ${If} ${AtLeastWinVista}
    WriteRegStr HKLM "Software\Clients\Media\VLC\Capabilities\FileAssociations" "$R0" "VLC$R0"
  ${EndIf}
FunctionEnd

;; Function that registers one skin extension for VLC
Function RegisterSkinExtension
  WriteRegStr HKCR "VLC$R0" "" "VLC skin file ($R0)"
  WriteRegStr HKCR "VLC$R0\shell" "" "Open"
  WriteRegStr HKCR "VLC$R0\shell\Open" "" ""
  WriteRegStr HKCR "VLC$R0\shell\Open\command" "" '"$INSTDIR\vlc.exe" -Iskins --skins2-last "%1"'
  WriteRegStr HKCR "VLC$R0\DefaultIcon" "" '"$INSTDIR\vlc.exe",0'

  ${If} ${AtLeastWinVista}
    WriteRegStr HKLM "Software\Clients\Media\VLC\Capabilities\FileAssociations" "$R0" "VLC$R0"
  ${EndIf}
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

!macro AssociateExtensionSection TYPE EXT
  ${MementoSection} ${EXT} SEC_EXT_${TYPE}_${EXT}
    SectionIn 1 3
    Push $R0
    StrCpy $R0 ${EXT}
    Call AssociateExtension
    Pop $R0
  ${MementoSectionEnd}
!macroend

!macro AssociateSkinExtensionSection TYPE EXT
  ${MementoUnselectedSection} ${EXT} SEC_EXT_SKIN_${EXT}
    SectionIn 1 3
    Push $R0
    StrCpy $R0 ${EXT}
    Call AssociateExtension
    Pop $R0
  ${MementoSectionEnd}
!macroend

!macro AssociateExtensionUnselectedSection TYPE EXT
  ${MementoUnselectedSection} ${EXT} SEC_EXT_${TYPE}_${EXT}
    SectionIn 1 3
    Push $R0
    StrCpy $R0 ${EXT}
    Call AssociateExtension
    Pop $R0
  ${MementoSectionEnd}
!macroend

!macro RegisterExtensionMacro TYPE EXT
  Push $R0
  StrCpy $R0 ${EXT}
  Push $R1
  StrCpy $R1 ${TYPE}
  Call RegisterExtension
  Pop $R1
  Pop $R0
!macroend

!macro RegisterSkinExtensionMacro TYPE EXT
  Push $R0
  StrCpy $R0 ${EXT}
  Call RegisterSkinExtension
  Pop $R0
!macroend

!macro UnRegisterExtensionSection TYPE EXT
  Push $R0
  StrCpy $R0 ${EXT}
  Call un.RegisterExtension
  Pop $R0
!macroend

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Extension lists  Macros                    ;
; Those macros calls the previous functions  ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

!macro MacroAudioExtensions _action
  !insertmacro ${_action} Audio ".3ga"
  !insertmacro ${_action} Audio ".669"
  !insertmacro ${_action} Audio ".a52"
  !insertmacro ${_action} Audio ".aac"
  !insertmacro ${_action} Audio ".ac3"
  !insertmacro ${_action} Audio ".adt"
  !insertmacro ${_action} Audio ".adts"
  !insertmacro ${_action} Audio ".aif"
  !insertmacro ${_action} Audio ".aifc"
  !insertmacro ${_action} Audio ".aiff"
  !insertmacro ${_action} Audio ".au"
  !insertmacro ${_action} Audio ".amr"
  !insertmacro ${_action} Audio ".aob"
  !insertmacro ${_action} Audio ".ape"
  !insertmacro ${_action} Audio ".caf"
  !insertmacro ${_action} Audio ".cda"
  !insertmacro ${_action} Audio ".dts"
  !insertmacro ${_action} Audio ".dsf"
  !insertmacro ${_action} Audio ".dff"
  !insertmacro ${_action} Audio ".flac"
  !insertmacro ${_action} Audio ".it"
  !insertmacro ${_action} Audio ".m4a"
  !insertmacro ${_action} Audio ".m4p"
  !insertmacro ${_action} Audio ".mid"
  !insertmacro ${_action} Audio ".mka"
  !insertmacro ${_action} Audio ".mlp"
  !insertmacro ${_action} Audio ".mod"
  !insertmacro ${_action} Audio ".mp1"
  !insertmacro ${_action} Audio ".mp2"
  !insertmacro ${_action} Audio ".mp3"
  !insertmacro ${_action} Audio ".mpc"
  !insertmacro ${_action} Audio ".mpga"
  !insertmacro ${_action} Audio ".oga"
  !insertmacro ${_action} Audio ".oma"
  !insertmacro ${_action} Audio ".opus"
  !insertmacro ${_action} Audio ".qcp"
  !insertmacro ${_action} Audio ".ra"
  !insertmacro ${_action} Audio ".rmi"
  !insertmacro ${_action} Audio ".snd"
  !insertmacro ${_action} Audio ".s3m"
  !insertmacro ${_action} Audio ".spx"
  !insertmacro ${_action} Audio ".tta"
  !insertmacro ${_action} Audio ".voc"
  !insertmacro ${_action} Audio ".vqf"
  !insertmacro ${_action} Audio ".w64"
  !insertmacro ${_action} Audio ".wav"
  !insertmacro ${_action} Audio ".wma"
  !insertmacro ${_action} Audio ".wv"
  !insertmacro ${_action} Audio ".xa"
  !insertmacro ${_action} Audio ".xm"
!macroend

!macro MacroVideoExtensions _action
  !insertmacro ${_action} Video ".3g2"
  !insertmacro ${_action} Video ".3gp"
  !insertmacro ${_action} Video ".3gp2"
  !insertmacro ${_action} Video ".3gpp"
  !insertmacro ${_action} Video ".amv"
  !insertmacro ${_action} Video ".asf"
  !insertmacro ${_action} Video ".avi"
  !insertmacro ${_action} Video ".bik"
  !insertmacro ${_action} Video ".dav"
  !insertmacro ${_action} Video ".divx"
  !insertmacro ${_action} Video ".drc"
  !insertmacro ${_action} Video ".dv"
  !insertmacro ${_action} Video ".dvr-ms"
  !insertmacro ${_action} Video ".evo"
  !insertmacro ${_action} Video ".f4v"
  !insertmacro ${_action} Video ".flv"
  !insertmacro ${_action} Video ".gvi"
  !insertmacro ${_action} Video ".gxf"
  !insertmacro ${_action} Video ".m1v"
  !insertmacro ${_action} Video ".m2t"
  !insertmacro ${_action} Video ".m2v"
  !insertmacro ${_action} Video ".m2ts"
  !insertmacro ${_action} Video ".m4v"
  !insertmacro ${_action} Video ".mkv"
  !insertmacro ${_action} Video ".mov"
  !insertmacro ${_action} Video ".mp2v"
  !insertmacro ${_action} Video ".mp4"
  !insertmacro ${_action} Video ".mp4v"
  !insertmacro ${_action} Video ".mpa"
  !insertmacro ${_action} Video ".mpe"
  !insertmacro ${_action} Video ".mpeg"
  !insertmacro ${_action} Video ".mpeg1"
  !insertmacro ${_action} Video ".mpeg2"
  !insertmacro ${_action} Video ".mpeg4"
  !insertmacro ${_action} Video ".mpg"
  !insertmacro ${_action} Video ".mpv2"
  !insertmacro ${_action} Video ".mts"
  !insertmacro ${_action} Video ".mtv"
  !insertmacro ${_action} Video ".mxf"
  !insertmacro ${_action} Video ".nsv"
  !insertmacro ${_action} Video ".nuv"
  !insertmacro ${_action} Video ".ogg"
  !insertmacro ${_action} Video ".ogm"
  !insertmacro ${_action} Video ".ogx"
  !insertmacro ${_action} Video ".ogv"
  !insertmacro ${_action} Video ".rec"
  !insertmacro ${_action} Video ".rm"
  !insertmacro ${_action} Video ".rmvb"
  !insertmacro ${_action} Video ".rpl"
  !insertmacro ${_action} Video ".thp"
  !insertmacro ${_action} Video ".tod"
  !insertmacro ${_action} Video ".tp"
  !insertmacro ${_action} Video ".ts"
  !insertmacro ${_action} Video ".tts"
  !insertmacro ${_action} Video ".vob"
  !insertmacro ${_action} Video ".vro"
  !insertmacro ${_action} Video ".webm"
  !insertmacro ${_action} Video ".wmv"
  !insertmacro ${_action} Video ".wtv"
  !insertmacro ${_action} Video ".xesc"
!macroend

!macro MacroOtherExtensions _action
  !insertmacro ${_action} Other ".asx"
  !insertmacro ${_action} Other ".b4s"
  !insertmacro ${_action} Other ".cue"
  !insertmacro ${_action} Other ".ifo"
  !insertmacro ${_action} Other ".m3u"
  !insertmacro ${_action} Other ".m3u8"
  !insertmacro ${_action} Other ".pls"
  !insertmacro ${_action} Other ".ram"
  !insertmacro ${_action} Other ".sdp"
  !insertmacro ${_action} Other ".vlc"
  !insertmacro ${_action} Other ".wvx"
  !insertmacro ${_action} Other ".xspf"
  !insertmacro ${_action} Other ".wpl"
  !insertmacro ${_action} Other ".zpl"
!macroend

!macro MacroUnassociatedExtensions _action
  !insertmacro ${_action} Other ".iso"
  !insertmacro ${_action} Other ".zip"
  !insertmacro ${_action} Other ".rar"
!macroend

!macro MacroSkinExtensions _action
  !insertmacro ${_action} Skin ".vlt"
  !insertmacro ${_action} Skin ".wsz"
!macroend

; One macro to rule them all
!macro MacroAllExtensions _action
  !insertmacro MacroAudioExtensions ${_action}
  !insertmacro MacroVideoExtensions ${_action}
  !insertmacro MacroOtherExtensions ${_action}
  !insertmacro MacroUnassociatedExtensions ${_action}
!macroend

; Generic function for adding the context menu for one ext.
!macro AddContextMenuExt EXT
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC "" "$(ContextMenuEntry_PlayWith)"
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC "Icon" '"$INSTDIR\vlc.exe",0'
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC "MultiSelectModel" "Player"
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC\command "" '"$INSTDIR\vlc.exe" --started-from-file --no-playlist-enqueue "%1"'

  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC "" "$(ContextMenuEntry_AddToPlaylist)"
  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC "Icon" '"$INSTDIR\vlc.exe",0'
  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC "MultiSelectModel" "Player"
  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC\command "" '"$INSTDIR\vlc.exe" --started-from-file --playlist-enqueue "%1"'
!macroend

!macro AddContextMenu TYPE EXT
  !insertmacro AddContextMenuExt VLC${EXT}
!macroend

!macro DeleteContextMenuExt EXT
  DeleteRegKey HKCR ${EXT}\shell\PlayWithVLC
  DeleteRegKey HKCR ${EXT}\shell\AddToPlaylistVLC
!macroend

!macro DeleteContextMenu TYPE EXT
  !insertmacro DeleteContextMenuExt VLC${EXT}
!macroend


