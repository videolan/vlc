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
!define MUI_ICON "vlc48x48.ico"
!define MUI_UNICON "vlc48x48.ico"
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

  WriteRegStr HKCR Applications\vlc.exe "" ""
  WriteRegStr HKCR Applications\vlc.exe\shell "" "Play"
  WriteRegStr HKCR Applications\vlc.exe\shell\Play\command "" \
    '$INSTDIR\vlc.exe "%1"'

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
  CreateShortCut "$SMPROGRAMS\VideoLAN\Website.lnk" \
    "$INSTDIR\${PRODUCT_NAME}.url"
SectionEnd

Section /o "Mozilla plugin" SEC03
  SectionIn 2 3
  File /r mozilla

  WriteRegStr HKLM \
    SOFTWARE\MozillaPlugins\@videolan.org/vlc,version=${VERSION} \
    "Path" '"$INSTDIR\mozilla\npvlc.dll"'
SectionEnd

Section /o "ActiveX plugin" SEC04
  SectionIn 2 3
  SetOutPath "$INSTDIR"
  File activex\axvlc.dll
  RegDLL "$INSTDIR\axvlc.dll"
SectionEnd

SubSection "File type associations" SEC05
  ; Make sure we have the same list in uninstall
  !insertmacro RegisterExtensionSection ".a52"
  !insertmacro RegisterExtensionSection ".aac"
  !insertmacro RegisterExtensionSection ".ac3"
  !insertmacro RegisterExtensionSection ".asf"
  !insertmacro RegisterExtensionSection ".asx"
  !insertmacro RegisterExtensionSection ".avi"
  !insertmacro RegisterExtensionSection ".bin"
  !insertmacro RegisterExtensionSection ".cue"
  !insertmacro RegisterExtensionSection ".divx"
  !insertmacro RegisterExtensionSection ".dts"
  !insertmacro RegisterExtensionSection ".dv"
  !insertmacro RegisterExtensionSection ".flac"
  !insertmacro RegisterExtensionSection ".m1v"
  !insertmacro RegisterExtensionSection ".m2v"
  !insertmacro RegisterExtensionSection ".m3u"
  !insertmacro RegisterExtensionSection ".mka"
  !insertmacro RegisterExtensionSection ".mkv"
  !insertmacro RegisterExtensionSection ".mov"
  !insertmacro RegisterExtensionSection ".mp1"
  !insertmacro RegisterExtensionSection ".mp2"
  !insertmacro RegisterExtensionSection ".mp3"
  !insertmacro RegisterExtensionSection ".mp4"
  !insertmacro RegisterExtensionSection ".mpeg"
  !insertmacro RegisterExtensionSection ".mpeg1"
  !insertmacro RegisterExtensionSection ".mpeg2"
  !insertmacro RegisterExtensionSection ".mpeg4"
  !insertmacro RegisterExtensionSection ".mpg"
  !insertmacro RegisterExtensionSection ".ogg"
  !insertmacro RegisterExtensionSection ".ogm"
  !insertmacro RegisterExtensionSection ".pls"
  !insertmacro RegisterExtensionSection ".spx"
  !insertmacro RegisterExtensionSection ".vob"
  !insertmacro RegisterExtensionSection ".vlc"
  !insertmacro RegisterExtensionSection ".wav"
  !insertmacro RegisterExtensionSection ".wma"
  !insertmacro RegisterExtensionSection ".wmv"
SubSectionEnd

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
    "The VLC mozilla plugin"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC04} \
    "The VLC ActiveX plugin"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC05} \
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

  ; Make sure we have the same list in install
  !insertmacro UnRegisterExtensionSection ".a52"
  !insertmacro UnRegisterExtensionSection ".aac"
  !insertmacro UnRegisterExtensionSection ".ac3"
  !insertmacro UnRegisterExtensionSection ".asf"
  !insertmacro UnRegisterExtensionSection ".asx"
  !insertmacro UnRegisterExtensionSection ".avi"
  !insertmacro UnRegisterExtensionSection ".bin"
  !insertmacro UnRegisterExtensionSection ".cue"
  !insertmacro UnRegisterExtensionSection ".divx"
  !insertmacro UnRegisterExtensionSection ".dts"
  !insertmacro UnRegisterExtensionSection ".dv"
  !insertmacro UnRegisterExtensionSection ".flac"
  !insertmacro UnRegisterExtensionSection ".m1v"
  !insertmacro UnRegisterExtensionSection ".m2v"
  !insertmacro UnRegisterExtensionSection ".m3u"
  !insertmacro UnRegisterExtensionSection ".mka"
  !insertmacro UnRegisterExtensionSection ".mkv"
  !insertmacro UnRegisterExtensionSection ".mov"
  !insertmacro UnRegisterExtensionSection ".mp1"
  !insertmacro UnRegisterExtensionSection ".mp2"
  !insertmacro UnRegisterExtensionSection ".mp3"
  !insertmacro UnRegisterExtensionSection ".mp4"
  !insertmacro UnRegisterExtensionSection ".mpeg"
  !insertmacro UnRegisterExtensionSection ".mpeg1"
  !insertmacro UnRegisterExtensionSection ".mpeg2"
  !insertmacro UnRegisterExtensionSection ".mpeg4"
  !insertmacro UnRegisterExtensionSection ".mpg"
  !insertmacro UnRegisterExtensionSection ".ogg"
  !insertmacro UnRegisterExtensionSection ".ogm"
  !insertmacro UnRegisterExtensionSection ".pls"
  !insertmacro UnRegisterExtensionSection ".spx"
  !insertmacro UnRegisterExtensionSection ".vob"
  !insertmacro UnRegisterExtensionSection ".vlc"
  !insertmacro UnRegisterExtensionSection ".wav"
  !insertmacro UnRegisterExtensionSection ".wma"
  !insertmacro UnRegisterExtensionSection ".wmv"

  UnRegDLL "$INSTDIR\axvlc.dll"
  Delete /REBOOTOK "$INSTDIR\axvlc.dll"

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
