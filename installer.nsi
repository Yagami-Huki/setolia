!ifndef PLUGIN_NAME
  !define PLUGIN_NAME "setolia"
!endif
!ifndef PLUGIN_VERSION
  !define PLUGIN_VERSION "1.1.0"
!endif
!ifndef CONFIG
  !define CONFIG "RelWithDebInfo"
!endif

Unicode True
RequestExecutionLevel admin
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Japanese.nlf"

Name "${PLUGIN_NAME} ${PLUGIN_VERSION}"
OutFile "${PLUGIN_NAME}-${PLUGIN_VERSION}-windows-x64-Installer.exe"
InstallDir "$PROGRAMFILES\obs-studio"

!include "MUI2.nsh"

Function .onInit
  SetRegView 64

  ReadRegStr $R0 HKLM "SOFTWARE\OBS Studio" ""

  IfErrors obs_not_found
    StrCpy $INSTDIR "$R0"
    Goto obs_found_end
  obs_not_found:
    ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1905180" "InstallLocation"

    IfErrors steam_obs_not_found
      StrCpy $INSTDIR "$R0"
      Goto steam_obs_found_end

    steam_obs_not_found:
    steam_obs_found_end:
  obs_found_end:
FunctionEnd

Function un.onInit
  GetFullPathName $INSTDIR "$INSTDIR\.."
FunctionEnd

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

Section "Install"
  SetOutPath "$INSTDIR\obs-plugins\64bit"
  SetOverwrite ifnewer
  File ".\build_x64\${CONFIG}\${PLUGIN_NAME}.dll"

  SetOutPath "$INSTDIR\data\obs-plugins\${PLUGIN_NAME}\locale"
  SetOverwrite ifnewer
  File /r ".\data\locale\*"

  SetOutPath "$INSTDIR\data\obs-plugins\${PLUGIN_NAME}\templates"
  SetOverwrite ifnewer
  File /r ".\data\templates\*"

  SetOutPath "$INSTDIR\obs-plugins"
  WriteUninstaller "$INSTDIR\obs-plugins\uninstall-${PLUGIN_NAME}.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\obs-plugins\64bit\${PLUGIN_NAME}.dll"
  RMDir /r "$INSTDIR\data\obs-plugins\${PLUGIN_NAME}"
  Delete "$INSTDIR\obs-plugins\uninstall-${PLUGIN_NAME}.exe"
SectionEnd
