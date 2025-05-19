;
;
; https://stackoverflow.com/questions/36185539/can-i-get-nsis-to-make-a-single-installer-that-handles-local-deployment-and-syst
; ./build-nsi.sh -o "c:/tmp" "G:/gitportable/nsis2/pd-0.54-1" 0.54-1xx


;####################################################
;
; NOTE: registry settings auto change for the 32bit Pd on a 64bit OS:
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

Var VInstDirBase
Var VContext
Var Vshortpath
Var PreUninstallerDisplayName
Var PreUninstallerPath
Var PreUninstallerUninstallString
Var ShowPreviousInstallationDetected
Var PdProcess
Var JustForMeShow
Var VProductName
Var Vpdexe
Var Vwishexe
Var Vinstalldir
Var Vusername
Var Showaltinstall
Var Valtinstallbasename
Var Valtinstall
Var Vtempproductname
Var Vtempinstalldir

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "/tmp/${PD_FOLDER}-${PRODUCT_VERSION}.windows-installer.exe"
InstallDir ""

; Installer
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
; License page
!include "license_page.nsh"
Page Custom JustForMe JustForMeLeave
Page Custom Troubleshoot TroubleshootLeave
Page Custom PreviousInstallationDetected PreviousInstallationDetectedLeave
Page Custom AltInstall AltInstallLeave
!define MUI_COMPONENTSPAGE_NODESC
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
Page Custom WriteAccess WriteAccessLeave
!insertmacro MUI_PAGE_INSTFILES

; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\${WISH_NAME}"
!define MUI_FINISHPAGE_RUN_PARAMETERS " $Vshortpath\tcl\pd-gui.tcl"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\doc\1.manual\index.htm"
!insertmacro MUI_PAGE_FINISH

; Uninstaller
;!insertmacro MUI_UNPAGE_CONFIRM
UninstPage Custom un.Troubleshoot un.TroubleshootLeave
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; Take the highest execution level available
; This means that if it's possible to, we become an administrator
RequestExecutionLevel highest

!macro ONINIT un
    Function ${un}.onInit
        ReadRegStr $Vusername HKCU "Volatile Environment" "USERNAME"
        UserInfo::GetAccountType
        Pop $0
        ${If} $0 == "Admin"
            ; If we're an admin, default to installing to C:\Program Files
            ; The value of SetShellVarContext detetmines whether SHCTX is HKLM or HKCU
            SetShellVarContext all
            StrCpy $VContext "all"
            StrCpy $VInstDirBase "$PROGRAMFILES${ARCHI}"
            StrCpy $VProductName "${PRODUCT_NAME}"
            StrCpy $JustForMeShow "yes"
        ${Else}
            ; If we're just a user, default to installing to ~\AppData\Local
            SetShellVarContext current
            StrCpy $VContext "current"
            StrCpy $VInstDirBase "$LOCALAPPDATA"
            StrCpy $VProductName "${PRODUCT_NAME} ($Vusername)"
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
            Call ${un}.SetupStuff
        ${Endif}
    FunctionEnd

    Function ${un}.SetupStuff
        ReadRegStr $PreUninstallerPath SHCTX "Software\$VProductName" "${ARCHI}"
        ReadRegStr $PreUninstallerDisplayName SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "DisplayName"
        ReadRegStr $PreUninstallerUninstallString SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "UninstallString"
        ${If} $PreUninstallerPath != ""
            ; If we're already installed show the optional uninstall page:
            StrCpy $ShowPreviousInstallationDetected "yes"
        ${Endif}

        ${If} ${ARCHI} != "64"
            ${AndIf} $VInstDirBase == "$LOCALAPPDATA"
                ; use special folder if we are 32bit and on $LOCALAPPDATA
                StrCpy $INSTDIR "$VInstDirBase\${PD_FOLDER}${ARCHI}"
        ${Else}
            StrCpy $INSTDIR "$VInstDirBase\${PD_FOLDER}"
        ${Endif}
    FunctionEnd
!macroend

; Define the function twice, once for the installer and again for the uninstaller
!insertmacro ONINIT ""
!insertmacro ONINIT "un"

