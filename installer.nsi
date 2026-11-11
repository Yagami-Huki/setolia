!ifndef PLUGIN_NAME
  !define PLUGIN_NAME "setolia"
!endif
!ifndef PLUGIN_VERSION
  !define PLUGIN_VERSION "1.3.0"
!endif
!ifndef CONFIG
  !define CONFIG "RelWithDebInfo"
!endif

Unicode True
RequestExecutionLevel admin
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Japanese.nlf"

Name "${PLUGIN_NAME} ${PLUGIN_VERSION}"
OutFile "${PLUGIN_NAME}-${PLUGIN_VERSION}-windows-x64-Installer.exe"
InstallDir "$PROGRAMFILES\obs-studio\plugins\${PLUGIN_NAME}"

!include "MUI2.nsh"
!include "LogicLib.nsh"

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

Var ObsDir

Function .onInit
  SetShellVarContext all
  StrCpy $INSTDIR "$APPDATA\obs-studio\plugins\${PLUGIN_NAME}"
FunctionEnd

; 旧形式のインストール先 (<OBS>\obs-plugins\64bit) に古い版が残っていると、
; OBS 32 以前で重複して読み込まれる可能性があるため、削除するかを確認する。
Function RemoveLegacyInstall
  SetRegView 64
  ReadRegStr $ObsDir HKLM "Software\OBS Studio" ""
  SetRegView lastused
  ${If} $ObsDir == ""
    StrCpy $ObsDir "$PROGRAMFILES64\obs-studio"
  ${EndIf}

  ${IfNot} ${FileExists} "$ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.dll"
    Return
  ${EndIf}

  MessageBox MB_YESNO|MB_ICONQUESTION \
    "古い形式でインストールされた ${PLUGIN_NAME} が見つかりました。$\r$\n$\r$\n\
$ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.dll$\r$\n$\r$\n\
残したままにすると、OBS 32 以前でプラグインが重複して読み込まれる可能性があります。削除しますか?" \
    /SD IDNO IDYES remove
  DetailPrint "旧形式のファイルを残しました: $ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.dll"
  Return

  remove:
  Delete "$ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.dll"
  Delete "$ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.pdb"
  RMDir /r "$ObsDir\data\obs-plugins\${PLUGIN_NAME}"
  ${If} ${FileExists} "$ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.dll"
    MessageBox MB_OK|MB_ICONEXCLAMATION \
      "$ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.dll を削除できませんでした。OBS を終了してから手動で削除してください。" /SD IDOK
  ${Else}
    DetailPrint "旧形式のファイルを削除しました: $ObsDir\obs-plugins\64bit\${PLUGIN_NAME}.dll"
  ${EndIf}
FunctionEnd

Section "Install"
  Call RemoveLegacyInstall

  ; OBS 33 以降はフォルダ直下、OBS 32 以前は bin\64bit を読むため、同じ dll を両方に置く。
  ; 2 つが必ず同じ内容になるよう、dll は常に上書きする。
  SetOverwrite on
  SetOutPath "$INSTDIR"
  File ".\build_x64\${CONFIG}\${PLUGIN_NAME}.dll"

  SetOutPath "$INSTDIR\bin\64bit"
  File ".\build_x64\${CONFIG}\${PLUGIN_NAME}.dll"

  SetOutPath "$INSTDIR\data\locale"
  SetOverwrite ifnewer
  File /r ".\data\locale\*"

  SetOutPath "$INSTDIR\data\templates"
  SetOverwrite ifnewer
  File /r ".\data\templates\*"

  ; OBS 32 以降のプラグインマネージャーが読むマニフェスト (CMake がビルドディレクトリに生成する)
  SetOutPath "$INSTDIR\data"
  SetOverwrite on
  File ".\build_x64\manifest.json"

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall-${PLUGIN_NAME}.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\${PLUGIN_NAME}.dll"
  Delete "$INSTDIR\${PLUGIN_NAME}.pdb"
  Delete "$INSTDIR\bin\64bit\${PLUGIN_NAME}.dll"
  Delete "$INSTDIR\bin\64bit\${PLUGIN_NAME}.pdb"
  RMDir /r "$INSTDIR\data"
  Delete "$INSTDIR\uninstall-${PLUGIN_NAME}.exe"
  RMDir "$INSTDIR\bin\64bit"
  RMDir "$INSTDIR\bin"
  RMDir "$INSTDIR"
SectionEnd
