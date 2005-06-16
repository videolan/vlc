;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; NSIS installer script for vlc ;
; (http://nsis.sourceforge.net) ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

!define PRODUCT_NAME "VLC media player"
!define PRODUCT_VERSION '${VERSION}'
!define PRODUCT_GROUP "VideoLAN"
!define PRODUCT_PUBLISHER "VideoLAN Team"
!define PRODUCT_WEB_SITE "http://www.videolan.org"
!define PRODUCT_DIR_REGKEY "Software\VideoLAN\VLC"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define PRODUCT_ID "{ea92ef52-afe4-4212-bacb-dfe9fca94cd6}"

;;;;;;;;;;;;;;;;;;;;;;;;;
; General configuration ;
;;;;;;;;;;;;;;;;;;;;;;;;;

Name "${PRODUCT_GROUP} ${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile ..\vlc-${VERSION}-win32.exe
InstallDir "$PROGRAMFILES\VideoLAN\VLC"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" "Install_Dir"
SetCompressor lzma
ShowInstDetails show
ShowUnInstDetails show
SetOverwrite ifnewer
CRCCheck on

InstType "Normal"
InstType "Full"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; NSIS Modern User Interface configuration ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "vlc48x48new.ico"
!define MUI_UNICON "vlc48x48new.ico"
!define MUI_COMPONENTSPAGE_SMALLDESC

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "COPYING.txt"
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\vlc.exe"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------


;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Push extensions on stack ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;
!macro MacroAudioExtensions _action
  !insertmacro ${_action} ".a52"
  !insertmacro ${_action} ".aac"
  !insertmacro ${_action} ".ac3"
  !insertmacro ${_action} ".dts"
  !insertmacro ${_action} ".flac"
  !insertmacro ${_action} ".mka"
  !insertmacro ${_action} ".mp1"
  !insertmacro ${_action} ".mp2"
  !insertmacro ${_action} ".mp3"
  !insertmacro ${_action} ".ogg"
  !insertmacro ${_action} ".spx"
  !insertmacro ${_action} ".wav"
  !insertmacro ${_action} ".wma"
!macroend

!macro MacroVideoExtensions _action
  !insertmacro ${_action} ".asf"
  !insertmacro ${_action} ".avi"
  !insertmacro ${_action} ".divx"
  !insertmacro ${_action} ".dv"
  !insertmacro ${_action} ".m1v"
  !insertmacro ${_action} ".m2v"
  !insertmacro ${_action} ".mkv"
  !insertmacro ${_action} ".mov"
  !insertmacro ${_action} ".mp4"
  !insertmacro ${_action} ".mpeg"
  !insertmacro ${_action} ".mpeg1"
  !insertmacro ${_action} ".mpeg2"
  !insertmacro ${_action} ".mpeg4"
  !insertmacro ${_action} ".mpg"
  !insertmacro ${_action} ".ps"
  !insertmacro ${_action} ".ts"
  !insertmacro ${_action} ".ogm"
  !insertmacro ${_action} ".vob"
  !insertmacro ${_action} ".wmv"
!macroend

!macro MacroOtherExtensions _action
  !insertmacro ${_action} ".asx"
  !insertmacro ${_action} ".bin"
  !insertmacro ${_action} ".cue"
  !insertmacro ${_action} ".m3u"
  !insertmacro ${_action} ".pls"
  !insertmacro ${_action} ".vlc"
!macroend

!macro MacroAllExtensions _action
  !insertmacro MacroAudioExtensions ${_action}
  !insertmacro MacroVideoExtensions ${_action}
  !insertmacro MacroOtherExtensions ${_action}
!macroend

;;;;;;;;;;;;;;;;;;;;;;;;;;
; File type associations ;
;;;;;;;;;;;;;;;;;;;;;;;;;;

Function RegisterExtension
  ; back up old value for extension $R0 (eg. ".opt")
  ReadRegStr $1 HKCR "$R0" ""
  StrCmp $1 "" NoBackup
    StrCmp $1 "VLC$R0" "NoBackup"
    WriteRegStr HKCR "$R0" "VLC.backup" $1
NoBackup:
  WriteRegStr HKCR "$R0" "" "VLC$R0"
  ReadRegStr $0 HKCR "VLC$R0" ""
  WriteRegStr HKCR "VLC$R0" "" "VLC media file"
  WriteRegStr HKCR "VLC$R0\shell" "" "Play"
  WriteRegStr HKCR "VLC$R0\shell\Play\command" "" '"$INSTDIR\vlc.exe" "%1"'
  WriteRegStr HKCR "VLC$R0\DefaultIcon" "" '"$INSTDIR\vlc.exe",0'
FunctionEnd

Function un.RegisterExtension
  ;start of restore script
  ReadRegStr $1 HKCR "$R0" ""
  StrCmp $1 "VLC$R0" 0 NoOwn ; only do this if we own it
    ReadRegStr $1 HKCR "$R0" "VLC.backup"
    StrCmp $1 "" 0 Restore ; if backup="" then delete the whole key
      DeleteRegKey HKCR "$R0"
    Goto NoOwn
Restore:
      WriteRegStr HKCR "$R0" "" $1
      DeleteRegValue HKCR "$R0" "VLC.backup"
NoOwn:
    DeleteRegKey HKCR "VLC$R0" ;Delete key with association settings
FunctionEnd

!macro RegisterExtensionSection EXT
  Section /o ${EXT}
    Push $R0
    StrCpy $R0 ${EXT}
    Call RegisterExtension
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

;;;;;;;;;;;;;;;;;;;;;;;;
; Context menu entries ;
;;;;;;;;;;;;;;;;;;;;;;;;

!macro AddContextMenu EXT
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC "" "Play with VLC media player"
  WriteRegStr HKCR ${EXT}\shell\PlayWithVLC\command "" '$INSTDIR\vlc.exe --no-playlist-enqueue "%1"'

  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC "" "Add to VLC media player's Playlist"
  WriteRegStr HKCR ${EXT}\shell\AddToPlaylistVLC\command "" '$INSTDIR\vlc.exe --one-instance --playlist-enqueue "%1"'
!macroend

!macro DeleteContextMenu EXT
  DeleteRegKey HKCR ${EXT}\shell\PlayWithVLC
  DeleteRegKey HKCR ${EXT}\shell\AddToPlaylistVLC
!macroend

;;;;;;;;;;;;;;;;;;;;;;
; Installer sections ;
;;;;;;;;;;;;;;;;;;;;;;

Section "Media player (required)" SEC01
  SectionIn 1 2 3 RO
  SetShellVarContext all
  SetOutPath "$INSTDIR"

  File  vlc.exe
  File  vlc.exe.manifest
  File  *.txt

  File  /r plugins
  File  /r locale
  File  /r skins
  File  /r http

  ; Add VLC to "recomended programs" for the following extensions
  WriteRegStr HKCR Applications\vlc.exe "" ""
  WriteRegStr HKCR Applications\vlc.exe "FriendlyAppName" "VLC media player"
  WriteRegStr HKCR Applications\vlc.exe\shell\Play "" "Play with VLC"
  WriteRegStr HKCR Applications\vlc.exe\shell\Play\command "" \
    '$INSTDIR\vlc.exe "%1"'
  !insertmacro MacroAllExtensions WriteRegStrSupportedTypes

  WriteRegStr HKCR "AudioCD\shell\PlayWithVLC" "" "Play with VLC media player"
  WriteRegStr HKCR "AudioCD\shell\PlayWithVLC\command" "" \
    "$INSTDIR\vlc.exe cdda:%1"
  WriteRegStr HKCR "DVD\shell\PlayWithVLC" "" "Play with VLC media player"
  WriteRegStr HKCR "DVD\shell\PlayWithVLC\command" "" \
    "$INSTDIR\vlc.exe dvd:%1"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\EventHandlers\PlayDVDMovieOnArrival" "VLCPlayDVDMovieOnArrival" ""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayDVDMovieOnArrival" "Action" "Play DVD movie"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayDVDMovieOnArrival" "DefaultIcon" '"$INSTDIR\vlc.exe",0'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayDVDMovieOnArrival" "InvokeProgID" "VLC.DVDMovie"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayDVDMovieOnArrival" "InvokeVerb" "play"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayDVDMovieOnArrival" "Provider" "VideoLAN VLC media player"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\EventHandlers\PlayCDAudioOnArrival" "VLCPlayCDAudioOnArrival" ""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayCDAudioOnArrival" "Action" "Play CD audio"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayCDAudioOnArrival" "DefaultIcon" '"$INSTDIR\vlc.exe",0'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayCDAudioOnArrival" "InvokeProgID" "VLC.CDAudio"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayCDAudioOnArrival" "InvokeVerb" "play"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayCDAudioOnArrival" "Provider" "VideoLAN VLC media player"
  WriteRegStr HKCR "VLC.DVDMovie" "" "VLC DVD Movie"
  WriteRegStr HKCR "VLC.DVDMovie\shell" "" "Play"
  WriteRegStr HKCR "VLC.DVDMovie\shell\Play\command" "" \
    '$INSTDIR\vlc.exe dvd:%1@1:0'
  WriteRegStr HKCR "VLC.DVDMovie\DefaultIcon" "" '"$INSTDIR\vlc.exe",0'
  WriteRegStr HKCR "VLC.CDAudio" "" "VLC CD Audio"
  WriteRegStr HKCR "VLC.CDAudio\shell" "" "Play"
  WriteRegStr HKCR "VLC.CDAudio\shell\Play\command" "" \
    '$INSTDIR\vlc.exe cdda:%1'
  WriteRegStr HKCR "VLC.CDAudio\DefaultIcon" "" '"$INSTDIR\vlc.exe",0'

SectionEnd

Section "Start Menu + Desktop Shortcut" SEC02
  SectionIn 1 2 3
  CreateDirectory "$SMPROGRAMS\VideoLAN"
  CreateShortCut "$SMPROGRAMS\VideoLAN\VLC media player.lnk" \
    "$INSTDIR\vlc.exe" "--intf wxwin --wxwin-embed"
  CreateShortCut "$SMPROGRAMS\VideoLAN\VLC media player (alt).lnk" \
    "$INSTDIR\vlc.exe" "--intf wxwin --no-wxwin-embed"
  CreateShortCut "$SMPROGRAMS\VideoLAN\VLC media player (skins).lnk" \
    "$INSTDIR\vlc.exe" "--intf skins"
  CreateShortCut "$SMPROGRAMS\VideoLAN\Reset VLC defaults and quit.lnk" \
    "$INSTDIR\vlc.exe" "--reset-config --reset-plugins-cache --save-config vlc:quit "
  CreateShortCut "$DESKTOP\VLC media player.lnk" \
    "$INSTDIR\vlc.exe" "--intf wxwin"
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" \
    "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\VideoLAN\${PRODUCT_NAME} Website.lnk" \
    "$INSTDIR\${PRODUCT_NAME}.url"
  WriteIniStr "$INSTDIR\Documentation.url" "InternetShortcut" "URL" \
    "${PRODUCT_WEB_SITE}/doc/"
  CreateShortCut "$SMPROGRAMS\VideoLAN\Documentation.lnk" \
    "$INSTDIR\Documentation.url"
SectionEnd

Section /o "Mozilla plugin" SEC03
  SectionIn 2 3
  File /r mozilla

  ; doesn't work. bug in mozilla/mozilla firefox or moz documentation (xpt file isn't loaded)
  ; see mozilla bugs 184506 and 159445
  ;!define Moz "SOFTWARE\MozillaPlugins\@videolan.org/vlc,version=${VERSION}"
  ;WriteRegStr HKLM ${Moz} "Description" "VideoLAN VLC plugin for Mozilla"
  ;WriteRegStr HKLM ${Moz} "Path" "$INSTDIR\mozilla\npvlc.dll"
  ;WriteRegStr HKLM ${Moz} "Product" "VLC media player"
  ;WriteRegStr HKLM ${Moz} "Vendor" "VideoLAN"
  ;WriteRegStr HKLM ${Moz} "Version" "${VERSION}"
  ;WriteRegStr HKLM ${Moz} "XPTPath" "$INSTDIR\mozilla\vlcintf.xpt"

  Push $R0
  Push $R1
  Push $R2

  !define Index 'Line${__LINE__}'
  StrCpy $R1 "0"

  "${Index}-Loop:"

    ; Check for Key
    EnumRegKey $R0 HKLM "SOFTWARE\Mozilla" "$R1"
    StrCmp $R0 "" "${Index}-End"
    IntOp $R1 $R1 + 1
    ReadRegStr $R2 HKLM "SOFTWARE\Mozilla\$R0\Extensions" "Plugins"
    StrCmp $R2 "" "${Index}-Loop" ""

    CopyFiles "$INSTDIR\mozilla\*" "$R2"
    Goto "${Index}-Loop"

  "${Index}-End:"
  !undef Index