!macro macroA un
    Function ${un}RefreshShellIcons
        ; By jerome tremblay - april 2003
        System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v \
        (0x08000000, 0, 0, 0)'
    FunctionEnd

    Function ${un}DoCheckIfRunning
        ; perhaps all this can be done with an nsis plugin but we just use nsis as it comes from package managers.

        ReadEnvStr $0 COMSPEC
        nsExec::ExecToStack '"$0" /c tasklist | findstr /R /I /M /C:"\<$PdProcess\>"'
        Pop $1
        Pop $2
        ${If} $2 == ""
            goto good
        ${Else}
            IfSilent default
            MessageBox MB_ICONEXCLAMATION|MB_YESNO "Refusing to continue. Save your work and quit \
            any running ${PD_FOLDER} app before doing an (un)installation.$\r$\n$\r$\n\
            We found the following app open:$\r$\n$\r$\n\
            $2 $\r$\n$\r$\n\
            Force installation anyway?" IDYES good IDNO default
            default:
            quit
        ${EndIf}
        good:
    FunctionEnd

    Function ${un}CheckIfRunning
        StrCpy $PdProcess "${PD_FOLDER}.com"
        Call ${un}DoCheckIfRunning
        StrCpy $PdProcess "${PD_FOLDER}.exe"
        Call ${un}DoCheckIfRunning
    FunctionEnd

    Function ${un}AssociatePdFiles
        ; Set file ext associations
        WriteRegStr SHCTX "Software\Classes\.pd" "" "PureData"
        WriteRegStr SHCTX "Software\Classes\PureData" "" ""
        WriteRegStr SHCTX "Software\Classes\PureData\DefaultIcon" "" "$Vinstalldir\bin\$Vpdexe"
        WriteRegStr SHCTX "Software\Classes\PureData\shell" "" ""
        WriteRegStr SHCTX "Software\Classes\PureData\shell\open" "" ""
        WriteRegStr SHCTX "Software\Classes\PureData\shell\open\command" "" '"$Vinstalldir\bin\$Vwishexe" "$Vinstalldir\tcl\pd-gui.tcl" "%1"'
        ; Set file ext icon
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd" "" ""
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\OpenWithList" "a" "$Vpdexe"
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\OpenWithList" "MRUList" ""
        WriteRegBin SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\OpenWithProgids" "PureData" "0"
        WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd\UserChoice" "Progid" "PureData"
        ; Refresh Shell Icons
        Call ${un}RefreshShellIcons
    FunctionEnd

    Function ${un}Wipeprefs
        DeleteRegKey HKCU "Software\Pure-Data"
        HideWindow
        MessageBox MB_ICONINFORMATION|MB_OK "Pd preferences had been cleared"
        quit
    FunctionEnd
!macroend

!insertmacro macroA ""
!insertmacro macroA "un."

Function JustForMe
    ${If} $JustForMeShow == "yes"
        nsDialogs::Create 1018
        Pop $0
        !insertmacro MUI_HEADER_TEXT \
        "User selection" "This section is only aviable for admin users."
        ${NSD_CreateLabel} 0 0 100% 40% "Since you are admin you can \
        select to install for all users or just for you.$\r$\n$\r$\n\
        If you select 'Just me' your start menu folder will be named \
        '${PRODUCT_NAME} ($Vusername)'.$\r$\n$\r$\nAlso in the next \
        section there's a tool to set which app opens .pd files. \
        Select here the scope for it."
        ${NSD_CreateRadioButton} 0 40% 100% 10% "All users"
        Pop $1
        SendMessage $1 ${BM_CLICK} "" "" ; Must select a default
        ${NSD_CreateRadioButton} 0 50% 100% 15% "Just me"
        Pop $2
        nsDialogs::Show
    ${EndIf}
FunctionEnd

Function JustForMeLeave
    ${NSD_GetState} $2 $3
    ${If} $3 == ${BST_CHECKED}
        StrCpy $ShowPreviousInstallationDetected " "
        SetShellVarContext current
        StrCpy $VContext "current"
        StrCpy $VInstDirBase "$LOCALAPPDATA"
        StrCpy $VProductName "${PRODUCT_NAME} ($Vusername)"
        Call .SetupStuff
    ${Else}
    ; we have to do all this again if we change our minds later and came back to here.
        StrCpy $ShowPreviousInstallationDetected " "
        SetShellVarContext all
        StrCpy $VContext "all"
        StrCpy $VInstDirBase "$PROGRAMFILES${ARCHI}"
        StrCpy $VProductName "${PRODUCT_NAME}"
        Call .SetupStuff
    ${Endif}
