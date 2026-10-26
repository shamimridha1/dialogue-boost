; Dialogue Boost VST3 - Windows installer
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"

!define NAME "Dialogue Boost"
!define VERSION "1.0.0"
!define VST3DIR "$COMMONFILES64\VST3\Dialogue Boost.vst3"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\DialogueBoost"

Name "${NAME} ${VERSION}"
OutFile "..\..\dist\DialogueBoost-1.0.0-Windows-Setup.exe"
RequestExecutionLevel admin
InstallDir "${VST3DIR}"
BrandingText "${NAME} ${VERSION}"
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Install ${NAME}"
!define MUI_WELCOMEPAGE_TEXT "Louder, clearer dialogue without clipping.$\r$\n$\r$\nThis installs the ${NAME} VST3 plugin for DaVinci Resolve (and any VST3 host).$\r$\n$\r$\nClose DaVinci Resolve before continuing."
!define MUI_FINISHPAGE_TITLE "${NAME} is installed"
!define MUI_FINISHPAGE_TEXT "Restart DaVinci Resolve.$\r$\n$\r$\nTo use it: Fairlight page > Mixer > click Effects + on your dialogue track > Dynamics > VST > Dialogue Boost.$\r$\n$\r$\nPlugin location:$\r$\nC:\Program Files\Common Files\VST3"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "${NAME} needs 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "Install"
  SetOutPath "${VST3DIR}\Contents\x86_64-win"
  File "/oname=Dialogue Boost.vst3" "..\..\build\Dialogue Boost.vst3\Contents\x86_64-win\Dialogue Boost.vst3"
  SetOutPath "$PROGRAMFILES64\Dialogue Boost"
  WriteUninstaller "$PROGRAMFILES64\Dialogue Boost\Uninstall.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${NAME} (VST3)"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "Abid"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$PROGRAMFILES64\Dialogue Boost\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "Uninstall"
  RMDir /r "${VST3DIR}"
  Delete "$PROGRAMFILES64\Dialogue Boost\Uninstall.exe"
  RMDir "$PROGRAMFILES64\Dialogue Boost"
  DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