SectionEnd

Section /o "ActiveX plugin" SEC04
  SectionIn 2 3
  SetOutPath "$INSTDIR"
  File activex\axvlc.dll
  RegDLL "$INSTDIR\axvlc.dll"
SectionEnd

Section "Context Menus" SEC05
  SectionIn 1 2 3
  !insertmacro MacroAllExtensions AddContextMenu
SectionEnd

SectionGroup "File type associations" SEC06
  SectionGroup "Audio Files"
    !insertmacro MacroAudioExtensions RegisterExtensionSection
  SectionGroupEnd
  SectionGroup "Video Files"
    !insertmacro MacroVideoExtensions RegisterExtensionSection
  SectionGroupEnd
  SectionGroup "Other"
    !insertmacro MacroOtherExtensions RegisterExtensionSection
  SectionGroupEnd
SectionGroupEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "InstallDir" $INSTDIR
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "Version" "${VERSION}"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\vlc.exe"

  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DisplayIcon" "$INSTDIR\vlc.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} \
    "The media player itself"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02} \
    "Adds icons to your start menu and your desktop for easy access"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC03} \
    "The VLC Mozilla and Mozilla Firefox plugin"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC04} \
    "The VLC ActiveX plugin"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC05} \
    "Add context menu items (Play With VLC and Add To VLC's Playlist)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC06} \
    "Sets VLC media player as the default application for the specified file type"