FunctionEnd

Function CheckValidAppFolder
    StrCpy $Vpdexe " "
    StrCpy $Vwishexe " "
    nsDialogs::SelectFolderDialog "Select the Pd(64) app folder to open .pd \
    files.$\r$\nNormally is 'C:\Program files\Pd'" "c:\"
    Pop $0
    StrCpy $Vinstalldir "$0"
    IfFileExists $0\bin\pd.exe 0 +2
    StrCpy $Vpdexe "pd.exe"
    IfFileExists $0\bin\pd64.exe 0 +2
    StrCpy $Vpdexe "pd64.exe"
    IfFileExists $0\bin\wish85.exe 0 +2
    StrCpy $Vwishexe "wish85.exe"
    IfFileExists $0\bin\wish86.exe 0 +2
    StrCpy $Vwishexe "wish86.exe"
FunctionEnd

Function DefaultToOpen
    loop:
    Call CheckValidAppFolder
    ${If} $Vpdexe != " "
        ${AndIf} $Vwishexe != " "
            HideWindow
            Call AssociatePdFiles
            MessageBox MB_ICONINFORMATION|MB_OK "Successfully set Pd(64) app \
            at '$Vinstalldir' to open .pd files."
            quit
        ${Else}
            MessageBox MB_ICONINFORMATION|MB_RETRYCANCEL "Not a valid Pd \
            or Pd64 app folder. Select a valid one." IDRETRY loop IDCANCEL bye
            goto loop
    ${Endif}
    bye:
    quit
FunctionEnd

Function Troubleshoot
    nsDialogs::Create 1018
    Pop $0
    !insertmacro MUI_HEADER_TEXT \
    "Troubleshoot existing installation" "Preferences repair tools."
    ${NSD_CreateLabel} 0 0 100% 40% "Sometimes your pre-existing Pd settings \
    (if there are any) can go wrong and the app might not be able to load. You \
    don't have to use this tool unless the app is failing to start. \
    $\r$\n$\r$\nAlso there's a tool to set which app is the default to \
    open .pd files."

    ${NSD_CreateRadioButton} 0 40% 100% 10% "Continue (normal installation)"
    Pop $1
    SendMessage $1 ${BM_CLICK} "" "" ; Must select a default
    ${NSD_CreateRadioButton} 0 55% 100% 10% "Clear Pd preferences and exit."
    Pop $2
    ${NSD_CreateRadioButton} 0 70% 100% 10% "Repair which Pd(64) defaults to \
    open .pd files and exit"
    Pop $3
    nsDialogs::Show
FunctionEnd

Function TroubleshootLeave
    ${NSD_GetState} $2 $4
    ${If} $4 == ${BST_CHECKED}
        Call Wipeprefs
    ${EndIf}
    ${NSD_GetState} $3 $4
    ${If} $4 == ${BST_CHECKED}
        Call DefaultToOpen
    ${EndIf}
FunctionEnd

Function PreviousInstallationDetected
    ; do an early warning if we find Pd is running
    Call CheckIfRunning
    ${If} $ShowPreviousInstallationDetected == "yes"
        nsDialogs::Create 1018
        Pop $0
        !insertmacro MUI_HEADER_TEXT \
        "Previous Installation" "Select if you are going to uninstall \
        previous installation"
        ${NSD_CreateLabel} 0 0 100% 40% "System tells there is already a \
        Pd installation:$\r$\n$\r$\n \
        '$PreUninstallerDisplayName' on '$PreUninstallerPath' $\r$\n$\r$\n\
        If you are going to upgrade or downgrade it is recommended that you run \
        the uninstaller. If not, install as an alternate app or proceed the \
        installation to a different location"
        ${NSD_CreateRadioButton} 0 40% 50% 10% "Run the uninstaller (recommended)"
        Pop $1
        SendMessage $1 ${BM_CLICK} "" "" ; Must select a default
        ${NSD_CreateRadioButton} 0 55% 50% 10% "Install as an alternate app"
        Pop $2
        ${NSD_CreateRadioButton} 0 70% 50% 10% "Continue"
        Pop $3
        nsDialogs::Show
    ${EndIf}
