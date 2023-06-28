;
; to make this work outside of Miller's machines (and on Windows) create a 'C:\tmp' dir and
; remove all occurrences of '/tmp/' except on :
;
;   "OutFile "/tmp/pd-${PRODUCT_VERSION}.windows-installer.exe"
;

; https://stackoverflow.com/questions/36185539/can-i-get-nsis-to-make-a-single-installer-that-handles-local-deployment-and-syst
; ./build-nsi.sh G:/gitportable/nsis/pd-0.53.1 0.53.1


;####################################################
;
; NOTE: registry settings auto change for the 32bit Pd:
;
;   SHCTX "Software\${PRODUCT_NAME}"
; turns
;   SHCTX "Software\WOW6432Node\${PRODUCT_NAME}"
;
; all the "Uninstall" keys:
;   SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
;    ...
; turns
;   SHCTX "Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
;    ...
;
; all this is healthier and in concordance with the OS.
; i.e read and write to the registry according to Pd<arch> and not to nsis<arch>
; i.e when a Pd-64bit installer is made with a 32bit nsis avoid using the 32bit registry.
;####################################################


Unicode True
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"

; App name
!if ${PDEXE} == "pd.exe"
    !if ${ARCHI} == "64"
        !define PRODUCT_NAME "Pure Data"
    !else
        !define PRODUCT_NAME "Pure Data (${ARCHI}-bit)"
    !endif
    !define PD_FOLDER "Pd"
!else
    !if ${ARCHI} == "64"
        !define PRODUCT_NAME "Pd64"
    !else
        !define PRODUCT_NAME "Pd64 (${ARCHI}-bit)"
    !endif
    !define PD_FOLDER "Pd64"
!endif
!define PRODUCT_VERSION "${PDVER}"
!define WISH_NAME "${WISHN}"
!define PRODUCT_PUBLISHER "Miller Puckette"
!define PRODUCT_WEB_SITE "http://www.puredata.info"

; MUI Settings / Icons
!define MUI_ICON "pd.ico"
!define MUI_UNICON "pd.ico"
; MUI Settings / Wizard
!define MUI_WELCOMEFINISHPAGE_BITMAP "big.bmp"
; MUI Settings / Header
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "small.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "small.bmp"

Var INSTDIR_BASE
Var CONTEXT
Var PreUninstallerDisplayName
Var PreUninstallerPath
Var PreUninstallerUninstallString
var ShowPreviousInstallationDetected

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "/tmp/${PD_FOLDER}-${PRODUCT_VERSION}.windows-installer.exe"
InstallDir ""

; Take the highest execution level available
; This means that if it's possible to, we become an administrator
RequestExecutionLevel highest

!macro ONINIT un
    Function ${un}.onInit
        ; avoid silent install for later use with "winget" (Windows package manager) (tested).
        SetSilent normal
        ; The value of SetShellVarContext detetmines whether SHCTX is HKLM or HKCU
        ; and whether SMPROGRAMS refers to all users or just the current user
        UserInfo::GetAccountType
        Pop $0
        ${If} $0 == "Admin"
            ; If we're an admin, default to installing to C:\Program Files
            SetShellVarContext all
            StrCpy $CONTEXT all
            StrCpy $INSTDIR_BASE "$PROGRAMFILES${ARCHI}"
        ${Else}
            ; If we're just a user, default to installing to ~\AppData\Local
            SetShellVarContext current
            StrCpy $CONTEXT current
            StrCpy $INSTDIR_BASE "$LOCALAPPDATA"
        ${EndIf}
        ; the installer .exe might be a 32bit app and Windows is treating it
        ; "as is" but we might need to access the 64bit part of the registry.
        ${If} ${ARCHI} == "64"
            SetRegView 64
        ${Else}
            ; read "NOTE: registry settings" above
            SetRegView 32
        ${Endif}
        ; This only happens in the installer, because the uninstaller already knows INSTDIR
        ${If} $INSTDIR == ""
            ReadRegStr $PreUninstallerPath SHCTX "Software\${PRODUCT_NAME}" "${ARCHI}"
            ReadRegStr $PreUninstallerDisplayName SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName"
            ReadRegStr $PreUninstallerUninstallString SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString"
            ${If} $PreUninstallerPath != ""
                ; If we're already installed show the optional uninstall page:
                StrCpy $ShowPreviousInstallationDetected "yes"
            ${Endif}

            ${If} ${ARCHI} != "64"
                ${AndIf} $INSTDIR_BASE == "$LOCALAPPDATA"
                    ; use special folder if we are 32bit and on $LOCALAPPDATA
                    StrCpy $INSTDIR "$INSTDIR_BASE\${PD_FOLDER}${ARCHI}"
            ${Else}
                StrCpy $INSTDIR "$INSTDIR_BASE\${PD_FOLDER}"
            ${Endif}
        ${Endif}
    FunctionEnd