!insertmacro MUI_FUNCTION_DESCRIPTION_END


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK \
    "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 \
    "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  SetShellVarContext all

  !insertmacro MacroAllExtensions DeleteContextMenu
  !insertmacro MacroAllExtensions UnRegisterExtensionSection

  UnRegDLL "$INSTDIR\axvlc.dll"
  Delete /REBOOTOK "$INSTDIR\axvlc.dll"

  ;remove mozilla plugin
  Push $R0
  Push $R1
  Push $R2

  !define Index 'Line${__LINE__}'
  StrCpy $R1 "0"

  "${Index}-Loop:"

    ; Check for Key
    EnumRegKey $R0 HKLM "SOFTWARE\Mozilla" "$R1"
    StrCmp $R0 "" "${Index}-End"
    IntOp $R1 $R1 + 1
    ReadRegStr $R2 HKLM "SOFTWARE\Mozilla\$R0\Extensions" "Plugins"
    StrCmp $R2 "" "${Index}-Loop" ""

    Delete "$R2\vlcintf.xpt"
    Delete "$R2\npvlc.dll"
    Goto "${Index}-Loop"

  "${Index}-End:"
  !undef Index

  RMDir "$SMPROGRAMS\VideoLAN"
  RMDir /r $SMPROGRAMS\VideoLAN
  RMDir /r $INSTDIR
  DeleteRegKey HKLM Software\VideoLAN

  DeleteRegKey HKCR Applications\vlc.exe
  DeleteRegKey HKCR AudioCD\shell\PlayWithVLC
  DeleteRegKey HKCR DVD\shell\PlayWithVLC
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\EventHandlers\PlayDVDMovieOnArrival" "VLCPlayDVDMovieOnArrival"
  DeleteRegKey HKLM Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayDVDMovieOnArrival
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\EventHandlers\PlayCDAudioOnArrival" "VLCPlayCDAudioOnArrival"
  DeleteRegKey HKLM Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\VLCPlayCDAudioOnArrival
  DeleteRegKey HKCR "VLC.MediaFile"

  DeleteRegKey HKLM \
    SOFTWARE\MozillaPlugins\@videolan.org/vlc,version=${VERSION}

  DeleteRegKey HKLM \
    Software\Microsoft\Windows\CurrentVersion\Uninstall\VideoLAN

  Delete "$DESKTOP\VLC media player.lnk"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