FunctionEnd

Function DoUninstall
    ExecWait '"$PreUninstallerUninstallString" /S _?=$PreUninstallerPath'
    Delete "$PreUninstallerUninstallString"
    RMDir "$PreUninstallerPath"
    ; don't show the uninstall window if you hit "back"
    StrCpy $ShowPreviousInstallationDetected "no"
FunctionEnd

Function AltInstall
    ${If} $Showaltinstall == "yes"
        nsDialogs::Create 1018
        Pop $0
        !insertmacro MUI_HEADER_TEXT \
        "Alternate installation" "Make an 'alternate' installation."
        ${NSD_CreateLabel} 0 0 100% 40% "This installation will be an \
        alternate install.$\r$\n$\r$\nThe start menu folder, desktop \
        icon and uninstaller will be named with the following string. \
        You can change it but is recommnded that you keep it short."
        StrCpy $1 "${PD_FOLDER}-${PDVER}-(alt)"
        ${NSD_CreateText} 0 40% 50% 10% "$1"
        Pop $Valtinstallbasename
        nsDialogs::Show
    ${EndIf}
FunctionEnd

Function AltInstallLeave
    ${If} $Showaltinstall == "yes"
        ${NSD_GetText} $Valtinstallbasename $0
        StrCpy $Vtempproductname "$VProductName"
        StrCpy $Vtempinstalldir "$INSTDIR"
        StrCpy $VProductName "$0"
        StrCpy $INSTDIR "$VInstDirBase\$VProductName"
        StrCpy $Valtinstall "yes"
    ${EndIf}
FunctionEnd

Function UndoAltInstall
    ${If} $Valtinstall == "yes"
        StrCpy $VProductName "$Vtempproductname"
        StrCpy $INSTDIR "$Vtempinstalldir"
        StrCpy $Valtinstall "no"
    ${EndIf}
    StrCpy $Showaltinstall "no"
FunctionEnd

Function PreviousInstallationDetectedLeave
    ${NSD_GetState} $1 $4
    ${If} $4 == ${BST_CHECKED}
        Call UndoAltInstall
        Call DoUninstall
    ${EndIf}
    ${NSD_GetState} $2 $4
    ${If} $4 == ${BST_CHECKED}
        StrCpy $Showaltinstall "yes"
    ${EndIf}
    ${NSD_GetState} $3 $4
    ${If} $4 == ${BST_CHECKED}
        Call UndoAltInstall
    ${EndIf}
FunctionEnd

;https://nsis.sourceforge.io/Go_to_a_NSIS_page
Function RelGotoPage
    IntCmp $R9 0 0 Move Move
    StrCmp $R9 "X" 0 Move
    StrCpy $R9 "120"

    Move:
    SendMessage $HWNDPARENT "0x408" "$R9" ""
FunctionEnd

Function WriteAccess
    IfSilent skip
    StrCpy $0 " "
    CreateDirectory "$INSTDIR\chk"
    WriteINIStr "$INSTDIR\chk\writecheck.ini" section1 check "yes"
    IfFileExists "$INSTDIR\chk\writecheck.ini" 0 +2
    StrCpy $0 "yes"
    ${If} $0 == "yes"
        Delete "$INSTDIR\chk\writecheck.ini"
        RmDir "$INSTDIR\chk\"
        return
    ${Else}
        MessageBox MB_ICONINFORMATION|MB_RETRYCANCEL "We don't have \
        permissions to write to folder: '$INSTDIR'. Please select a writtable \
        folder." IDRETRY loop IDCANCEL bye
        goto loop
    ${Endif}
    bye:
    quit
    loop:
    StrCpy $R9 -1 ;Relative page number.
    Call RelGotoPage
    skip:
FunctionEnd

Function WriteAccessLeave
FunctionEnd

