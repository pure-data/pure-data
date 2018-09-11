#!/bin/sh

# script to call makensis (NSIS installer generator, adapted for mingw32)
# to make a Pd install/uninstall executable for MS windows.
# slightly adapted from a script by Roman Haefeli

cat > /dev/null << .

*** The README adapted from Roman: ***

The included build.sh is a shell script to create a proper Windows
installer (.exe) out of a Pure Data build tree.

Features of the resulting installer:

 * Install Pure Data to user selectable destination
   (Default: "%Program Files%\Pd")

 * Create Startmenu entries (optional)

 * Set proper file associations in Windows registry so that .pd files
   are opened with Pure Data and have proper icons in Explorer (optional)

 * Include uninstaller to cleanly wipe Pure Data from Windows (including
   Startmenu and registry entries)

Prerequisites for building the installer
----------------------------------------

* makensis, from the package mingw32-nsis

How to build installer
----------------------

Run the script and specify the directory containing a windows build of
Pure Data:

 ./build-msi.sh <path-to-pd-dir> <pd-version-number>

The scripts dynamically creates a list of all files to be included in the
installer and then runs makensis. The result is an installer named
pd-<VERSION>.windows-installer.exe.  Various parameters can be set in the
file pd.nsi.
.

# Set some defaults
SCRIPTDIR=${0%/*}
OUTDIR=$(mktemp -d || echo /tmp)
INSTALLFILE="${OUTDIR}/install_files_list.nsh"
UNINSTALLFILE="${OUTDIR}/uninstall_files_list.nsh"
LICENSEFILE="${OUTDIR}/license_page.nsh"
NSIFILE="${OUTDIR}/pd.nsi"

cleanup() {
 if [ "x${OUTDIR}" != "x/tmp" ]; then
    rm -rf "${OUTDIR}"
 fi
 exit $1
}

# show usage when invoked without args
if [ "$#" -lt "2" ]
then
  cat << EOF
Helper script to build a proper Windows installer out of a
Pd build tree.

Usage:
$0 <path_to_pd_build> <version>
EOF
  cleanup 1
fi

PDWINDIR=$(realpath "$1")
PDVERSION=$2

# Check validity of specified PDWINDIR
if ! [ -d "$PDWINDIR" ]
then
  echo "'$PDWINDIR' is not a directory"
  cleanup 1
elif ! [ -f "$PDWINDIR/bin/pd.exe" ]
then
  echo "Could not find bin/pd.exe. Is this a Windows build?"
  cleanup 1
fi

#### WRITE INSTALL LIST #########################################
find "$PDWINDIR" \
  -mindepth 0 \
  -type d \
  -printf 'SetOutPath "$INSTDIR/%P"\n' \
  -exec \
    find {} \
    -maxdepth 1 \
    -type f \
    -printf 'File "%p"\n' \; \
    -not -name "*.o" \
  | sed 's|/|\\|g' \
  > "${INSTALLFILE}"

#### WRITE UNINSTALL FILE LIST ###################################
# clear
> "${UNINSTALLFILE}"

# delete files
find "$PDWINDIR" -type f -printf 'Delete "$INSTDIR/%P"\n' \
  -not -name "*.o" \
  | sed 's|/|\\|g' \
  >> "${UNINSTALLFILE}"

# delete directories
# tac is used to reverse order so that dirs are removed in depth-first order
find "$PDWINDIR" -type d -printf 'RMDir "$INSTDIR/%P"\n' \
  | tac \
  | sed 's|/|\\|g' \
  >> "${UNINSTALLFILE}"

#### WRITE LICENSE INCLUDE ######################################
echo "!insertmacro MUI_PAGE_LICENSE \"$PDWINDIR/LICENSE.txt\"" \
  > "${LICENSEFILE}"

# stick version number in pd.nsi script
cat "${SCRIPTDIR}/pd.nsi" | sed \
	-e "s/PDVERSION/${PDVERSION}/" \
	-e "s|include \"/tmp/|include \"${OUTDIR}/|" \
	> "${NSIFILE}"

# check if we have nsis compiler
nsis_version=$(makensis -version)
nsis_exit=$?
case $nsis_exit in
  0) echo "Found NSIS version $nsis_version"
     ;;
  1) echo "makensis returned error. What's the problem?"
     cleanup 1
     ;;
  2) echo "NSIS is not found.  install, e.g., mingw32-nsis."
     cleanup 1
     ;;
  *) echo "Unkown error"
     cleanup 1
     ;;
esac

# run the build
if makensis "${NSIFILE}"
then
  echo "Build successful"
else
  echo "Some error occured during compilation of ${NSIFILE}"
  exit 1
fi
cleanup 0
