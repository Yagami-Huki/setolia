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
InstallDir "$PROGRAMDATA\obs-studio\plugins\${PLUGIN_NAME}"

!include "MUI2.nsh"

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

Section "Install"
  SetOutPath "$INSTDIR\bin\64bit"
  SetOverwrite ifnewer
  File ".\build_x64\${CONFIG}\${PLUGIN_NAME}.dll"

  SetOutPath "$INSTDIR\data\locale"
  SetOverwrite ifnewer
  File /r ".\data\locale\*"

  SetOutPath "$INSTDIR\data\templates"
  SetOverwrite ifnewer
  File /r ".\data\templates\*"

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall-${PLUGIN_NAME}.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\bin\64bit\${PLUGIN_NAME}.dll"
  RMDir /r "$INSTDIR\data"
  Delete "$INSTDIR\uninstall-${PLUGIN_NAME}.exe"
  RMDir "$INSTDIR\bin\64bit"
  RMDir "$INSTDIR\bin"
  RMDir "$INSTDIR"
SectionEnd