SectionGroup /e "${PRODUCT_NAME}"
    Section "Application" PureData
        ; When on silent installs custom pages are skipped but we need to \
        ; ckeck if Pd is running and uninstall previous installation.
        IfSilent x_silent x_nosilent
        x_silent:
        Call CheckIfRunning
        ${If} $ShowPreviousInstallationDetected == "yes"
            Call DoUninstall
        ${EndIf}
        x_nosilent:
        SectionIn RO
        !include "install_files_list.nsh"
        ; write ini file
        WriteINIStr "$INSTDIR\uninst.ini" section1 vname "$VProductName"
        WriteINIStr "$INSTDIR\uninst.ini" section1 shellcontext "$VContext"
    SectionEnd

    Section "Create Startmenu entry" StartMenu
        WriteINIStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
        WriteINIStr "$INSTDIR\uninst.ini" section1 uninstallstartmenu "yes"
        ; ugly hack so the app shows up in "recently added"
        ; should have been one "SetShellVarContext $VContext" but it refuses to compile
        ${If} $VContext == "all"
            SetShellVarContext all
        ${Else}
            SetShellVarContext current
        ${EndIf}
        CreateDirectory "$SMPROGRAMS\$VProductName\"
        CreateShortCut "$SMPROGRAMS\$VProductName\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url" "" "%SYSTEMROOT%\system32\shell32.dll" 14
        CreateShortCut "$SMPROGRAMS\$VProductName\$VProductName.lnk" "$INSTDIR\bin\${WISH_NAME}" '"$INSTDIR\tcl\pd-gui.tcl"' "$INSTDIR\bin\${PDEXE}" 0
    SectionEnd

    Section "Create Desktop Shortcut" DesktopShortcut
        CreateShortCut "$Desktop\$VProductName.lnk" "$INSTDIR\bin\${WISH_NAME}" '"$INSTDIR\tcl\pd-gui.tcl"' "$INSTDIR\bin\${PDEXE}" 0
        WriteINIStr "$INSTDIR\uninst.ini" section1 uninstalldesktopshortcut "yes"
    SectionEnd

    Section "Open .pd-files with Pd" SetFileAssociations
        StrCpy $Vpdexe "${PDEXE}"
        StrCpy $Vwishexe "${WISH_NAME}"
        StrCpy $Vinstalldir "$INSTDIR"
        Call AssociatePdFiles
        WriteINIStr "$INSTDIR\uninst.ini" section1 uninstallfileassociation "yes"
    SectionEnd

    Section
        ; make variable $Vshortpath
        GetFullPathName /SHORT $Vshortpath $INSTDIR
    SectionEnd
SectionGroupEnd


Section -Post
    WriteUninstaller "$INSTDIR\uninst.exe"
    ${If} $Valtinstall != "yes"
        WriteRegStr SHCTX "Software\$VProductName" "${ARCHI}" $INSTDIR
    ${EndIf}
    ; These registry entries are necessary for the program to show up in the Add/Remove programs dialog
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "DisplayName" "$VProductName ${PRODUCT_VERSION}"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "UninstallString" "$INSTDIR\uninst.exe"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "ModifyPath" "$INSTDIR\uninst.exe"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "DisplayIcon" "$INSTDIR\bin\${PDEXE}"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "Publisher" "${PRODUCT_PUBLISHER}"

    ;WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "NoModify" 1
    ;WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "NoRepair" 1

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName" "EstimatedSize" "$0"
SectionEnd


Function un.onUninstSuccess
    IfSilent skip
    HideWindow
    MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed \
    from your computer."
    skip:
    quit
FunctionEnd

Function un.sure
    IfSilent skip
    MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you \
    want to completely remove $(^Name) and all of its components?" IDYES +2
    Abort
    skip:
FunctionEnd

Function un.dirty
    IfSilent skip
    HideWindow
    MessageBox MB_ICONINFORMATION|MB_OK "We could not find '$INSTDIR\uninst.ini' \
    so we could not remove the startup program folder and other Windows registry \
    stuff. We could only delete '$INSTDIR'."
    skip:
FunctionEnd