!macroend

; Define the function twice, once for the installer and again for the uninstaller
!insertmacro ONINIT ""
!insertmacro ONINIT "un"

!define MUI_ABORTWARNING

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!include "/tmp/license_page.nsh"
Page Custom PreviousInstallationDetected PreviousInstallationDetectedLeave
!define MUI_COMPONENTSPAGE_NODESC
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Finish page
Var SHORTPATH
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\${WISH_NAME}"
!define MUI_FINISHPAGE_RUN_PARAMETERS " $SHORTPATH\tcl\pd-gui.tcl"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\doc\1.manual\index.htm"
!insertmacro MUI_PAGE_FINISH

;!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; Function to refresh shell icons
!define SHCNE_ASSOCCHANGED 0x08000000
!define SHCNF_IDLIST 0

Function RefreshShellIcons
    ; By jerome tremblay - april 2003
    System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v \
    (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'
FunctionEnd

Function un.RefreshShellIcons
    ; By jerome tremblay - april 2003
    System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v \
    (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'
FunctionEnd

; start Pd-GUI from the installer (this is to avoid whitespace in path)
; ; make variable $SHORTPATH
Function PdGuiFromInstaller
    GetFullPathName /SHORT $SHORTPATH $INSTDIR
FunctionEnd

Function PreviousInstallationDetected
${If} $ShowPreviousInstallationDetected == "yes"
    nsDialogs::Create 1018
    Pop $0
    !insertmacro MUI_HEADER_TEXT \
    "Previous Installation" "Select if you are going to uninstall previous installation"
    ${NSD_CreateLabel} 0 0 100% 40% "System tells there is already a Pd installation:$\r$\n$\r$\n\
        '$PreUninstallerDisplayName' on '$PreUninstallerPath' $\r$\n$\r$\n\
        If you are going to upgrade or downgrade it is recommended that you run \
        the uninstaller. If not, proceed the installation to a different location"
    ${NSD_CreateRadioButton} 0 50% 50% 10% "Run the uninstaller (recommended)"
    Pop $1
    SendMessage $1 ${BM_CLICK} "" "" ; Must select a default
    ${NSD_CreateRadioButton} 0 60% 50% 10% "continue"
    Pop $2
    nsDialogs::Show
${EndIf}
FunctionEnd

Function PreviousInstallationDetectedLeave
${NSD_GetState} $1 $3
${If} $3 == ${BST_CHECKED}
    ; run the uninstaller
    ; https://stackoverflow.com/questions/4676898/how-to-execute-an-nsis-uninstaller-from-within-an-another-nsis-installer-and-wai
    ExecWait '"$PreUninstallerUninstallString" /S _?=$PreUninstallerPath'
    IfErrors uninstaller_canceled
    ; if all went good this file is gone
    IfFileExists \
    "$PreUninstallerPath\bin\${PDEXE}" uninstaller_canceled uninstaller_done
    uninstaller_done:
    ; the uninstaller does not delete itself when runned via ExecWait
    Delete "$PreUninstallerUninstallString"
    RMDir "$PreUninstallerPath"
    ; don't show the uninstall window if you hit "back"
    StrCpy $ShowPreviousInstallationDetected "no"
    uninstaller_canceled:
${EndIf}
FunctionEnd

SectionGroup /e "${PRODUCT_NAME}"

    Section "Application" PureData
        SectionIn RO
        !include "/tmp/install_files_list.nsh"
    SectionEnd

    Section "Create Startmenu entry" StartMenu
        ; ugly hack so the app shows up in "recently added"
        ; should have been one "SetShellVarContext $CONTEXT" but it refuses to compile
        ${If} $CONTEXT == "all"
            SetShellVarContext all
        ${Else}
            SetShellVarContext current
        ${EndIf}
        WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
        CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\"
        CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url" "" "%SYSTEMROOT%\system32\shell32.dll" 14
        CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\bin\${WISH_NAME}" '"$INSTDIR\tcl\pd-gui.tcl"' "$INSTDIR\bin\${PDEXE}" 0
    SectionEnd

    Section "Create Desktop Shortcut" DesktopShortcut
        CreateShortCut "$Desktop\${PRODUCT_NAME}.lnk" "$INSTDIR\bin\${WISH_NAME}" '"$INSTDIR\tcl\pd-gui.tcl"' "$INSTDIR\bin\${PDEXE}" 0
    SectionEnd

    Section "Open .pd-files with Pd" SetFileAssociations
        ; Set file ext associations
        WriteRegStr SHCTX "Software\Classes\.pd" "" "PureData"
        WriteRegStr SHCTX "Software\Classes\PureData" "" ""
        WriteRegStr SHCTX "Software\Classes\PureData\DefaultIcon" "" "$INSTDIR\bin\${PDEXE}"
        WriteRegStr SHCTX "Software\Classes\PureData\shell" "" ""
        WriteRegStr SHCTX "Software\Classes\PureData\shell\open" "" ""
        WriteRegStr SHCTX "Software\Classes\PureData\shell\open\command" "" '$INSTDIR\bin\${WISH_NAME} "$INSTDIR\tcl\pd-gui.tcl" "%1"'
        ; Set file ext icon
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd" "" ""
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\OpenWithList" "a" "${PDEXE}"
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\OpenWithList" "MRUList" ""
        WriteRegBin SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\OpenWithProgids" "PureData" "0"
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\UserChoice" "Progid" "PureData"
        ; Refresh Shell Icons
        Call RefreshShellIcons
    SectionEnd

    Section
        ; make variable $SHORTPATH
        Call PdGuiFromInstaller
    SectionEnd

SectionGroupEnd


Section -Post

    WriteUninstaller "$INSTDIR\uninst.exe"
    WriteRegStr SHCTX "Software\${PRODUCT_NAME}" "${ARCHI}" $INSTDIR
    ; These registry entries are necessary for the program to show up in the Add/Remove programs dialog
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "$(^Name)"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\uninst.exe"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayIcon" "$INSTDIR\bin\${PDEXE}"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "${PRODUCT_PUBLISHER}"

    ;WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "NoModify" 1
    ;WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "NoRepair" 1

    ${GetSize} "$INSTDIR" "/S=0M" $0 $1 $2
    IntOp $3 $0 * 1000
    IntFmt $4 "0x%08X" $3
    WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "EstimatedSize" "$4"
SectionEnd


Function un.onUninstSuccess
    HideWindow
    MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.sure
    MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
    Abort
FunctionEnd

Section Uninstall
    Call un.sure
    Delete "$INSTDIR\${PRODUCT_NAME}.url"
    Delete "$INSTDIR\uninst.exe"
    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

    Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}\Website.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
    RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}\"
    !include "/tmp/uninstall_files_list.nsh"

    ; file ext association
    ;DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"

    DeleteRegKey SHCTX "Software\${PRODUCT_NAME}"
    DeleteRegKey SHCTX "Software\Classes\.pd"
    DeleteRegKey SHCTX "Software\Classes\PureData"

    ; file ext icon
    DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd"
    DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

    ; Refresh Shell Icons
    Call un.RefreshShellIcons

    SetAutoClose true
SectionEnd