Function un.deletefiles
    !include "uninstall_files_list.nsh"
    Delete "$INSTDIR\${PRODUCT_NAME}.url"
    Delete "$INSTDIR\uninst.ini"
    Delete "$INSTDIR\uninst.exe"
    RmDir "$INSTDIR"
FunctionEnd

Function un.Troubleshoot
    nsDialogs::Create 1018
    Pop $0
    !insertmacro MUI_HEADER_TEXT \
    "Troubleshoot existing installation" "Preferences repair tools."
    ${NSD_CreateLabel} 0 0 100% 40% "Sometimes your ${PD_FOLDER} \
    preferences can go wrong and the app might not be able to load. \
    $\r$\n$\r$\nAlso there's a tool to repair .pd files associations \
    to open with this ${PD_FOLDER} installation."

    ${NSD_CreateRadioButton} 0 40% 100% 10% "Uninstall the app"
    Pop $1
    SendMessage $1 ${BM_CLICK} "" "" ; Must select a default
    ${NSD_CreateRadioButton} 0 55% 100% 10% "Clear Pd preferences and exit."
    Pop $2
    ${NSD_CreateRadioButton} 0 70% 100% 10% "Repair .pd files associations \
    and exit"
    Pop $3
    GetDlgItem $R0 $HWNDPARENT 1
    SendMessage $R0 ${WM_SETTEXT} 0 "STR:Next"
    nsDialogs::Show
FunctionEnd

Function un.TroubleshootLeave
    ${NSD_GetState} $2 $4
    ${If} $4 == ${BST_CHECKED}
        Call un.Wipeprefs
    ${EndIf}
    ${NSD_GetState} $3 $4
    ${If} $4 == ${BST_CHECKED}
        ReadINIStr $0 "$INSTDIR\uninst.ini" section1 shellcontext
        ${If} "$0" != "all"
            SetShellVarContext current
        ${EndIf}
        StrCpy $Vpdexe "${PDEXE}"
        StrCpy $Vwishexe "${WISH_NAME}"
        StrCpy $Vinstalldir "$INSTDIR"
        Call un.AssociatePdFiles
        MessageBox MB_ICONINFORMATION|MB_OK "Successfully set the app \
            at '$Vinstalldir' to open .pd files."
        quit
    ${EndIf}
FunctionEnd

Section Uninstall

    ; let the uninstaller quit if Pd is running
    Call un.CheckIfRunning
    IfSilent skip
    Call un.sure
    skip:
    StrCpy $0 " "
    IfFileExists "$INSTDIR\uninst.ini" 0 +2
    StrCpy $0 "yes"
    ${If} $0 == "yes"
        ; read ini file
        ReadINIStr $VProductName "$INSTDIR\uninst.ini" section1 vname
        ReadINIStr $0 "$INSTDIR\uninst.ini" section1 shellcontext
        ${If} "$0" != "all"
            SetShellVarContext current
        ${EndIf}
        ReadINIStr $0 "$INSTDIR\uninst.ini" section1 uninstallstartmenu
        ${If} "$0" == "yes"
            Delete "$SMPROGRAMS\$VProductName\Website.lnk"
            Delete "$SMPROGRAMS\$VProductName\$VProductName.lnk"
            RMDir "$SMPROGRAMS\$VProductName"
        ${EndIf}
        ReadINIStr $0 "$INSTDIR\uninst.ini" section1 uninstalldesktopshortcut
        ${If} "$0" == "yes"
            Delete "$DESKTOP\$VProductName.lnk"
        ${EndIf}
        ReadINIStr $0 "$INSTDIR\uninst.ini" section1 uninstallfileassociation
        ${If} "$0" == "yes"
            DeleteRegKey SHCTX "Software\Classes\.pd"
            DeleteRegKey SHCTX "Software\Classes\PureData"
            ; file ext icon
            DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pd"
        ${EndIf}
        Call un.deletefiles
        Call un.RefreshShellIcons
        DeleteRegKey SHCTX "Software\$VProductName"
        DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\$VProductName"
        SetAutoClose true
    ${Else}
        ; no ini file. dirty uninstall
        Call un.deletefiles
        Call un.dirty
        Call un.RefreshShellIcons
        SetAutoClose true
    ${EndIf}
SectionEnd
